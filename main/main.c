#include "reset_app.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "reset_ui.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static QueueHandle_t s_input;
static SemaphoreHandle_t s_mutex;
static reset_snapshot_t s_snapshot;
static reset_nav_t s_nav;
static int s_battery=-1;
typedef struct { int key; bsp_btn_ev_t event; } input_t;
void reset_publish(const reset_snapshot_t *snapshot) {
    if(!s_mutex) return;
    xSemaphoreTake(s_mutex,portMAX_DELAY); s_snapshot=*snapshot; xSemaphoreGive(s_mutex);
    const input_t update={.key=-1}; if(s_input) (void)xQueueSend(s_input,&update,0);
}
static void on_key(bsp_btn_t button,bsp_btn_ev_t event,void *user) {
    (void)user;
    const input_t input={.key=button,.event=event};
    if(s_input) (void)xQueueSend(s_input,&input,0);
}
static void ui_task(void *arg) {
    (void)arg;
    int64_t last_minute=-1; unsigned ticks=0; reset_snapshot_t view={0};
    for(;;) {
        input_t input; bool changed=xQueueReceive(s_input,&input,pdMS_TO_TICKS(1000))==pdTRUE;
        if(changed && input.key>=0) {
            if(input.event==BSP_BTN_LONG && input.key==BSP_BTN_UP) reset_network_request(true);
            else if(input.event==BSP_BTN_LONG && input.key==BSP_BTN_OK) reset_network_request(false);
            else if(input.event==BSP_BTN_CLICK && !view.setup) {
                reset_navigation(&s_nav,input.key==BSP_BTN_UP ? KEY_UP : input.key==BSP_BTN_DOWN ? KEY_DOWN : KEY_OK);
                if(s_nav.page && s_nav.detail && input.key==BSP_BTN_OK) {
                    int d=reset_challenge_day(&view.challenge,time(NULL)); s_nav.selected_day=d>0 && d<=28 ? (unsigned)d-1 : 0;
                }
            }
        }
        int64_t now=time(NULL);
        if(ticks++%30==0) { s_battery=bsp_battery_soc(); changed=true; }
        if(changed || now/60!=last_minute) {
            xSemaphoreTake(s_mutex,portMAX_DELAY); view=s_snapshot; xSemaphoreGive(s_mutex);
            if(bsp_lvgl_lock(500)) { reset_ui_render(&s_nav,&view,now,s_battery); bsp_lvgl_unlock(); }
            last_minute=now/60;
        }
    }
}
void app_main(void) {
    s_mutex=xSemaphoreCreateMutex(); s_input=xQueueCreate(12,sizeof(input_t));
    if(!s_mutex || !s_input) { ESP_LOGE("resets","Input allocation failed"); return; }
    (void)bsp_i2c_init(); (void)bsp_battery_init();
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init()) { ESP_LOGE("resets","Display failed"); return; }
    bsp_display_backlight(80);
    if(bsp_lvgl_lock(1000)) { reset_ui_render(&s_nav,&s_snapshot,time(NULL),s_battery); bsp_lvgl_unlock(); }
    if(xTaskCreate(ui_task,"reset_ui",4608,NULL,4,NULL)!=pdPASS) { ESP_LOGE("resets","UI allocation failed"); return; }
    if(bsp_button_init(on_key,NULL)!=ESP_OK) ESP_LOGE("resets","Button initialization failed");
    reset_network_start();
}
