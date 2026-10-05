#!/usr/bin/env python3
"""Local read-only preview server. Firmware connects directly to upstream."""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from html.parser import HTMLParser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
from pathlib import Path
import threading
import time
from urllib.parse import urlsplit
from urllib.request import Request, urlopen
ROOT=Path(__file__).resolve().parent
SOURCE='https://codex-resets.com'
STATES={'upcoming','open','improvement','reset','checking','closed'}
class ChallengeParser(HTMLParser):
    def __init__(self):
        super().__init__(); self.days=[]; self.start=None
    def handle_starttag(self,tag,attrs):
        a=dict(attrs); classes=set(a.get('class','').split())
        if 'data-challenge-clock' in a and a.get('data-start'):
            self.start=datetime.strptime(a['data-start'],'%Y-%m-%d').date().isoformat()
        if tag=='li' and 'challenge-cell' in classes:
            states={c.removeprefix('challenge-state--') for c in classes if c.startswith('challenge-state--')} & STATES
            if states=={'improvement','reset'}: state='both'
            elif len(states)==1: state=next(iter(states))
            else: raise ValueError('Unknown challenge state')
            self.days.append(state)
    def result(self):
        if len(self.days)!=28 or not self.start: raise ValueError('Challenge markup changed')
        return {'start':self.start,'days':self.days}
def parse_status(raw):
    d=raw['data']; latest=d['latest_reset']; stats=d['stats']
    stamp=latest['announced_at']; datetime.fromisoformat(stamp.replace('Z','+00:00'))
    if latest['reset_type'] not in ('regular','banked'): raise ValueError('Unknown reset type')
    total=stats['total']; avg=stats['avg_interval_days']
    if isinstance(total,bool) or not isinstance(total,int) or not 1<=total<=1000000: raise ValueError('Invalid total')
    if not isinstance(avg,(float,int)) or not math.isfinite(avg) or not 0<=avg<10000: raise ValueError('Invalid average')
    return {'at':stamp,'total':total,'average':avg,'type':latest['reset_type'],'text':str(latest['text'])[:511],
            'scheduled':bool(d.get('scheduled_reset'))}
def fetch(kind):
    path='/api/v1/status' if kind=='status' else '/zh-CN/tibo-28'
    req=Request(SOURCE+path,headers={'User-Agent':'AI-Passport-Codex-Resets/1.0','Accept-Encoding':'identity'})
    with urlopen(req,timeout=15) as r:
        limit=12288 if kind=='status' else 262144
        body=r.read(limit+1)
        if len(body)>limit: raise ValueError('Response too large')
    if kind=='status': result=parse_status(json.loads(body))
    else:
        parser=ChallengeParser(); parser.feed(body.decode('utf-8')); result=parser.result()
    result['fetchedAt']=datetime.now(timezone.utc).isoformat()
    return result
class Store:
    def __init__(self):
        self.lock=threading.Lock(); self.checked=0.; self.data={'status':None,'challenge':None}; self.errors={}
        self.cache=ROOT/'.cache/snapshot.json'
        if self.cache.exists():
            try:
                cached=json.loads(self.cache.read_text())
                if cached.get('version')==1: self.data=cached['data']
            except (OSError,ValueError,KeyError): pass
    def get(self,force=False):
        with self.lock:
            if time.monotonic()-self.checked> (15 if force else 300):
                with ThreadPoolExecutor(max_workers=2) as pool:
                    jobs={k:pool.submit(fetch,k) for k in self.data}
                    for k,job in jobs.items():
                        try: self.data[k]=job.result(); self.errors.pop(k,None)
                        except Exception as e: self.errors[k]=type(e).__name__
                self.checked=time.monotonic()
                self.cache.parent.mkdir(exist_ok=True)
                temp=self.cache.with_suffix('.tmp'); temp.write_text(json.dumps({'version':1,'data':self.data})); temp.replace(self.cache)
            return {'data':self.data,'errors':dict(self.errors),'serverNow':datetime.now(timezone.utc).isoformat(),'source':SOURCE}
STORE=Store()
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed=urlsplit(self.path)
        if parsed.path=='/api/snapshot':
            self.send_payload(json.dumps(STORE.get('refresh=1' in parsed.query),ensure_ascii=False).encode(),'application/json; charset=utf-8'); return
        files={'/':'index.html','/app.js':'app.js','/style.css':'style.css','/passport.woff2':'passport.woff2'}
        name=files.get(parsed.path)
        if not name: self.send_error(404); return
        mime={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.woff2':'font/woff2'}
        p=ROOT/name
        if not p.exists(): self.send_error(404); return
        self.send_payload(p.read_bytes(),mime[p.suffix])
    def send_payload(self,data,mime):
        self.send_response(200); self.send_header('Content-Type',mime); self.send_header('Content-Length',str(len(data)))
        self.send_header('Cache-Control','no-store'); self.send_header('X-Content-Type-Options','nosniff'); self.end_headers(); self.wfile.write(data)
    def log_message(self,fmt,*args): print(fmt%args,flush=True)
if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--port',type=int,default=8765); args=parser.parse_args()
    print(f'AI Passport preview: http://127.0.0.1:{args.port}',flush=True)
    ThreadingHTTPServer(('127.0.0.1',args.port),Handler).serve_forever()
