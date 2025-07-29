#ifndef __JEKV_PARTITION_MANAGER_H__
#define __JEKV_PARTITION_MANAGER_H__

#include <stdint.h>
#include "dlist.h"
#include "jekv_base.h"
#include "jekv_porting.h"
#include "jekv_sector.h"
#include "jekv_item.h"
#include "jekv_hash.h"
#include "jekv_storage.h"
#include "jekv_handler.h"
#include "jekv_iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @note  ptm is the abbreviation of partition manager
  */

/**
  * @brief  kv partition init, add partition to list
  */
int jekv_ptm_init(const char *partition_name);

/**
  * @brief  kv partition deinit, remove partition from list
  */
int jekv_ptm_deinit(const char *partition_name);

/**
  * @brief  is kv partition manager init
  */
int jekv_ptm_is_in_using(void);

/**
  * @brief  open handle
  */
int jekv_ptm_open_handle(const char *partition_name, const char *group, jekv_open_mode_t mode,
                           jekv_handle_info_t **handle);

/**
  * @brief  close handle
  */
int jekv_ptm_close_handle(jekv_handle_info_t *handle);

/**
  * @brief  is handle valid
  */
bool jekv_ptm_is_handle_valid(jekv_handle_info_t *handle);

/**
  * @brief  find parition storage by parition name
  */
jekv_storage_t *jekv_ptm_find_storage(const char *partition_name);

struct dl_list *jekv_get_storage_list(void);
struct dl_list *jekv_get_handle_list(void);

int jekv_ptm_get_status(const char *partition_name, jekv_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
