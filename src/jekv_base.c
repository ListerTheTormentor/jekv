
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_api"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_partition_manager.h"
#include "jekv_log.h"

#define JEKV_LOCK() jekv_port_mutex_lock()
#define JEKV_UNLOCK() jekv_port_mutex_unlock()

int jekv_init(const char *partition_name)
{
    int err;

    if (!partition_name) {
        return JEKV_ERR_INVALID_PARAM;
    }

    jekv_log_debug("init %s", partition_name);

    if(jekv_port_is_init()){
        return JEKV_ERR_OK;
    }

    /*initialize the port during the first partition initialization*/
    err = jekv_port_init();
    if (err != JEKV_ERR_OK) {
        return err;
    }

    JEKV_LOCK();

    /*init partition*/
    err = jekv_ptm_init(partition_name);

    JEKV_UNLOCK();

    jekv_log_debug("init end,err=%d", err);

    return err;
}

int jekv_deinit(const char *partition_name)
{
    int err;

    if (!partition_name) {
        return JEKV_ERR_INVALID_PARAM;
    }

    /*check kv is initialized or not*/
    if (!(jekv_ptm_is_in_using() && jekv_port_is_init())) {
        return JEKV_ERR_FAIL;
    }

    JEKV_LOCK();

    err = jekv_ptm_deinit(partition_name);

    JEKV_UNLOCK();

    /*deinit port if no partiton in using*/
    if (!jekv_ptm_is_in_using()) {
        jekv_port_deinit();
    }

    return err;
}

int jekv_erase(const char *partition_name)
{
    jekv_storage_t *storage = NULL;

    if (!partition_name) {
        return JEKV_ERR_INVALID_PARAM;
    }

    if (jekv_ptm_is_in_using()) {
        JEKV_LOCK();

        storage = jekv_ptm_find_storage(partition_name);

        JEKV_UNLOCK();

        if (storage) {
            /*Do not erase the using partition*/
            jekv_log_error("%s","partition busy,not erase");
            return JEKV_ERR_FAIL;
        }
    }

    jekv_log_warning("erase %s",partition_name);

    return jekv_pt_erase_all(&storage->pt);
}

int jekv_open(const char *partition_name, const char *group_name, jekv_open_mode_t mode, jekv_handle_t *handle)
{
    int err = JEKV_ERR_FAIL;

    if (!(partition_name && group_name && handle && (int)mode >= 0 && (int)mode < JEKV_OP_MAX)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    if (strlen(group_name) > JEKV_MAX_KEY_LEN) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_ptm_open_handle(partition_name, group_name, mode, (jekv_handle_info_t **)handle);

    JEKV_UNLOCK();

    return err;
}

int jekv_close(jekv_handle_t handle)
{
    int err = JEKV_ERR_FAIL;

    if (!handle) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (jekv_ptm_is_handle_valid(handle)) {
        err = jekv_ptm_close_handle(handle);
    } else {
        err = JEKV_ERR_INVALID_HANDLE;
    }

    JEKV_UNLOCK();

    return err;
}

static int get_item(jekv_handle_t *handle, jekv_type_t type, const char *key, void *out_value, uint32_t *length)
{
    int err;
    jekv_handle_info_t *h = (jekv_handle_info_t *)handle;

    if (!(handle && key && out_value && length && *length > 0 && type > JEKV_TYPE_ANY && type < JEKV_TYPE_MAX)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (jekv_ptm_is_handle_valid(h)) {
        err = jekv_storage_read_item(h->storage, h->group_id, type, key, out_value, length);
    } else {
        err = JEKV_ERR_INVALID_HANDLE;
    }

    JEKV_UNLOCK();

    jekv_log_debug("get item %s, size=%u, err=%d", key, (err == JEKV_ERR_OK ? *length : 0), err);

    return err;
}

static int set_item(jekv_handle_t *handle, jekv_type_t type, const char *key, const void *value, uint32_t length)
{
    int err;
    jekv_handle_info_t *h = (jekv_handle_info_t *)handle;
    int key_len;

    if (!(handle && key && value && type > JEKV_TYPE_ANY && type < JEKV_TYPE_MAX)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    key_len = strlen(key);
    if (!(key_len > 0 && key_len <= JEKV_MAX_KEY_LEN)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (!jekv_ptm_is_handle_valid(h)) {
        err = JEKV_ERR_INVALID_HANDLE;
    } else if (h->mode == JEKV_OP_READ_ONLY) {
        err = JEKV_ERR_READ_ONLY;
    } else {
        err = jekv_storage_write_item(h->storage, h->group_id, type, key, value, length);
    }

    JEKV_UNLOCK();

    jekv_log_debug("write %s, size=%u, err=%d", key, length, err);

    return err;
}

int jekv_get_str(jekv_handle_t handle, const char *key, char *out_value, uint32_t *length)
{
    return get_item(handle, JEKV_TYPE_STRING, key, out_value, length);
}

int jekv_set_str(jekv_handle_t handle, const char *key, const char *value)
{
    if (!value) {
        return JEKV_ERR_INVALID_PARAM;
    }

    return set_item(handle, JEKV_TYPE_STRING, key, value, strlen(value) + 1);
}

int jekv_get_binary(jekv_handle_t handle, const char *key, void *data, uint32_t *data_len)
{
    return get_item(handle, JEKV_TYPE_BINARY, key, data, data_len);
}

int jekv_set_binary(jekv_handle_t handle, const char *key, const void *data, uint32_t data_len)
{
    return set_item(handle, JEKV_TYPE_BINARY, key, data, data_len);
}

int jekv_get_blob(jekv_handle_t handle, const char *key, void *blob, uint32_t *blob_len)
{
    return get_item(handle, JEKV_TYPE_BLOB, key, blob, blob_len);
}

int jekv_set_blob(jekv_handle_t handle, const char *key, const void *blob, uint32_t blob_len)
{
    return set_item(handle, JEKV_TYPE_BLOB, key, blob, blob_len);
}

int jekv_set_i8(jekv_handle_t handle, const char *key, int8_t value)
{
    return set_item(handle, JEKV_TYPE_INT8, key, &value, sizeof(value));
}

int jekv_set_u8(jekv_handle_t handle, const char *key, uint8_t value)
{
    return set_item(handle, JEKV_TYPE_UINT8, key, &value, sizeof(value));
}

int jekv_set_i16(jekv_handle_t handle, const char *key, int16_t value)
{
    return set_item(handle, JEKV_TYPE_INT16, key, &value, sizeof(value));
}

int jekv_set_u16(jekv_handle_t handle, const char *key, uint16_t value)
{
    return set_item(handle, JEKV_TYPE_UINT16, key, &value, sizeof(value));
}

int jekv_set_i32(jekv_handle_t handle, const char *key, int32_t value)
{
    return set_item(handle, JEKV_TYPE_INT32, key, &value, sizeof(value));
}

int jekv_set_u32(jekv_handle_t handle, const char *key, uint32_t value)
{
    return set_item(handle, JEKV_TYPE_UINT32, key, &value, sizeof(value));
}

int jekv_set_i64(jekv_handle_t handle, const char *key, int64_t value)
{
    return set_item(handle, JEKV_TYPE_INT64, key, &value, sizeof(value));
}

int jekv_set_u64(jekv_handle_t handle, const char *key, uint64_t value)
{
    return set_item(handle, JEKV_TYPE_UINT64, key, &value, sizeof(value));
}

int jekv_set_float(jekv_handle_t handle, const char *key, double value)
{
    return set_item(handle, JEKV_TYPE_DOUBLE, key, &value, sizeof(value));
}

int jekv_get_i8(jekv_handle_t handle, const char *key, int8_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_INT8, key, out_value, &length);
}

int jekv_get_u8(jekv_handle_t handle, const char *key, uint8_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_UINT8, key, out_value, &length);
}

int jekv_get_i16(jekv_handle_t handle, const char *key, int16_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_INT16, key, out_value, &length);
}

int jekv_get_u16(jekv_handle_t handle, const char *key, uint16_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_UINT16, key, out_value, &length);
}

int jekv_get_i32(jekv_handle_t handle, const char *key, int32_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_INT32, key, out_value, &length);
}

int jekv_get_u32(jekv_handle_t handle, const char *key, uint32_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_UINT32, key, out_value, &length);
}

int jekv_get_i64(jekv_handle_t handle, const char *key, int64_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_INT64, key, out_value, &length);
}

int jekv_get_u64(jekv_handle_t handle, const char *key, uint64_t *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_UINT64, key, out_value, &length);
}

int jekv_get_float(jekv_handle_t handle, const char *key, double *out_value)
{
    uint32_t length = sizeof(*out_value);
    return get_item(handle, JEKV_TYPE_DOUBLE, key, out_value, &length);
}

int jekv_del_key(jekv_handle_t handle, const char *key)
{
    int err;
    jekv_handle_info_t *h = (jekv_handle_info_t *)handle;

    if (!(handle && key)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (!jekv_ptm_is_handle_valid(h)) {
        err = JEKV_ERR_INVALID_HANDLE;
    } else if (h->mode == JEKV_OP_READ_ONLY) {
        err = JEKV_ERR_READ_ONLY;
    } else {
        err = jekv_storage_del_item(h->storage, h->group_id, (jekv_type_t)(JEKV_TYPE_ANY_WITHOUT_SEG), key);
    }

    JEKV_UNLOCK();

    jekv_log_debug("del %s,err=%d", key, err);

    return err;
}

int jekv_del_group(jekv_handle_t handle)
{
    int err;
    jekv_handle_info_t *h = (jekv_handle_info_t *)handle;

    if (!(handle && h->valid)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (!jekv_ptm_is_handle_valid(h)) {
        err = JEKV_ERR_INVALID_HANDLE;
    } else if (h->mode == JEKV_OP_READ_ONLY) {
        err = JEKV_ERR_READ_ONLY;
    } else {
        err = jekv_storage_del_group(h->storage, h->group_id);
    }

    JEKV_UNLOCK();

    jekv_log_debug("del all group, id=%d", h->group_id);

    return err;
}

int jekv_entry_find(const char *partition_name, const char *group, jekv_type_t type, jekv_iterator_t *output_iterator)
{
    int err;

    if (!(partition_name && group && (int)type >= JEKV_TYPE_ANY && (int)type < JEKV_TYPE_MAX && output_iterator)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (type == JEKV_TYPE_ANY) {
        type = (jekv_type_t)JEKV_TYPE_ANY_WITHOUT_SEG;
    }

    err = jekv_iterator_find(partition_name, group, type, output_iterator);

    JEKV_UNLOCK();

    return err;
}

int jekv_entry_find_by_handle(jekv_handle_t handle, jekv_type_t type, jekv_iterator_t *output_iterator)
{
    int err;

    if (!(handle && (int)type >= JEKV_TYPE_ANY && (int)type < JEKV_TYPE_MAX && output_iterator)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    if (jekv_ptm_is_handle_valid((jekv_handle_info_t *)handle)) {
        if (type == JEKV_TYPE_ANY) {
            type = (jekv_type_t)JEKV_TYPE_ANY_WITHOUT_SEG;
        }
        err = jekv_iterator_find_by_handle(handle, type, output_iterator);
    } else {
        err = JEKV_ERR_INVALID_HANDLE;
    }

    JEKV_UNLOCK();

    return err;
}

int jekv_entry_next(jekv_iterator_t *iterator)
{
    int err;

    if (!(iterator)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_iterator_next(iterator);

    JEKV_UNLOCK();

    return err;
}

int jekv_entry_info(jekv_iterator_t iterator, jekv_entry_t *info)
{
    int err;

    if (!(iterator && info)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_iterator_info(iterator, info);

    JEKV_UNLOCK();

    return err;
}

int jekv_entry_data(jekv_iterator_t iterator, void *data, uint32_t *data_len)
{
    int err;

    if (!(iterator && data && data_len && *data_len > 0)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_iterator_data(iterator, data, data_len);

    JEKV_UNLOCK();

    return err;
}

int jekv_release_iterator(jekv_iterator_t iterator)
{
    int err;

    if (!(iterator)) {
        return JEKV_ERR_OK;
    }

    JEKV_LOCK();

    err = jekv_iterator_release(iterator);

    JEKV_UNLOCK();

    return err;
}

int jekv_print(const char *partition_name, const char *group_name)
{
    int err;

    if (!partition_name) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_iterator_print(partition_name, group_name);

    JEKV_UNLOCK();

    return err;
}

int jekv_get_info(jekv_handle_t handle, const char *key, jekv_type_t *type, uint32_t *size)
{
    int err;
    jekv_handle_info_t *h = (jekv_handle_info_t *)handle;
    jekv_item_t item;

    if (!(handle && key && type && size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_storage_find_key(h->storage, h->group_id, key, &item);
    if (err == JEKV_ERR_OK) {
        *type = (jekv_type_t)item.type;

        if (item.type == JEKV_TYPE_BLOB) {
            *size = item.all_size;
        } else {
            *size = item.length;
        }
    }

    JEKV_UNLOCK();

    jekv_log_debug("get info %s, size=%u, err=%d", key, ((!err) ? *size : 0), err);

    return err;
}

int jekv_get_status(const char *partition_name, jekv_status_t *status)
{
    int err;

    if (!(partition_name && status)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    JEKV_LOCK();

    err = jekv_ptm_get_status(partition_name, status);

    JEKV_UNLOCK();

    return err;
}
