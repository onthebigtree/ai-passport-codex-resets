#include "reset_json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    FILE *f=fopen("tests/fixtures/codex-status.json","rb");assert(f);char buf[12289];size_t n=fread(buf,1,12288,f);fclose(f);buf[n]=0;
    reset_status_t status={0};assert(reset_parse_status(buf,&status));assert(status.valid&&status.total==57&&!status.banked);assert(status.average_days>6.7f&&status.average_days<6.9f);
    reset_status_t saved=status;
    const char *bad[]={"{}","{","null","{\"data\":null}","{\"data\":{\"stats\":{\"total\":1},\"latest_reset\":null}}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i){assert(!reset_parse_status(bad[i],&status));assert(!memcmp(&status,&saved,sizeof(status)));}
    char *total=strstr(buf,"\"total\":57");assert(total);memcpy(total+8,"-1",2);assert(!reset_parse_status(buf,&status));
    puts("Reset JSON: PASS (live fixture, invalid schemas, cache preservation)");
}
