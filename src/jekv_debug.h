#ifndef __JEKV_DEBUG_H__
#define __JEKV_DEBUG_H__

#include <stdint.h>
#include "jekv_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JEKV_TRACE_POWER_OFF(type, stage) jekv_port_power_off(type, stage)

/*power off type */
enum {
    JEKV_TRACE_TYPE_WRITE,
    JEKV_TRACE_TYPE_BLOB,
    JEKV_TRACE_TYPE_GC,
};

/*power off stage for normal write type*/
enum {
    JEKV_TRACE_BEFORE_WRITE_DATA,
    JEKV_TRACE_BEFPRE_ERASE_OLD,
};

/*power off stage for BLOB type*/
enum {
    JEKV_TRACE_AFTER_WRITE_A_SEG,
    JEKV_TRACE_AFTER_WRITE_ALL_SEG,
    JEKV_TRACE_AFTER_MODIFY_NEW,
    JEKV_TRACE_AFTER_ERASE_OLD_DESC,
};

/*power off stage for garbage collection*/
enum {
    JEKV_TRACE_GC_1_NEW_SECTOR,
    JEKV_TRACE_GC_2_SET_OLD_DELETING,
    JEKV_TRACE_GC_3_COPY,
    JEKV_TRACE_GC_4_ERASE_OLD,
};

int jekv_debug_print_sector(const char *partition_name, int index, int size);

int jekv_debug_print_status(const char *partition_name);

int jekv_debug_print_storage(int storage_detail, int sec_detail);

int jekv_debug_print_handle(void);

void jekv_debug_print_value(const char *key, jekv_type_t type, const void *p, uint32_t length);

void jekv_debug_cmd(int argc, char *argv[]);

int jekv_debug_jekv_open(const char *partition, const char *group, jekv_open_mode_t mode);

int jekv_debug_jekv_close(int id);

int jekv_debug_clear_partiton_handle(const char *partition);

jekv_handle_t jekv_debug_jekv_get_handle(int id);

bool jekv_debug_jekv_check_handle_id(int id);

int jekv_debug_cmd_get_item(int id, const char *key);

int jekv_debug_set_item_with_size(int id, const char *key, const char *str_size, jekv_type_t type);

int jekv_debug_set_base_item(int id, const char *key, const char *type, const char *value);

int jekv_debug_del(int id, const char *key);

int jekv_debug_del_group(int id);

#ifdef __cplusplus
}
#endif

#endif
