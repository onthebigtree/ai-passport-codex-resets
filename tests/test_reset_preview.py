import importlib.util
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parent.parent
spec=importlib.util.spec_from_file_location('preview_server',ROOT/'preview/server.py')
server=importlib.util.module_from_spec(spec);spec.loader.exec_module(server)
class PreviewTest(unittest.TestCase):
    def test_sources(self):
        data=server.parse_status(json.loads((ROOT/'tests/fixtures/codex-status.json').read_text()))
        self.assertEqual(data['total'],57)
        p=server.ChallengeParser();p.feed((ROOT/'tests/fixtures/codex-challenge.html').read_text())
        c=p.result();self.assertEqual(c['start'],'2026-10-05');self.assertEqual(c['days'],['open']+['upcoming']*27)
    def test_markup_drift(self):
        p=server.ChallengeParser();p.feed('<html>rate limited</html>')
        with self.assertRaises(ValueError):p.result()
    def test_partial_failure_keeps_cache(self):
        store=server.Store();old={'sentinel':True};store.data={'status':old,'challenge':None}
        original=server.fetch
        def failing(kind):raise TimeoutError()
        server.fetch=failing
        try:
            import tempfile
            with tempfile.TemporaryDirectory() as tmp:
                store.cache=Path(tmp)/'cache.json';out=store.get(True)
            self.assertIs(out['data']['status'],old);self.assertEqual(set(out['errors']),{'status','challenge'})
        finally:server.fetch=original
    def test_invalid_numbers(self):
        for val in [-1,1.2,True]:
            data=json.loads((ROOT/'tests/fixtures/codex-status.json').read_text());data['data']['stats']['total']=val
            with self.assertRaises(ValueError):server.parse_status(data)
if __name__=='__main__':unittest.main()
