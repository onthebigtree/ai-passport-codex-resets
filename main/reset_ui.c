#include "reset_ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <time.h>
LV_FONT_DECLARE(reset_font_14);
#define CREAM 0xFFF4DD
#define PAPER 0xFFFCF4
#define INK 0x25221D
#define MUTED 0x796F60
#define RED 0xF56A48
#define YELLOW 0xFFDC69
#define GREEN 0xB2CF9B
#define LINE 0xD8CBB4
static reset_nav_t *s_nav;
static lv_obj_t *s_screen;
static int s_battery;
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int border,int radius) {
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(o,lv_color_hex(INK),0); lv_obj_set_style_border_width(o,border,0);
    lv_obj_set_style_radius(o,radius,0); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *text,const lv_font_t *font,uint32_t color) {
    lv_obj_t *o=lv_label_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_text(o,text); return o;
}
static void small(lv_obj_t *p,int x,int y,int w,const char *s,uint32_t color) { (void)label(p,x,y,w,s,&reset_font_14,color); }
static void date_text(char *out,size_t size,int64_t stamp,int offset,const char *format) {
    if(stamp<=0) { snprintf(out,size,"--"); return; }
    time_t t=(time_t)(stamp+offset); struct tm tm; gmtime_r(&t,&tm); strftime(out,size,format,&tm);
}
static uint32_t day_color(day_state_t s) {
    if(s==DAY_IMPROVEMENT) return GREEN;
    if(s==DAY_RESET || s==DAY_BOTH) return RED;
    if(s==DAY_OPEN || s==DAY_CHECKING) return YELLOW;
    if(s==DAY_CLOSED) return LINE;
    return PAPER;
}
static void header(const reset_snapshot_t *v) {
    const char *state=v->setup ? "配网" : v->network==NET_ONLINE ? "已同步" : v->network==NET_SYNCING ? "同步中" : v->network==NET_ERROR ? "更新失败" : v->network==NET_CONNECTING ? "连接中" : "离线";
    box(s_screen,20,18,5,5,v->network==NET_ONLINE ? 0x608457 : RED,0,3);
    small(s_screen,31,10,106,state,MUTED);
    char battery[16]; if(s_battery<0) snprintf(battery,sizeof(battery),"--%%"); else snprintf(battery,sizeof(battery),"%d%%",s_battery);
    lv_obj_t *b=label(s_screen,175,10,44,battery,&reset_font_14,MUTED); lv_obj_set_style_text_align(b,LV_TEXT_ALIGN_RIGHT,0);
}
static void footer(const reset_snapshot_t *v,const char *hint) {
    char stamp[40],date[20];
    int64_t fetched=s_nav->page ? v->challenge.fetched_at : v->status.fetched_at;
    date_text(date,sizeof(date),fetched,8*3600,"%m/%d %H:%M");
    snprintf(stamp,sizeof(stamp),"更新 %s CST",date);
    small(s_screen,16,270,210,stamp,MUTED);
    box(s_screen,14,291,212,1,LINE,0,0);
    lv_obj_t *h=label(s_screen,24,297,192,hint,&reset_font_14,INK); lv_obj_set_style_text_align(h,LV_TEXT_ALIGN_CENTER,0);
}
static void home(const reset_snapshot_t *v,int64_t now) {
    label(s_screen,14,39,220,"Codex Resets",&lv_font_montserrat_20,INK);
    lv_obj_t *card=box(s_screen,14,74,212,133,PAPER,2,12);
    small(card,12,10,184,"距最近一次额度重置",MUTED);
    char value[32]="--",unit[40]="等待同步";
    if(v->status.valid) {
        int64_t elapsed=now-v->status.reset_at;
        if(now<1700000000) snprintf(unit,sizeof(unit),"等待校时");
        else if(elapsed<0) snprintf(unit,sizeof(unit),"时间待确认");
        else { snprintf(value,sizeof(value),"%lld",(long long)(elapsed/86400)); snprintf(unit,sizeof(unit),"天 %02lld 小时 %02lld 分",(long long)(elapsed/3600%24),(long long)(elapsed/60%60)); }
    }
    label(card,10,31,186,value,&lv_font_montserrat_48,RED);
    small(card,12,87,185,unit,INK);
    char date[40]; date_text(date,sizeof(date),v->status.reset_at,8*3600,"%m/%d %H:%M CST");
    small(card,12,108,187,date,MUTED);
    char total[24]="--",average[24]="--";
    if(v->status.valid) { snprintf(total,sizeof(total),"%lu",(unsigned long)v->status.total); snprintf(average,sizeof(average),"%.1f",(double)v->status.average_days); }
    small(s_screen,16,216,96,"累计重置",MUTED); small(s_screen,126,216,102,"平均间隔 / 天",MUTED);
    label(s_screen,16,239,100,total,&lv_font_montserrat_20,INK); label(s_screen,126,239,100,average,&lv_font_montserrat_20,INK);
    footer(v,"上下 切页  OK 公告");
}
static void announcement(const reset_snapshot_t *v) {
    small(s_screen,14,42,212,"最近一次公告",INK);
    lv_obj_t *badge=box(s_screen,14,68,90,25,v->status.banked ? YELLOW : GREEN,1,6);
    small(badge,6,3,80,v->status.banked ? "备用重置" : "常规重置",INK);
    if(v->status.scheduled) small(s_screen,114,72,115,"另有计划重置",RED);
    lv_obj_t *body=box(s_screen,14,105,212,136,PAPER,1,8);
    lv_obj_t *text=label(body,10,8,190,v->status.valid ? v->status.text : "尚无公告，请先联网同步",&reset_font_14,INK);
    lv_obj_update_layout(text);
    int max=(lv_obj_get_height(text)>116 ? lv_obj_get_height(text)-116+23 : 0)/24;
    if(s_nav->scroll>(unsigned)max) s_nav->scroll=(unsigned)max;
    lv_obj_set_y(text,8-(int)s_nav->scroll*24);
    small(s_screen,14,249,214,"codex-resets.com",MUTED);
    footer(v,"上下 滚动  OK 返回");
}
static day_state_t s_days[RESET_DAYS];
static int s_selected;
static char s_day_numbers[RESET_DAYS][3];
static void draw_calendar(lv_event_t *event) {
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_area_t bounds; lv_obj_get_coords(lv_event_get_target_obj(event),&bounds);
    for(unsigned i=0;i<RESET_DAYS;++i) {
        int x=bounds.x1+(int)(i%7)*31, y=bounds.y1+(int)(i/7)*29;
        lv_area_t area={.x1=x,.y1=y,.x2=x+25,.y2=y+23};
        lv_draw_rect_dsc_t rect; lv_draw_rect_dsc_init(&rect);
        rect.bg_color=lv_color_hex(day_color(s_days[i])); rect.bg_opa=LV_OPA_COVER;
        rect.radius=5; rect.border_width=(int)i==s_selected?2:1;
        rect.border_color=lv_color_hex((int)i==s_selected?INK:LINE); rect.border_opa=LV_OPA_COVER;
        lv_draw_rect(layer,&rect,&area);
        lv_draw_label_dsc_t text; lv_draw_label_dsc_init(&text);
        text.font=&reset_font_14; text.color=lv_color_hex(INK); text.align=LV_TEXT_ALIGN_CENTER;
        text.text=s_day_numbers[i]; text.text_static=1;
        area.y1+=2; lv_draw_label(layer,&text,&area);
    }
}
static void challenge(const reset_snapshot_t *v,int64_t now) {
    label(s_screen,14,39,220,"Tibo / 28",&lv_font_montserrat_20,INK);
    int day=reset_challenge_day(&v->challenge,now);
    char number[12]="--",clock[20],status[64];
    if(day>=1 && day<=28) snprintf(number,sizeof(number),"%02d",day);
    else if(day>28) snprintf(number,sizeof(number),"28");
    label(s_screen,14,66,87,number,&lv_font_montserrat_40,RED);
    small(s_screen,94,71,130,day>28 ? "挑战已结束" : day==0 ? "等待开始" : "挑战日 / 28",INK);
    date_text(clock,sizeof(clock),now>1700000000 ? now : 0,reset_pacific_offset(now),"%H:%M PT"); small(s_screen,94,93,130,clock,MUTED);
    s_selected=s_nav->detail ? (int)s_nav->selected_day : day>=1 && day<=28 ? day-1 : -1;
    for(unsigned i=0;i<RESET_DAYS;++i) {
        s_days[i]=v->challenge.valid ? v->challenge.days[i] : DAY_UNKNOWN;
        snprintf(s_day_numbers[i],sizeof(s_day_numbers[i]),"%02u",i+1);
    }
    lv_obj_t *grid=lv_obj_create(s_screen); lv_obj_remove_style_all(grid);
    lv_obj_set_pos(grid,14,128); lv_obj_set_size(grid,212,111);
    lv_obj_remove_flag(grid,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(grid,draw_calendar,LV_EVENT_DRAW_MAIN,NULL);
    unsigned selected=s_nav->detail ? s_nav->selected_day : day>0 ? (unsigned)(day>28 ? 27 : day-1) : 0;
    snprintf(status,sizeof(status),"第 %02u 天 · %s",selected+1,v->challenge.valid ? reset_day_label(v->challenge.days[selected]) : "待确认");
    small(s_screen,14,248,218,status,INK);
    footer(v,s_nav->detail ? "上下 选日  OK 返回" : "上下 切页  OK 日历");
}
static void setup(const reset_snapshot_t *v) {
    small(s_screen,16,44,212,"连接 Wi-Fi",INK);
    small(s_screen,16,77,210,"1. 手机连接设备热点",MUTED);
    small(s_screen,16,101,215,v->ap_name,INK);
    small(s_screen,16,128,210,"热点密码",MUTED);
    small(s_screen,16,152,215,v->ap_password,INK);
    small(s_screen,16,183,210,"2. 浏览器打开",MUTED);
    label(s_screen,16,208,214,"192.168.4.1",&lv_font_montserrat_20,RED);
    small(s_screen,16,248,211,v->message,MUTED);
    small(s_screen,26,296,195,"长按上键 关闭设置",INK);
}
void reset_ui_render(reset_nav_t *nav,const reset_snapshot_t *v,int64_t now,int battery) {
    s_nav=nav; s_battery=battery;
    if(s_screen) lv_obj_clean(s_screen);
    else { s_screen=lv_obj_create(NULL); lv_obj_remove_style_all(s_screen); }
    lv_obj_set_style_bg_color(s_screen,lv_color_hex(CREAM),0); lv_obj_set_style_bg_opa(s_screen,LV_OPA_COVER,0);
    lv_obj_remove_flag(s_screen,LV_OBJ_FLAG_SCROLLABLE);
    header(v);
    if(v->setup) setup(v);
    else if(s_nav->page) challenge(v,now);
    else if(s_nav->detail) announcement(v);
    else home(v,now);
    lv_screen_load(s_screen);
}
