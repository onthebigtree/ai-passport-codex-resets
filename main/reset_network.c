#include "reset_app.h"
#include "reset_json.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CONNECTED BIT0
#define DISCONNECTED BIT1
#define REQUEST_REFRESH 1
#define REQUEST_SETUP 2
#define POLL_US (300LL * 1000000)
#define MAX_STATUS 12288
static const char *TAG="resets_net";
static EventGroupHandle_t s_events;
static QueueHandle_t s_requests, s_credentials;
static httpd_handle_t s_server;
static reset_snapshot_t s_view;
static bool s_portal;
static char s_token[33];
typedef struct { char ssid[33], password[65]; } credentials_t;
typedef struct { uint32_t version; reset_status_t status; challenge_t challenge; } cache_t;
static void publish(reset_net_state_t state,const char *message) {
    s_view.network=state;
    snprintf(s_view.message,sizeof(s_view.message),"%s",message);
    s_view.setup=s_portal; reset_publish(&s_view);
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data) {
    (void)arg; (void)data;
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        xEventGroupClearBits(s_events,DISCONNECTED); xEventGroupSetBits(s_events,CONNECTED);
    } else if (base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_events,CONNECTED); xEventGroupSetBits(s_events,DISCONNECTED);
    }
}
static bool load_credentials(credentials_t *c) {
    nvs_handle_t h; if (nvs_open("codex",NVS_READONLY,&h)!=ESP_OK) return false;
    size_t n=sizeof(*c); esp_err_t e=nvs_get_blob(h,"wifi",c,&n); nvs_close(h);
    return e==ESP_OK && n==sizeof(*c) && c->ssid[0] && c->ssid[32]==0 && c->password[64]==0;
}
static bool save_credentials(const credentials_t *c) {
    nvs_handle_t h; if (nvs_open("codex",NVS_READWRITE,&h)!=ESP_OK) return false;
    esp_err_t e=nvs_set_blob(h,"wifi",c,sizeof(*c));
    if(e==ESP_OK) e=nvs_commit(h);
    nvs_close(h); return e==ESP_OK;
}
static void load_cache(void) {
    nvs_handle_t h; if (nvs_open("codex",NVS_READONLY,&h)!=ESP_OK) return;
    cache_t cache={0}; size_t n=sizeof(cache);
    if (nvs_get_blob(h,"cache",&cache,&n)==ESP_OK && n==sizeof(cache) && cache.version==RESET_CACHE_VERSION) {
        cache.status.text[RESET_TEXT-1]=0;
        bool valid=true;
        for (int i=0;i<RESET_DAYS;++i) if(cache.challenge.days[i]>DAY_CLOSED) valid=false;
        if(valid) { s_view.status=cache.status; s_view.challenge=cache.challenge; }
    }
    nvs_close(h);
}
static void save_cache(void) {
    /* At most one write per five-minute successful sync; no per-second writes. */
    cache_t cache={.version=RESET_CACHE_VERSION,.status=s_view.status,.challenge=s_view.challenge};
    nvs_handle_t h; if(nvs_open("codex",NVS_READWRITE,&h)!=ESP_OK) return;
    if(nvs_set_blob(h,"cache",&cache,sizeof(cache))==ESP_OK) (void)nvs_commit(h);
    nvs_close(h);
}
static bool connect_wifi(const credentials_t *c) {
    (void)esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(150));
    xEventGroupClearBits(s_events,CONNECTED|DISCONNECTED);
    wifi_config_t cfg={0};
    memcpy(cfg.sta.ssid,c->ssid,strlen(c->ssid)); memcpy(cfg.sta.password,c->password,strlen(c->password));
    cfg.sta.pmf_cfg.capable=true;
    if(esp_wifi_set_config(WIFI_IF_STA,&cfg)!=ESP_OK || esp_wifi_connect()!=ESP_OK) return false;
    EventBits_t bits=xEventGroupWaitBits(s_events,CONNECTED|DISCONNECTED,pdFALSE,pdFALSE,pdMS_TO_TICKS(20000));
    return (bits&CONNECTED)!=0;
}
static esp_err_t setup_get(httpd_req_t *req) {
    char page[2200];
    snprintf(page,sizeof(page),"<!doctype html><html lang=zh-CN><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Codex Resets 配网</title><style>body{font:16px system-ui;background:#fff4dd;color:#24211b;max-width:420px;margin:48px auto;padding:24px}input,button{box-sizing:border-box;width:100%%;padding:14px;margin:8px 0 20px;border:2px solid;border-radius:10px;font:inherit}button{background:#f66345;font-weight:bold}small{line-height:1.8}</style><h1>连接你的 Passport</h1><p>输入 2.4 GHz Wi-Fi。连接成功后才会保存。</p><form method=post action=/save><input type=hidden name=token value='%s'><label>Wi-Fi 名称<input name=ssid maxlength=32 required autocomplete=off></label><label>Wi-Fi 密码<input type=password name=password maxlength=64 autocomplete=off></label><button>连接并保存</button></form><small>结果会显示在设备屏幕上。成功后此热点关闭。<br>数据来自 <a href='https://codex-resets.com'>Codex Resets</a>，与 OpenAI 无关联。</small></html>",s_token);
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,page);
}
static int hex_digit(char c) { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; if(c>='A'&&c<='F') return c-'A'+10; return -1; }
static bool decode(char *out,size_t cap,const char *src) {
    size_t n=0;
    for(size_t i=0;src[i];++i) {
        unsigned char c=src[i];
        if(c=='%') {
            if(!src[i+1] || !src[i+2]) return false;
            int a=hex_digit(src[i+1]), b=hex_digit(src[i+2]); if(a<0||b<0) return false;
            c=(unsigned char)(a*16+b); i+=2;
        } else if(c=='+') c=' ';
        if(c<32 || c==127 || n+1>=cap) return false;
        out[n++]=(char)c;
    }
    out[n]=0; return true;
}
static esp_err_t setup_save(httpd_req_t *req) {
    if(req->content_len<=0 || req->content_len>512) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Form too large");
    char body[513]; int done=0;
    while(done<req->content_len) {
        int n=httpd_req_recv(req,body+done,req->content_len-done);
        if(n<=0) return httpd_resp_send_err(req,HTTPD_408_REQ_TIMEOUT,"Incomplete form");
        done+=n;
    }
    body[done]=0;
    char token[100], raw_ssid[100], raw_password[200]; credentials_t c={0};
    if(httpd_query_key_value(body,"token",token,sizeof(token))!=ESP_OK || strcmp(token,s_token)
       || httpd_query_key_value(body,"ssid",raw_ssid,sizeof(raw_ssid))!=ESP_OK
       || httpd_query_key_value(body,"password",raw_password,sizeof(raw_password))!=ESP_OK
       || !decode(c.ssid,sizeof(c.ssid),raw_ssid) || !decode(c.password,sizeof(c.password),raw_password) || !c.ssid[0])
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid network details");
    size_t len=strlen(c.password);
    if(len>0 && len<8) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Password must be at least 8 bytes");
    if(len==64) for(size_t i=0;i<len;++i) if(hex_digit(c.password[i])<0) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid PSK");
    if(xQueueSend(s_credentials,&c,0)!=pdTRUE) {
        httpd_resp_set_status(req,"503 Service Unavailable");
        return httpd_resp_sendstr(req,"Connection in progress");
    }
    httpd_resp_set_type(req,"text/plain; charset=utf-8");
    return httpd_resp_sendstr(req,"正在连接，请查看设备屏幕。失败时可返回重试；原有配置会保留。");
}
static bool start_portal(void) {
    if(s_portal) return true;
    uint32_t random[4]; esp_fill_random(random,sizeof(random));
    snprintf(s_view.ap_name,sizeof(s_view.ap_name),"Passport-%04lX",(unsigned long)(random[0]&0xffff));
    snprintf(s_view.ap_password,sizeof(s_view.ap_password),"%08lx%04lx",(unsigned long)random[1],(unsigned long)(random[2]&0xffff));
    snprintf(s_token,sizeof(s_token),"%08lx%08lx%08lx%08lx",(unsigned long)random[0],(unsigned long)random[1],(unsigned long)random[2],(unsigned long)random[3]);
    wifi_config_t ap={0};
    memcpy(ap.ap.ssid,s_view.ap_name,strlen(s_view.ap_name));
    memcpy(ap.ap.password,s_view.ap_password,strlen(s_view.ap_password));
    ap.ap.ssid_len=strlen(s_view.ap_name); ap.ap.authmode=WIFI_AUTH_WPA2_PSK; ap.ap.max_connection=1; ap.ap.channel=1;
    if(esp_wifi_set_mode(WIFI_MODE_APSTA)!=ESP_OK || esp_wifi_set_config(WIFI_IF_AP,&ap)!=ESP_OK) {
        (void)esp_wifi_set_mode(WIFI_MODE_STA); return false;
    }
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG(); cfg.max_open_sockets=3; cfg.lru_purge_enable=true; cfg.stack_size=4608;
    cfg.recv_wait_timeout=5; cfg.send_wait_timeout=5;
    if(httpd_start(&s_server,&cfg)!=ESP_OK) { (void)esp_wifi_set_mode(WIFI_MODE_STA); return false; }
    const httpd_uri_t get={.uri="/",.method=HTTP_GET,.handler=setup_get};
    const httpd_uri_t post={.uri="/save",.method=HTTP_POST,.handler=setup_save};
    if(httpd_register_uri_handler(s_server,&get)!=ESP_OK || httpd_register_uri_handler(s_server,&post)!=ESP_OK) {
        httpd_stop(s_server); s_server=NULL; (void)esp_wifi_set_mode(WIFI_MODE_STA); return false;
    }
    s_portal=true; publish(NET_SETUP,"连接热点后打开设置网址"); return true;
}
static void stop_portal(void) {
    if(s_server) { httpd_stop(s_server); s_server=NULL; }
    (void)esp_wifi_set_mode(WIFI_MODE_STA); s_portal=false; s_view.setup=false;
    memset(s_view.ap_password,0,sizeof(s_view.ap_password)); memset(s_token,0,sizeof(s_token));
}
static esp_http_client_handle_t open_url(const char *url) {
    esp_http_client_config_t config={.url=url,.timeout_ms=15000,.crt_bundle_attach=esp_crt_bundle_attach,
        .buffer_size=2048,.buffer_size_tx=1024,.user_agent="AI-Passport-Codex-Resets/1.0",.disable_auto_redirect=true};
    esp_http_client_handle_t h=esp_http_client_init(&config); if(!h) return NULL;
    esp_http_client_set_header(h,"Accept-Encoding","identity");
    if(esp_http_client_open(h,0)!=ESP_OK || esp_http_client_fetch_headers(h)<0 || esp_http_client_get_status_code(h)!=200) {
        esp_http_client_cleanup(h); return NULL;
    }
    return h;
}
static bool fetch_status(reset_status_t *out) {
    char *body=malloc(MAX_STATUS+1); if(!body) return false;
    esp_http_client_handle_t h=open_url("https://codex-resets.com/api/v1/status");
    if(!h) { free(body); return false; }
    int used=0,n=0; bool ok=true;
    while(used<MAX_STATUS && (n=esp_http_client_read(h,body+used,MAX_STATUS-used))>0) used+=n;
    if(n<0 || !esp_http_client_is_complete_data_received(h)) ok=false;
    body[used]=0;
    if(ok) ok=reset_parse_status(body,out);
    esp_http_client_cleanup(h); free(body); return ok;
}
static bool fetch_challenge(challenge_t *out) {
    esp_http_client_handle_t h=open_url("https://codex-resets.com/zh-CN/tibo-28"); if(!h) return false;
    challenge_parser_t parser; challenge_parser_init(&parser);
    char chunk[768]; int n; size_t total=0; bool ok=true;
    while((n=esp_http_client_read(h,chunk,sizeof(chunk)))>0) {
        total+=(size_t)n; if(total>262144) { ok=false; break; }
        challenge_parser_feed(&parser,chunk,(size_t)n);
    }
    ok=ok && n==0 && esp_http_client_is_complete_data_received(h) && challenge_parser_finish(&parser,out);
    esp_http_client_cleanup(h); return ok;
}
static void sync_data(void) {
    publish(NET_SYNCING,"正在同步");
    reset_status_t status; challenge_t challenge;
    bool a=fetch_status(&status), b=fetch_challenge(&challenge);
    int64_t now=time(NULL);
    if(a) { status.fetched_at=now; s_view.status=status; }
    if(b) { challenge.fetched_at=now; s_view.challenge=challenge; }
    if(a || b) save_cache();
    publish(a&&b ? NET_ONLINE : NET_ERROR,a&&b ? "同步成功" : a ? "挑战更新失败" : b ? "重置更新失败" : "更新失败，保留上次数据");
    ESP_LOGI(TAG,"sync reset=%d challenge=%d heap=%lu largest=%lu",a,b,(unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
static void network_task(void *arg) {
    (void)arg;
    esp_err_t err=nvs_flash_init();
    if(err!=ESP_OK) { publish(NET_ERROR,"存储异常，请检查日志"); vTaskDelete(NULL); return; }
    load_cache(); publish(NET_STARTING,"正在启动网络");
    if(esp_netif_init()!=ESP_OK || esp_event_loop_create_default()!=ESP_OK || !esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) goto failed;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&init)!=ESP_OK || esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK
        || esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL)!=ESP_OK
        || esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL)!=ESP_OK
        || esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK || esp_wifi_start()!=ESP_OK) goto failed;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL); esp_sntp_setservername(0,"pool.ntp.org"); esp_sntp_setservername(1,"time.cloudflare.com"); esp_sntp_init();
    credentials_t active={0}; bool have=load_credentials(&active);
    if(have) { publish(NET_CONNECTING,"正在连接 Wi-Fi"); if(!connect_wifi(&active)) publish(NET_OFFLINE,"网络不可用，稍后重试"); }
    else if(!start_portal()) goto failed;
    int64_t last_sync=-POLL_US, last_connect=esp_timer_get_time(), portal_at=esp_timer_get_time();
    for(;;) {
        int request=0; (void)xQueueReceive(s_requests,&request,pdMS_TO_TICKS(500));
        int64_t now=esp_timer_get_time();
        if(request==REQUEST_SETUP) {
            if(s_portal) { stop_portal(); if(have) (void)connect_wifi(&active); publish(NET_OFFLINE,"设置已关闭"); }
            else { if(!start_portal()) publish(NET_ERROR,"设置启动失败"); portal_at=now; }
        }
        credentials_t candidate;
        if(s_portal && xQueueReceive(s_credentials,&candidate,0)==pdTRUE) {
            publish(NET_CONNECTING,"正在验证 Wi-Fi");
            if(connect_wifi(&candidate)) {
                if(save_credentials(&candidate)) {
                    active=candidate; have=true; stop_portal(); last_sync=-POLL_US; publish(NET_SYNCING,"连接成功");
                } else publish(NET_ERROR,"保存失败，请重试");
            } else {
                publish(NET_SETUP,"连接失败，请返回重试");
                if(have) (void)connect_wifi(&active);
            }
            memset(&candidate,0,sizeof(candidate)); portal_at=now;
        }
        if(s_portal) {
            if(now-portal_at>600LL*1000000) { stop_portal(); publish(NET_OFFLINE,"设置超时，长按上键重开"); }
            continue;
        }
        bool connected=(xEventGroupGetBits(s_events)&CONNECTED)!=0;
        if(!connected && have && (now-last_connect>30000000 || request==REQUEST_REFRESH)) {
            publish(NET_CONNECTING,"正在重新连接"); connected=connect_wifi(&active); last_connect=esp_timer_get_time();
            if(!connected) publish(NET_OFFLINE,"网络不可用，稍后重试");
        }
        if(!connected) {
            if(s_view.network==NET_ONLINE || s_view.network==NET_SYNCING) publish(NET_OFFLINE,"已离线，显示上次数据");
            continue;
        }
        /* Clock must be valid before TLS validation or date arithmetic. */
        if(time(NULL)<1700000000) { publish(NET_CONNECTING,"等待网络校时"); continue; }
        if(now-last_sync>=POLL_US || (request==REQUEST_REFRESH && now-last_sync>=15000000)) {
            sync_data(); last_sync=esp_timer_get_time();
        }
    }
failed:
    publish(NET_ERROR,"网络启动失败，请重启"); vTaskDelete(NULL);
}
void reset_network_start(void) {
    s_events=xEventGroupCreate(); s_requests=xQueueCreate(4,sizeof(int)); s_credentials=xQueueCreate(1,sizeof(credentials_t));
    if(!s_events || !s_requests || !s_credentials || xTaskCreate(network_task,"reset_network",8192,NULL,3,NULL)!=pdPASS)
        publish(NET_ERROR,"网络内存不足");
}
void reset_network_request(bool setup) {
    int request=setup ? REQUEST_SETUP : REQUEST_REFRESH;
    if(s_requests) (void)xQueueSend(s_requests,&request,0);
}
