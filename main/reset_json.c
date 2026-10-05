#include "reset_json.h"
#include "cJSON.h"
#include <math.h>
#include <string.h>
static const cJSON *field(const cJSON *o,const char *key) { return cJSON_GetObjectItemCaseSensitive(o,key); }
bool reset_parse_status(const char *body, reset_status_t *out) {
    cJSON *root=cJSON_Parse(body);
    if (!root) return false;
    reset_status_t v={0};
    const cJSON *data=field(root,"data"), *stats=field(data,"stats"), *last=field(data,"latest_reset");
    const cJSON *total=field(stats,"total"), *avg=field(stats,"avg_interval_days");
    const cJSON *at=field(last,"announced_at"), *text=field(last,"text"), *type=field(last,"reset_type");
    bool ok=cJSON_IsNumber(total) && total->valuedouble>=1 && total->valuedouble<=1000000 && floor(total->valuedouble)==total->valuedouble
        && cJSON_IsNumber(avg) && isfinite(avg->valuedouble) && avg->valuedouble>=0 && avg->valuedouble<10000
        && cJSON_IsString(at) && cJSON_IsString(text) && cJSON_IsString(type)
        && (!strcmp(type->valuestring,"regular") || !strcmp(type->valuestring,"banked"));
    if (ok) {
        v.reset_at=reset_parse_iso(at->valuestring); ok=v.reset_at>0;
        v.total=(uint32_t)total->valuedouble; v.average_days=(float)avg->valuedouble;
        v.banked=!strcmp(type->valuestring,"banked");
        v.scheduled=cJSON_IsObject(field(data,"scheduled_reset"));
        reset_copy_text(v.text,sizeof(v.text),text->valuestring);
        v.valid=ok;
    }
    cJSON_Delete(root);
    if (ok) *out=v;
    return ok;
}
