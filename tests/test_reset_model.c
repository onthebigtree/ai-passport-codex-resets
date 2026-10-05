#include "reset_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char *read_file(const char *path,size_t *length) {
    FILE *f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    char *s=malloc((size_t)n+1); assert(s); assert(fread(s,1,(size_t)n,f)==(size_t)n); fclose(f); s[n]=0; *length=(size_t)n; return s;
}
int main(void) {
    int64_t start=reset_parse_iso("2026-10-05T07:00:00.000Z");
    assert(start==1791183600LL);
    assert(reset_parse_iso("2026-02-29")==-1);
    assert(reset_parse_iso("2026-13-01")==-1);
    assert(reset_parse_iso("2026-10-05T24:00:00Z")==-1);
    assert(reset_parse_iso("2026-10-05T12:00:00+08:00")==-1);
    assert(reset_pacific_offset(reset_parse_iso("2026-11-01T08:59:59Z"))==-25200);
    assert(reset_pacific_offset(reset_parse_iso("2026-11-01T09:00:00Z"))==-28800);
    assert(reset_pacific_offset(reset_parse_iso("2026-03-08T09:59:59Z"))==-28800);
    assert(reset_pacific_offset(reset_parse_iso("2026-03-08T10:00:00Z"))==-25200);
    size_t len; char *html=read_file("tests/fixtures/codex-challenge.html",&len);
    for(size_t chunk=1;chunk<=1024;chunk=chunk*2+1) {
        challenge_parser_t parser; challenge_parser_init(&parser); challenge_t out={0};
        for(size_t i=0;i<len;i+=chunk) challenge_parser_feed(&parser,html+i,len-i<chunk?len-i:chunk);
        assert(challenge_parser_finish(&parser,&out)); assert(out.days[0]==DAY_OPEN);
        for(int i=1;i<28;++i) assert(out.days[i]==DAY_UPCOMING);
        assert(reset_challenge_day(&out,start-1)==0); assert(reset_challenge_day(&out,start)==1);
        assert(reset_challenge_day(&out,reset_parse_iso("2026-11-02T07:59:59Z"))==28);
        assert(reset_challenge_day(&out,reset_parse_iso("2026-11-02T08:00:00Z"))==29);
        assert(reset_challenge_day(&out,0)==0);
    }
    challenge_parser_t p; challenge_t c={0}; challenge_parser_init(&p);
    challenge_parser_feed(&p,html,len/2); assert(!challenge_parser_finish(&p,&c));
    challenge_parser_init(&p); challenge_parser_feed(&p,html,len);
    const char *extra="<li class=\"challenge-cell challenge-state--open\">";
    challenge_parser_feed(&p,extra,strlen(extra)); assert(!challenge_parser_finish(&p,&c));
    char *bad=malloc(len+1); memcpy(bad,html,len+1); char *state=strstr(bad,"--open"); assert(state); memcpy(state,"--nope",6);
    challenge_parser_init(&p); challenge_parser_feed(&p,bad,len); assert(!challenge_parser_finish(&p,&c)); free(bad);free(html);
    reset_nav_t nav={0}; reset_navigation(&nav,KEY_DOWN);assert(nav.page==1);
    reset_navigation(&nav,KEY_OK);assert(nav.detail); reset_navigation(&nav,KEY_UP);assert(nav.selected_day==27);
    reset_navigation(&nav,KEY_DOWN);assert(nav.selected_day==0);reset_navigation(&nav,KEY_BACK);assert(!nav.detail);
    reset_navigation(&nav,KEY_UP);assert(nav.page==0);reset_navigation(&nav,KEY_OK);reset_navigation(&nav,KEY_UP);assert(nav.scroll==0);
    for(int i=0;i<100;++i) {
        reset_navigation(&nav,KEY_DOWN);
    }
    assert(nav.scroll==24);
    char text[7]; reset_copy_text(text,sizeof(text),"abc世界z");assert(!strcmp(text,"abc??z"));
    reset_copy_text(text,1,"abc");assert(text[0]==0);assert(!reset_font_codepoint(0x9f98));
    puts("Reset model: PASS (stream boundaries, invalid markup, DST, dates, navigation, UTF-8)");
}
