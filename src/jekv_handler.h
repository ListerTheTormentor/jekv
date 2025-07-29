#ifndef __JEKV_HANDLER_H__
#define __JEKV_HANDLER_H__

#include <stdint.h>
#include "dlist.h"
#include "jekv_base.h"
#include "jekv_porting.h"
#include "jekv_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  kv open handle information structure
  */
typedef struct jekv_handle_info_t {
    struct dl_list list;       /**< handle list         */
    jekv_storage_t *storage;   /**< storage information */
    uint8_t group_id;          /**< group id            */
    uint8_t valid;             /**< handle is valid     */
    jekv_open_mode_t mode;     /**< open mode           */
} jekv_handle_info_t;

int jekv_handler_open(jekv_storage_t *storage, uint8_t group_id, jekv_open_mode_t mode, jekv_handle_info_t **handle);
int jekv_handler_close(jekv_handle_info_t *handle);

#ifdef __cplusplus
}
#endif

#endif
