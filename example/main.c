#include <stdio.h>
#include "jekv_easy.h"

int main(void){

    int err;
    char buf[32];

    printf("test start\n");

    err = jekv_easy_init();
    printf("jekv_easy_init err=%d\n",err);

    jekv_easy_write_str("keys1","1111");
    jekv_easy_write_str("keys2","2222");

    jekv_easy_read_str("keys1",buf,sizeof(buf),"");
    printf("read s1=%s\n", buf);

    jekv_easy_read_str("keys2",buf,sizeof(buf),"");
    printf("read s2=%s\n", buf);

    return 0;
}