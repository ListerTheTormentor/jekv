#include <string.h>
#include <stdlib.h>

#include "dlist.h"
#include "jekv_base.h"
#include "jekv_hash.h"
#include "jekv_item.h"
#include "jekv_porting.h"
#include "jekv_partition.h"
#include "jekv_sector.h"

#ifndef __JEKV_SECTOR_MANAGER_H__
#define __JEKV_SECTOR_MANAGER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct dl_list active;    /**< using sector list      */
    struct dl_list idle;      /**< idle sector list       */
    jekv_partition_t *pt;     /**< partition infomation   */
    jekv_sector_t *sec_arr;   /**< sector infomation list */
    uint32_t serial_number;   /**< next serial number     */

} jekv_sector_manager_t;

int jekv_sm_load(jekv_sector_manager_t *sm, jekv_partition_t *pt);
int jekv_sm_unload(jekv_sector_manager_t *sm);

int jekv_sm_get_status(jekv_sector_manager_t *sm, jekv_status_t *status);
int jekv_sm_request_sector(jekv_sector_manager_t *sm, int need_size);

int jekv_sm_find_item(jekv_sector_manager_t *sm, uint8_t group_id, jekv_type_t type, const char *key, int *item_index,
                        jekv_sector_t **sector, jekv_item_t *item, uint8_t seg_index, jekv_seg_start_t seg_start);

int jekv_sm_check_write_blob_size(jekv_sector_manager_t *sm, uint32_t size);

inline static jekv_sector_t *jekv_sm_get_current_sector(jekv_sector_manager_t *sm)
{
    return dl_list_last(&sm->active, jekv_sector_t, list);
}

inline static int jekv_sm_get_gc_size(jekv_sector_t *sec)
{
    return (JEKV_ENTRY_COUNT - sec->used_slice + sec->droped_slice) * JEKV_SLICE_SIZE;
}

inline static int jekv_sm_get_free_size(jekv_sector_t *sec)
{
    return (JEKV_ENTRY_COUNT - sec->used_slice) * JEKV_SLICE_SIZE;
}

int jekv_sm_get_status(jekv_sector_manager_t *sm, jekv_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
