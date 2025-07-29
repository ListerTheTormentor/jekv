#ifndef __JEKV_HASH_H__
#define __JEKV_HASH_H__

#include <stdint.h>
#include "jekv_base.h"
#include "jekv_porting.h"
#include "jekv_item.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JEKV_HASH_INVALID -1

/**
  * @brief  kv hash table node
  */
typedef union {
    struct {
        uint32_t id   : 8;  /**< slice id in sector */
        uint32_t hash : 24; /**< hash code          */
    };
    struct {
        int8_t index; /**< look for index     */
        uint8_t resv[3];
    };
} jekv_hash_node_t;

/**
  * @brief  kv hash table information
  */
typedef struct {
    jekv_hash_node_t *hash_table; /**< hash table array */
    uint8_t count;                /**< item entry num   */
    uint8_t size;                 /**< hash table size  */
} jekv_hash_t;

int jekv_hash_init(jekv_hash_t *h);
int jekv_hash_append(jekv_hash_t *h, const jekv_item_t *item, uint32_t index);
int jekv_hash_erase(jekv_hash_t *h, const uint32_t index);
int jekv_hash_find(jekv_hash_t *h, uint32_t start, const jekv_item_t *item);
void jekv_hash_clear(jekv_hash_t *h);

#ifdef __cplusplus
}
#endif

#endif
