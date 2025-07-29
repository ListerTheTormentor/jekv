#ifndef __JEKV_EASY_H__
#define __JEKV_EASY_H__

#include <stdio.h>
#include <stdint.h>
#include "jekv_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  kv default partition name
  */
#define JEKV_DEF_PARTITION      "kvs"
#define JEKV_DEF_GROUP          "def_group"

int jekv_easy_init(void);
int jekv_easy_deinit(void);

void jekv_easy_write_str(const char *key, const char *value);
char* jekv_easy_read_str(const char *key, char value[], uint32_t size,const char* def_value);

void jekv_easy_write_int(const char *key, int value);
int jekv_easy_read_int(const char *key, int def_value);

void jekv_easy_erase_key(const char *key);

#ifdef __cplusplus
}
#endif

#endif
