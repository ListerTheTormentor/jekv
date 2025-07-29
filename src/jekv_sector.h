#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "dlist.h"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_hash.h"
#include "jekv_item.h"
#include "jekv_partition.h"

#ifndef __JEKV_SECTOR_H__
#define __JEKV_SECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_NVS_VER_NUM
#define CONFIG_NVS_VER_NUM 1
#endif

#define JEKV_SECTOR_MAGIC 0x4D57

/**
  * @brief  kv sector state
  */
typedef enum {
    JEKV_SECTOR_STATE_UNINIT    = 0xff, /* 1111 1111 uninit   */
    JEKV_SECTOR_STATE_USING     = 0xfe, /* 1111 1110 using    */
    JEKV_SECTOR_STATE_FULL      = 0xfc, /* 1111 1100 full     */
    JEKV_SECTOR_STATE_DELETTING = 0xf8, /* 1111 1000 deleting */
    JEKV_SECTOR_STATE_CRASH     = 0xf0, /* 1111 0000 crashed  */
    JEKV_SECTOR_STATE_INVALID   = 0x00, /* 0000 0000 invalid  */
} jekv_sector_state_t;

/**
  * @brief  kv sector header structure
  */
typedef struct {
    uint16_t magic;         /**< sector header magic  */
    uint8_t state;          /**< sector state         */
    uint8_t reserve_1;      /**< sector reserve1      */
    uint32_t crc32;         /**< sector crc32         */
    uint32_t serial_number; /**< sector serial number */
    uint8_t version;        /**< sector version       */
    uint8_t reserve_2[19];  /**< sector reserve2      */
} jekv_sector_header_t;

/**
  * @brief  kv sector manager information
  */
typedef struct {
    struct dl_list list; /* for list manager */
    uint8_t state;       /* sector state     */
    uint8_t version;     /* sector version   */

    uint8_t next_free_slice; /* next free slice id       */
    uint8_t used_slice;      /* used num : using + droped*/
    uint8_t droped_slice;    /* erased num               */

    uint32_t address;       /* offset address from partition start position */
    uint32_t serial_number; /* sector serial number */
    jekv_hash_t hash;     /* hash list            */
    jekv_partition_t *pt; /* partition info       */
} jekv_sector_t;

int jekv_sector_init(jekv_sector_t *sec);

int jekv_sector_set_state(jekv_sector_t *sec, jekv_sector_state_t state);

int jekv_sector_erase(jekv_sector_t *sec);

/*index start from 0. not include header */
int jekv_sector_erase_item(jekv_sector_t *sec, int index, jekv_item_t *item, bool erase_hash);

int jekv_sector_load(jekv_partition_t *pt, jekv_sector_t *sec, int index);

int jekv_sector_write_item_data(jekv_sector_t *sec, jekv_item_t *pitem, const void *extra_data, uint32_t len,
                                  int entry_cnt);

int jekv_sector_write_item(jekv_sector_t *sec, uint8_t group_id, jekv_type_t type, const char *key, const void *data,
                             uint32_t size, uint8_t seg_id);

int jekv_sector_read_item_data(jekv_sector_t *sec, int found_slice_index, jekv_item_t *item, void *data, uint32_t size);

int jekv_sector_find_item(jekv_sector_t *sec, uint8_t group_id, jekv_type_t type, const char *key, int *item_index,
                            jekv_item_t *item, uint8_t seg_index, jekv_seg_start_t seg_start);

int jekv_sector_copy(jekv_sector_t *dst, jekv_sector_t *src);

#ifdef __cplusplus
}
#endif

#endif
