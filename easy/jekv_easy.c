#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "jekv_easy.h"

static jekv_handle_t g_handle = NULL;

int jekv_easy_init(void)
{
    int err;

    if(g_handle){
        return JEKV_ERR_OK;
    }

    err =  jekv_init(JEKV_DEF_PARTITION);
    if(err != JEKV_ERR_OK){
        return err;
    }

    return jekv_open(JEKV_DEF_PARTITION, JEKV_DEF_GROUP, JEKV_OP_READ_WRITE, &g_handle);
}

int jekv_easy_deinit(void)
{
    if(!g_handle){
        jekv_close(g_handle);
        g_handle = NULL;

        jekv_deinit(JEKV_DEF_PARTITION);
    }
    return JEKV_ERR_OK;
}

void jekv_easy_write_str(const char *key, const char *value)
{
    jekv_set_str(g_handle,key,value);
}

char* jekv_easy_read_str(const char *key, char value[], uint32_t size,const char* def_value)
{
    if(jekv_get_str(g_handle,key,value,&size) != JEKV_ERR_OK){
        snprintf(value,size,"%s",def_value);
    }
    return value;
}

void jekv_easy_write_int(const char *key, int value)
{
    char buf[16];
    snprintf(buf,sizeof(buf),"%d",value);
    jekv_set_str(g_handle,key,buf);
}

int jekv_easy_read_int(const char *key, int def_value)
{
    char buf[16] = "";
    jekv_easy_read_str(key,buf,sizeof(buf),"");
    return atoi(buf);
}

void jekv_easy_erase_key(const char *key)
{
    jekv_del_key(g_handle,key);
}
