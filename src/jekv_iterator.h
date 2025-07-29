#ifndef __JEKV_ITERATOR_H__
#define __JEKV_ITERATOR_H__

#include <stdint.h>

#include "jekv_base.h"
#include "jekv_porting.h"
#include "jekv_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  kv open handle information structure
  */
typedef struct {
    jekv_storage_t *storage; /**< storage info          */
    jekv_sector_t *sector;   /**< current sector        */
    int slice_index;         /**< current slice index   */
    uint8_t group_id;        /**< group id for look up  */
    jekv_type_t type;        /**< item type for look up */
    jekv_entry_t info;       /**< item information      */
} jekv_iterator_info_t;

int jekv_iterator_find(const char *partition_name, const char *group, jekv_type_t type, jekv_iterator_t *output_iterator);

int jekv_iterator_find_by_handle(jekv_handle_t handle, jekv_type_t type, jekv_iterator_t *output_iterator);

int jekv_iterator_next(jekv_iterator_t *iterator);

int jekv_iterator_info(jekv_iterator_t iterator, jekv_entry_t *info);

int jekv_iterator_data(jekv_iterator_t iterator, void *data, uint32_t *data_len);

int jekv_iterator_release(jekv_iterator_t iterator);

int jekv_iterator_print(const char *partition_name, const char *group_name);

#ifdef __cplusplus
}
#endif

#endif
