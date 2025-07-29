#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_ptm"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_partition_manager.h"
#include "jekv_handler.h"
#include "jekv_log.h"

/**
 * @brief   storage list
 */
static struct dl_list g_storage_list = DL_LIST_HEAD_INIT(g_storage_list);

/**
 * @brief   handle list
 */
static struct dl_list g_handle_list = DL_LIST_HEAD_INIT(g_handle_list);

int jekv_ptm_is_in_using(void)
{
    return !dl_list_empty(&g_storage_list);
}

struct dl_list *jekv_get_storage_list(void)
{
    return &g_storage_list;
}

struct dl_list *jekv_get_handle_list(void)
{
    return &g_handle_list;
}

jekv_storage_t *jekv_ptm_find_storage(const char *partition_name)
{
    jekv_storage_t *entry = NULL;

    /*look up storage from storage list*/
    dl_list_for_each(entry, &g_storage_list, jekv_storage_t, list)
    {
        if (!strcmp(entry->pt.name, partition_name)) {
            return entry;
        }
    }
    return NULL;
}

int jekv_ptm_init(const char *partition_name)
{
    int err;
    jekv_storage_t *storage = NULL;
    jekv_partition_t pt;

    /*check init before*/
    storage = jekv_ptm_find_storage(partition_name);
    if (storage) {
        jekv_log_warning("Aready init");
        return JEKV_ERR_AREADY_INIT;
    }

    /*init partition*/
    err = jekv_pt_init(partition_name, &pt);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /*init storage*/
    err = jekv_storage_init(&pt, &storage);
    if (err == JEKV_ERR_OK) {
        dl_list_add_tail(&g_storage_list, &storage->list);
    }

    jekv_log_info("ptm init, err=%d", err);

    return err;
}

int jekv_ptm_deinit(const char *partition_name)
{
    int err;

    jekv_storage_t *storage   = NULL;
    jekv_handle_info_t *entry = NULL;
    jekv_handle_info_t *next;

    if (!g_storage_list.next) {
        jekv_log_error("%s","Not Init");
        return JEKV_ERR_FAIL;
    }

    /*remove storage from list*/
    storage = jekv_ptm_find_storage(partition_name);
    if (!storage) {
        jekv_log_error("%s","Not found");
        return JEKV_ERR_NOT_FOUND;
    }

    /*delete all handle*/
    dl_list_for_each_safe(entry, next, &g_handle_list, jekv_handle_info_t, list)
    {
        if (entry->storage == storage) {
            jekv_log_debug("Destroy handle: %p, group=%d", entry, entry->group_id);

            dl_list_del(&entry->list);
            jekv_handler_close(entry);
        }
    }

    /*deteach from list*/
    dl_list_del(&storage->list);

    /* deinit storage*/
    err = jekv_storage_deinit(storage);

    /*free storage*/
    JEKV_FREE(storage);

    jekv_log_info("ptm deinit, err=%d", err);

    return err;
}

int jekv_ptm_open_handle(const char *partition_name, const char *group, jekv_open_mode_t mode,
                           jekv_handle_info_t **handle)
{
    int err;
    jekv_storage_t *storage = NULL;
    uint8_t group_id;

    /*check init*/
    if (dl_list_empty(&g_storage_list)) {
        jekv_log_error("%s","not init");
        return JEKV_ERR_NOT_INIT;
    }

    if (dl_list_len(&g_handle_list) >= JEKV_MAX_HANDLE_NUM) {
        return JEKV_ERR_FAIL;
    }

    /* find partition*/
    storage = jekv_ptm_find_storage(partition_name);
    if (!storage) {
        jekv_log_warning("%s not init", partition_name);
        return JEKV_ERR_NOT_FOUND;
    }

    /* check write allowed*/
    if (mode == JEKV_OP_READ_WRITE && storage->pt.readonly) {
        jekv_log_warning("%s can't be written", partition_name);
        return JEKV_ERR_READ_ONLY;
    }

    /* get group id*/
    err = jekv_storage_open_group(storage, group, true, &group_id);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /* create handler*/
    err = jekv_handler_open(storage, group_id, mode, handle);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /* add to handler list */
    dl_list_add_tail(&g_handle_list, &(*handle)->list);

    jekv_log_debug("open %p", handle);

    return JEKV_ERR_OK;
}

int jekv_ptm_close_handle(jekv_handle_info_t *handle)
{
    jekv_handle_info_t *entry = NULL;
    jekv_handle_info_t *next;

    jekv_log_debug("close %p", handle);

    /*look up storage from storage list*/
    dl_list_for_each_safe(entry, next, &g_handle_list, jekv_handle_info_t, list)
    {
        if (entry == handle) {
            dl_list_del(&entry->list);
            return jekv_handler_close(handle);
        }
    }
    return JEKV_ERR_INVALID_HANDLE;
}

bool jekv_ptm_is_handle_valid(jekv_handle_info_t *handle)
{
    bool found = false;

    jekv_handle_info_t *entry = NULL;

    /*look up storage from storage list*/
    dl_list_for_each(entry, &g_handle_list, jekv_handle_info_t, list)
    {
        if (entry == handle) {
            found = (entry->valid ? true : false);
            break;
        }
    }

    return found;
}

int jekv_ptm_get_status(const char *partition_name, jekv_status_t *status)
{
    jekv_handle_info_t *entry = NULL;
    jekv_storage_t *storage   = jekv_ptm_find_storage(partition_name);

    if (!storage) {
        return JEKV_ERR_NOT_INIT;
    }

    memset(status, 0, sizeof(*status));

    status->group_num = dl_list_len(&storage->group_list);

    /*look up handler list*/
    dl_list_for_each(entry, &g_handle_list, jekv_handle_info_t, list)
    {
        if (entry->storage == storage) {
            status->handle_num++;
        }
    }

    return jekv_sm_get_status(&storage->sm, status);
}
