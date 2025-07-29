#ifndef __JEKV_STORAGE_H__
#define __JEKV_STORAGE_H__

#include <stdint.h>

#include "jekv_base.h"
#include "jekv_porting.h"
#include "jekv_partition.h"
#include "jekv_sector_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct dl_list list;               /**< group link node  */
    char name[JEKV_MAX_KEY_LEN + 1]; /**< group name       */
    uint8_t id;                        /**< group id         */
} jekv_group_t;

/**
  * @brief  kv storage structure
  */
typedef struct {
    struct dl_list list;        /**< patition storage list      */
    jekv_partition_t pt;        /**< kv partition information   */
    jekv_sector_manager_t sm;   /**< sector manager information */
    struct dl_list group_list;  /**< group list                 */
} jekv_storage_t;

int jekv_storage_init(jekv_partition_t *pt, jekv_storage_t **storage);
int jekv_storage_deinit(jekv_storage_t *storage);

int jekv_storage_open_group(jekv_storage_t *storage, const char *group, bool create_new, uint8_t *group_id);
int jekv_storage_del_group(jekv_storage_t *storage, uint8_t group_id);

int jekv_storage_write_item(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, const char *key,
                              const void *data, uint32_t size);
int jekv_storage_read_item(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, const char *key, void *data,
                             uint32_t *size);
int jekv_storage_del_item(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, const char *key);

int jekv_storage_find_key(jekv_storage_t *storage, uint8_t group_id, const char *key, jekv_item_t *item);

jekv_group_t *jekv_storage_find_group_by_name(jekv_storage_t *storage, const char *group_name);

jekv_group_t *jekv_storage_find_group_by_id(jekv_storage_t *storage, uint8_t group_id);

#ifdef __cplusplus
}
#endif

#endif
