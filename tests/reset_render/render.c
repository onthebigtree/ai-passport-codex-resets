#include "reset_ui.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LV_FONT_DECLARE(reset_font_14);
static uint16_t frame[240*320];
static uint16_t draw[240*20];
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *pixels) {
    uint16_t *src=(uint16_t *)pixels;
    for(int y=area->y1;y<=area->y2;++y) for(int x=area->x1;x<=area->x2;++x) frame[y*240+x]=*src++;
    lv_display_flush_ready(d);
}
static void capture(const char *name) {
    lv_refr_now(NULL); char path[128];snprintf(path,sizeof(path),"build/%s.ppm",name);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 320\n255\n");
    for(int i=0;i<240*320;++i) {
        uint16_t p=frame[i];unsigned char rgb[]={(unsigned char)((p>>11)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(void) {
    lv_init(); lv_display_t *d=lv_display_create(240,320);assert(d);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,draw,NULL,sizeof(draw),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
    lv_font_glyph_dsc_t glyph={0};assert(lv_font_get_glyph_dsc(&reset_font_14,&glyph,0x91cd,0)&&!glyph.is_placeholder);
    assert(!lv_font_get_glyph_dsc(&reset_font_14,&glyph,0x9f98,0)||glyph.is_placeholder);
    reset_snapshot_t view={.network=NET_ONLINE};reset_nav_t nav={0};
    view.status=(reset_status_t){.valid=true,.reset_at=1790975928,.fetched_at=1791208800,.total=57,.average_days=6.8f};
    snprintf(view.status.text,sizeof(view.status.text),"Reset all propagated. Enjoy. https://t.co/GaVJhbptR0");
    view.challenge=(challenge_t){.valid=true,.start_day=reset_civil_day(2026,10,5),.fetched_at=1791208800};
    view.challenge.days[0]=DAY_OPEN;for(int i=1;i<28;++i)view.challenge.days[i]=DAY_UPCOMING;
    reset_ui_render(&nav,&view,1791208800,-1);capture("reset-home-native");
    nav.page=1;reset_ui_render(&nav,&view,1791208800,-1);capture("reset-challenge-native");
    nav.page=0;nav.detail=true;reset_ui_render(&nav,&view,1791208800,-1);capture("reset-announcement-native");
    for(int i=0;i<100;++i){nav.page=i%2;nav.detail=i%3==0;reset_ui_render(&nav,&view,1791208800,-1);lv_refr_now(NULL);}
    assert(lv_mem_test()==LV_RESULT_OK);
    lv_mem_monitor_t monitor;lv_mem_monitor(&monitor);
    printf("Native LVGL render: PASS, free pool=%zu / %zu bytes after 100 page switches\n",monitor.free_size,monitor.total_size);
    return 0;
}
