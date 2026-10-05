#include "reset_model.h"
#include <stdio.h>
#include <string.h>
/* Gregorian days since 1970-01-01 (civil calendar, independent of host TZ). */
int64_t reset_civil_day(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    return era * 146097LL + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}
int64_t reset_parse_iso(const char *s) {
    int y,m,d,h=0,n=0,sec=0;
    if (!s || strlen(s)<10 || sscanf(s,"%4d-%2d-%2d",&y,&m,&d)!=3 || y<2020 || y>2099 || m<1 || m>12) return -1;
    const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int max = lengths[m-1] + (m==2 && y%4==0);
    if (d<1 || d>max) return -1;
    if (strlen(s)>10) {
        if (sscanf(s+10,"T%2d:%2d:%2d",&h,&n,&sec)!=3 || h<0 || h>23 || n<0 || n>59 || sec<0 || sec>59 || s[strlen(s)-1]!='Z') return -1;
    }
    return reset_civil_day(y,(unsigned)m,(unsigned)d)*86400 + h*3600 + n*60 + sec;
}
int reset_pacific_offset(int64_t now) {
    int y=2020;
    while (y<2100 && reset_civil_day(y+1,1,1)*86400<=now) ++y;
    int64_t march=reset_civil_day(y,3,1), nov=reset_civil_day(y,11,1);
    /* US DST: second Sunday in March 10:00 UTC, first Sunday in November 09:00 UTC. */
    int64_t start=(march+(7-(march+4)%7)%7+7)*86400+36000;
    int64_t end=(nov+(7-(nov+4)%7)%7)*86400+32400;
    return now>=start && now<end ? -7*3600 : -8*3600;
}
int reset_challenge_day(const challenge_t *c, int64_t now) {
    if (!c->valid || now<=0) return 0;
    int64_t day=(now+reset_pacific_offset(now))/86400-c->start_day+1;
    return day<1 ? 0 : day>RESET_DAYS ? RESET_DAYS+1 : (int)day;
}
void reset_navigation(reset_nav_t *n, reset_key_t key) {
    if (key==KEY_BACK) { n->detail=false; n->scroll=0; return; }
    if (key==KEY_OK) { n->detail=!n->detail; n->scroll=0; return; }
    if (!n->detail) { n->page=1-n->page; return; }
    if (n->page==1) n->selected_day=(n->selected_day+(key==KEY_DOWN ? 1 : RESET_DAYS-1))%RESET_DAYS;
    else if (key==KEY_UP && n->scroll>0) --n->scroll;
    else if (key==KEY_DOWN && n->scroll<24) ++n->scroll;
}
static bool has_class(const char *tag, const char *name) {
    const char *p=strstr(tag,"class=\"");
    if (!p) return false;
    p+=7;
    const char *end=strchr(p,'"');
    if (!end) return false;
    size_t n=strlen(name);
    while (p<end) {
        while (*p==' ') ++p;
        const char *space=memchr(p,' ',(size_t)(end-p));
        const char *next=space ? space : end;
        if ((size_t)(next-p)==n && !strncmp(p,name,n)) return true;
        p=next+1;
    }
    return false;
}
static void parse_tag(challenge_parser_t *p) {
    const char *start=strstr(p->tag,"data-start=\"");
    if (start && strstr(p->tag,"data-challenge-clock")) {
        char date[11]={0};
        if (strlen(start+12)>=10) memcpy(date,start+12,10);
        int64_t t=reset_parse_iso(date);
        if (t>0) { p->value.start_day=t/86400; p->saw_start=true; }
    }
    if (strncmp(p->tag,"<li ",4) || !has_class(p->tag,"challenge-cell")) return;
    if (p->count>=RESET_DAYS) { p->invalid=true; return; }
    day_state_t state=DAY_UNKNOWN;
    bool improved=has_class(p->tag,"challenge-state--improvement"), reset=has_class(p->tag,"challenge-state--reset");
    if (improved && reset) state=DAY_BOTH;
    else if (improved) state=DAY_IMPROVEMENT;
    else if (reset) state=DAY_RESET;
    else if (has_class(p->tag,"challenge-state--upcoming")) state=DAY_UPCOMING;
    else if (has_class(p->tag,"challenge-state--open")) state=DAY_OPEN;
    else if (has_class(p->tag,"challenge-state--checking")) state=DAY_CHECKING;
    else if (has_class(p->tag,"challenge-state--closed")) state=DAY_CLOSED;
    if (state==DAY_UNKNOWN) p->invalid=true;
    p->value.days[p->count++]=state;
}
void challenge_parser_init(challenge_parser_t *p) { memset(p,0,sizeof(*p)); }
void challenge_parser_feed(challenge_parser_t *p, const char *data, size_t length) {
    for (size_t i=0;i<length;++i) {
        char c=data[i];
        if (c=='<') { p->in_tag=true; p->length=0; p->overflow=false; }
        if (!p->in_tag) continue;
        if (p->length+1<sizeof(p->tag)) p->tag[p->length++]=c;
        else p->overflow=true;
        if (c=='>') {
            p->tag[p->length]=0;
            if (!p->overflow) parse_tag(p);
            else if (strstr(p->tag,"challenge-cell")) p->invalid=true;
            p->in_tag=false;
        }
    }
}
bool challenge_parser_finish(challenge_parser_t *p, challenge_t *out) {
    if (p->invalid || p->count!=RESET_DAYS || !p->saw_start) return false;
    p->value.valid=true; *out=p->value; return true;
}
const char *reset_day_label(day_state_t s) {
    const char *labels[]={"待确认","未开始","等待 Tibo","产品改进","额度重置","改进和重置","核实中","当日已结束"};
    return labels[s<=DAY_CLOSED ? s : DAY_UNKNOWN];
}
/* Network announcements use the API's English original. Unsupported glyphs
 * are visibly replaced, never silently dropped or read past a UTF-8 boundary. */
bool reset_font_codepoint(uint32_t cp) { return cp>=32 && cp<=126; }
void reset_copy_text(char *dst, size_t cap, const char *src) {
    size_t n=0;
    if (!cap) return;
    if (src) for (size_t i=0;src[i] && n+1<cap;) {
        unsigned char c=(unsigned char)src[i++];
        if (c<128) dst[n++]=(c=='\n' || reset_font_codepoint(c)) ? (char)c : ' ';
        else { while (((unsigned char)src[i]&0xc0)==0x80) ++i; dst[n++]='?'; }
    }
    dst[n]=0;
}
