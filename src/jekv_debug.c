#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define LOG_TAG "jekv_debug"
#include "jekv_porting.h"
#include "jekv_partition.h"
#include "jekv_storage.h"
#include "jekv_partition_manager.h"
#include "jekv_base.h"
#include "jekv_debug.h"
#include "jekv_log.h"

#define JEKV_DEBUG_HANDLER_NUM (JEKV_MAX_HANDLE_NUM + 2)

static jekv_handle_t g_cmd_handle[JEKV_DEBUG_HANDLER_NUM];

int jekv_debug_print_sector(const char *partition_name, int index, int size)
{
    int err;

    jekv_storage_t * st = jekv_ptm_find_storage(partition_name);
    jekv_partition_t* pt = NULL;
    uint8_t *sec     = NULL;
    uint32_t offset;

    if(!st){
        return JEKV_ERR_FAIL;
    }

    sec = JEKV_MALLOC(JEKV_SECTOR_SIZE);
    if (!sec) {
        return JEKV_ERR_NO_MEM;
    }

    pt = &st->pt;
    offset = pt->offset + index * JEKV_SECTOR_SIZE;

    err = jekv_partition_read(pt->dev,offset,sec, size);

    jekv_log_error("read off[%d]=0x%x", index, offset);

    JEKV_DUMPE("sec", 32, sec, size);

    JEKV_FREE(sec);

    return err;
}

int jekv_debug_print_status(const char *partition_name)
{
    jekv_status_t status;
    int err;

    err = jekv_get_status(partition_name, &status);
    if (err == JEKV_ERR_OK) {
        jekv_log_error("total_size=0x%x, %u", status.total_size, status.total_size);
        jekv_log_error("using_size=0x%x, %u", status.using_size, status.using_size);
        jekv_log_error("droped_size=0x%x, %u", status.droped_size, status.droped_size);
        jekv_log_error("free_size=0x%x, %u", status.free_size, status.free_size);
        jekv_log_error("group_num=%d", status.group_num);
        jekv_log_error("handle_num=%d", status.handle_num);
    } else {
        jekv_log_error("get status err=%d", err);
    }

    return err;
}

static int jekv_debug_jekv_get_user_id(jekv_handle_t handle)
{
    int ret = -1;

    for (int i = 0; i < JEKV_DEBUG_HANDLER_NUM; i++) {
        if (g_cmd_handle[i] == handle) {
            ret = i;
            break;
        }
    }

    return ret;
}

int jekv_debug_print_storage(int storage_detail, int sec_detail)
{
    jekv_storage_t *it;
    jekv_sector_t *sec;
    jekv_group_t *group;
    jekv_handle_info_t *hinfo = NULL;

    struct dl_list *storage     = jekv_get_storage_list();
    struct dl_list *handle_list = jekv_get_handle_list();

    if (dl_list_empty(storage)) {
        jekv_log_error("%s","Not init!");
        return JEKV_ERR_NOT_INIT;
    }

    /*look up storage from storage list*/
    dl_list_for_each(it, storage, jekv_storage_t, list)
    {
        jekv_log_error("%s: offset=[%d,0x%x], sec_num=%u, readonly=%d", it->pt.name, it->pt.offset / JEKV_SECTOR_SIZE,
                    it->pt.offset, it->pt.sec_num, it->pt.readonly);

        if (storage_detail) {
            jekv_log_error("active : %d", dl_list_len(&it->sm.active));
            jekv_log_error("idle : %d\r\n", dl_list_len(&it->sm.idle));
            jekv_log_error("serial_number %u", it->sm.serial_number);

            jekv_debug_print_status(it->pt.name);

            /*look up sgroup list*/
            JEKV_RAWE("group list:\r\n");

            dl_list_for_each(group, &it->group_list, jekv_group_t, list)
            {
                JEKV_RAWE("group id=%02d : %s\r\n", group->id, group->name);

                dl_list_for_each(hinfo, handle_list, jekv_handle_info_t, list)
                {
                    int use_id = jekv_debug_jekv_get_user_id((jekv_handle_t)hinfo);

                    if (hinfo->group_id == group->id) {
                        JEKV_RAWE("   handle %p: valid=%d,mode=%d, user_id=%d\r\n", hinfo, hinfo->valid, hinfo->mode,
                                        use_id);
                    }
                }
            }

            JEKV_RAWE("\r\n");

            if (sec_detail) {
                JEKV_RAWE("active sec list:\r\n");

                /*look up active sector list*/

                dl_list_for_each(sec, &it->sm.active, jekv_sector_t, list)
                {
                    int free_slice = JEKV_ENTRY_COUNT - sec->used_slice;
                    JEKV_RAWE("state=%x, sec index=[%u], sn=%d, next=%d, used=%d, droped=%d, left=[%u,%u]\r\n",
                                    sec->state, sec->address / JEKV_SECTOR_SIZE, sec->serial_number, sec->next_free_slice,
                                    sec->used_slice, sec->droped_slice, free_slice, free_slice * JEKV_SLICE_SIZE);

                    if (sec_detail > 2) {
                        jekv_hash_t *h = &sec->hash;

                        JEKV_RAWE("   hash list:\r\n");
                        for (int i = 0; i < h->count; i++) {
                            if (h->hash_table[i].index != JEKV_HASH_INVALID) {
                                JEKV_RAWE("       %d: %d-0x%06x", i, h->hash_table[i].index, h->hash_table[i].hash);
                                if ((i + 1) % 5 == 0) {
                                    JEKV_RAWE("\r\n");
                                }
                            }
                        }
                        JEKV_RAWE("\r\n");
                    }
                }
            }

            JEKV_RAWE("\r\n");
        }
    }

    return JEKV_ERR_OK;
}

int jekv_debug_print_handle(void)
{
    jekv_handle_info_t *it;
    int cnt = 0;
    struct dl_list *handle_list;

    handle_list = jekv_get_handle_list();

    JEKV_RAWE("handler list:\r\n");

    dl_list_for_each(it, handle_list, jekv_handle_info_t, list)
    {
        JEKV_RAWE("handler %d,%p: pt=%s, group_id=%d,valid=%d,mode=%d\r\n", cnt++, it, it->storage->pt.name, it->group_id,
                        it->valid, it->mode);
    }

    return JEKV_ERR_OK;
}

int jekv_debug_jekv_open(const char *partition, const char *group, jekv_open_mode_t mode)
{
    jekv_handle_t handle = NULL;

    int err = jekv_open(partition, group, mode, &handle);
    if (err != JEKV_ERR_OK) {
        jekv_log_error("open kv %s %s %d,err=%d", partition, group, mode, err);
        return err;
    }

    for (int i = 0; i < JEKV_DEBUG_HANDLER_NUM; i++) {
        if (!g_cmd_handle[i]) {
            g_cmd_handle[i] = handle;
            jekv_log_error("open kv %s %s %d OK, handle_index=%d,handle=%p", partition, group, mode, i, handle);
            return JEKV_ERR_OK;
        }
    }

    return JEKV_ERR_FAIL;
}

int jekv_debug_jekv_close(int id)
{
    if (id >= 0 && id < JEKV_DEBUG_HANDLER_NUM && g_cmd_handle[id]) {
        int err = jekv_close(g_cmd_handle[id]);
        if (err == JEKV_ERR_OK) {
            g_cmd_handle[id] = NULL;
        }
        jekv_log_error("close %d, %p, err=%d", id, g_cmd_handle[id], err);
        return err;
    } else {
        jekv_log_error("not open %d", id);
        return JEKV_ERR_INVALID_HANDLE;
    }
}

jekv_handle_t jekv_debug_jekv_get_handle(int id)
{
    jekv_handle_t handle = NULL;

    if (id >= 0 && id < JEKV_DEBUG_HANDLER_NUM && g_cmd_handle[id]) {
        handle = g_cmd_handle[id];
    } else {
        jekv_log_error("the handler index=%d not opend", id);
    }

    return handle;
}

bool jekv_debug_jekv_check_handle_id(int id)
{
    if (!(id >= 0 && id < JEKV_DEBUG_HANDLER_NUM && g_cmd_handle[id])) {
        jekv_log_error("the handler index=%d not opend", id);
        return false;
    }
    return true;
}

int jekv_debug_clear_partiton_handle(const char *partition)
{
    jekv_handle_info_t *info;

    for (int i = 0; i < JEKV_DEBUG_HANDLER_NUM; i++) {
        info = (jekv_handle_info_t *)g_cmd_handle[i];
        if (info) {
            if (!strcmp(info->storage->pt.name, partition)) {
                jekv_log_debug("clear %d", i);
                g_cmd_handle[i] = NULL;
            }
        }
    }

    return JEKV_ERR_OK;
}

void jekv_debug_print_value(const char *key, jekv_type_t type, const void *p, uint32_t length)
{
    if (!(key && p)) {
        return;
    }

    switch (type) {
        case JEKV_TYPE_STRING:
        {
            jekv_log_error("%s=%s", key, (length > 0 ? (char *)p : ""));
            break;
        }
        case JEKV_TYPE_INT32:
        {
            jekv_log_error("%s=%d", key, *((int *)p));
            break;
        }
        case JEKV_TYPE_UINT32:
        {
            jekv_log_error("%s=%u", key, *((unsigned int *)p));
            break;
        }
        case JEKV_TYPE_INT8:
        {
            jekv_log_error("%s=%d", key, *((signed char *)p));
            break;
        }
        case JEKV_TYPE_INT16:
        {
            jekv_log_error("%s=%d", key, *((short *)p));
            break;
        }
        case JEKV_TYPE_UINT8:
        {
            jekv_log_error("%s=%u", key, *((unsigned char *)p));
            break;
        }
        case JEKV_TYPE_UINT16:
        {
            jekv_log_error("%s=%u", key, *((unsigned short *)p));
            break;
        }
        case JEKV_TYPE_INT64:
        {
            jekv_log_error("%s=%" PRId64, key, (int64_t)(*((int64_t *)p)));
            break;
        }
        case JEKV_TYPE_UINT64:
        {
            jekv_log_error("%s=%" PRIu64, key, (uint64_t)(*((uint64_t *)p)));
            break;
        }
        case JEKV_TYPE_DOUBLE:
        {
            jekv_log_error("%s=%lf", key, *((double *)p));
            break;
        }
        case JEKV_TYPE_BINARY:
        case JEKV_TYPE_BLOB:
        {
            JEKV_DUMPE(key, 32, p, length);
            break;
        }
        default:
        {
            jekv_log_error("error type");
            break;
        }
    }
}

int jekv_debug_cmd_get_item(int id, const char *key)
{
    jekv_type_t type;
    uint32_t size;
    int err;
    unsigned char *buf = NULL;

    if (!(id >= 0 && id < JEKV_DEBUG_HANDLER_NUM && g_cmd_handle[id])) {
        jekv_log_error("the handler index=%d not opend", id);
        return JEKV_ERR_INVALID_HANDLE;
    }

    err = jekv_get_info(g_cmd_handle[id], key, &type, &size);
    if (err == JEKV_ERR_OK) {
        if (type == JEKV_TYPE_STRING || type == JEKV_TYPE_BLOB || JEKV_TYPE_BINARY) {
            buf = JEKV_MALLOC(size);

            if (!buf) {
                jekv_log_error("malloc fail,size=%d\n", (int)size);
                return JEKV_ERR_NO_MEM;
            }
        }

        jekv_log_error("get %s ,type=0x%x,len=%u", key, type, size);

        switch (type) {
            case JEKV_TYPE_STRING:
            {
                err = jekv_get_str(g_cmd_handle[id], key, (char *)buf, &size);
                if (!err) {
                    jekv_log_error("%s=%s", key, (char *)buf);
                }
                break;
            }
            case JEKV_TYPE_INT32:
            {
                int32_t v;
                err = jekv_get_i32(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%d", key, v);
                }
                break;
            }
            case JEKV_TYPE_UINT32:
            {
                uint32_t v;
                err = jekv_get_u32(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%u", key, v);
                }
                break;
            }
            case JEKV_TYPE_INT8:
            {
                int8_t v;
                err = jekv_get_i8(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%d", key, v);
                }
                break;
            }
            case JEKV_TYPE_INT16:
            {
                int16_t v;
                err = jekv_get_i16(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%d", key, v);
                }
                break;
            }
            case JEKV_TYPE_UINT8:
            {
                uint8_t v;
                err = jekv_get_u8(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%u", key, v);
                }
                break;
            }
            case JEKV_TYPE_UINT16:
            {
                uint16_t v;
                err = jekv_get_u16(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%u", key, v);
                }
                break;
            }
            case JEKV_TYPE_INT64:
            {
                int64_t v;
                err = jekv_get_i64(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%" PRId64, key, v);
                }
                break;
            }
            case JEKV_TYPE_UINT64:
            {
                uint64_t v;
                err = jekv_get_u64(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%" PRIu64, key, v);
                }
                break;
            }
            case JEKV_TYPE_DOUBLE:
            {
                double v;
                err = jekv_get_float(g_cmd_handle[id], key, &v);
                if (!err) {
                    jekv_log_error("%s=%lf", key, v);
                }
                break;
            }
            case JEKV_TYPE_BINARY:
            {
                err = jekv_get_binary(g_cmd_handle[id], key, buf, &size);
                if (!err) {
                    JEKV_DUMPE(key, 32, buf, size);
                }
                break;
            }
            case JEKV_TYPE_BLOB:
            {
                jekv_log_error("start get blob %s", key);

                err = jekv_get_blob(g_cmd_handle[id], key, buf, &size);
                if (!err) {
                    JEKV_DUMPE(key, 32, buf, size);
                }
                break;
            }
            default:
            {
                jekv_log_error("error type");
                break;
            }
        }

    } else if (err == JEKV_ERR_NOT_FOUND) {
        jekv_log_error("%s not exist.", key);
    }

    if (buf) {
        JEKV_FREE(buf);
    }

    return err;
}

int jekv_debug_set_item_with_size(int id, const char *key, const char *str_size, jekv_type_t type)
{
    int len;
    int err = JEKV_ERR_INVALID_PARAM;
    int i;
    uint8_t *p = NULL;

    if (jekv_debug_jekv_check_handle_id(id)) {
        len = strtoul(str_size, NULL, 0);

        if (len > 0) {
            p = JEKV_MALLOC(len);

            if (p) {
                for (i = 0; i < len; i++) {
                    p[i] = (uint8_t)(0x30 + (i % 10));
                }

                p[len - 1] = 0;

                if (type == JEKV_TYPE_STRING) {
                    err = jekv_set_str(g_cmd_handle[id], key, (char *)p);
                } else if (type == JEKV_TYPE_BINARY) {
                    err = jekv_set_binary(g_cmd_handle[id], key, (char *)p, len);
                } else if (type == JEKV_TYPE_BLOB) {
                    err = jekv_set_blob(g_cmd_handle[id], key, (char *)p, len);
                } else {
                    jekv_log_error("bad type,type=%d", type);
                }

                JEKV_FREE(p);
            } else {
                err = JEKV_ERR_NO_MEM;
                jekv_log_error("no memory");
            }
        }
    }

    return err;
}

static int jekv_debug_check_int_float(const char *type, const char *value)
{
    int err = JEKV_ERR_OK;
    int32_t v32;

    errno = 0;

    if (!strcmp(type, "U8") || !strcmp(type, "u8")) {
        v32 = strtoul(value, NULL, 0);
        if (v32 < 0 || v32 > 0xff || errno != 0) {
            err = JEKV_ERR_INVALID_PARAM;
        }

    } else if (!strcmp(type, "I8") || !strcmp(type, "i8")) {
        v32 = strtoul(value, NULL, 0);
        if (v32 < -0x80 || v32 > 0x7f || errno != 0) {
            err = JEKV_ERR_INVALID_PARAM;
        }

    } else if (!strcmp(type, "U16") || !strcmp(type, "u16")) {
        v32 = strtoul(value, NULL, 0);
        if (v32 < 0 || v32 > 0xffff || errno != 0) {
            err = JEKV_ERR_INVALID_PARAM;
        }

    } else if (!strcmp(type, "I16") || !strcmp(type, "i16")) {
        v32 = strtoul(value, NULL, 0);
        if (v32 < -0x8000 || v32 > 0x7fff || errno != 0) {
            err = JEKV_ERR_INVALID_PARAM;
        }

    } else if (!strcmp(type, "I32") || !strcmp(type, "i32")) {
        strtol(value, NULL, 0);
        if (!(errno == 0 || errno == ERANGE || INT32_MIN)) {
            err = JEKV_ERR_INVALID_PARAM;
        }

    } else if (!strcmp(type, "U32") || !strcmp(type, "u32")) {
        strtoul(value, NULL, 0);
        if (!(errno == 0 || errno == ERANGE || UINT32_MAX)) {
            err = JEKV_ERR_INVALID_PARAM;
        }
    } else if (!strcmp(type, "I64") || !strcmp(type, "i64")) {
        strtoll(value, NULL, 0);
        if (!(errno == 0 || errno == ERANGE || INT64_MIN)) {
            err = JEKV_ERR_INVALID_PARAM;
        }

    } else if (!strcmp(type, "U64") || !strcmp(type, "u64")) {
        strtoull(value, NULL, 0);
        if (!(errno == 0 || errno == ERANGE || UINT64_MAX)) {
            err = JEKV_ERR_INVALID_PARAM;
        }
    } else if (!strcmp(type, "FLOAT") || !strcmp(type, "float")) {
        strtof(value, NULL);
        if (errno != 0) {
            err = JEKV_ERR_INVALID_PARAM;
        }
    } else {
        jekv_log_error("bad type");
        err = JEKV_ERR_INVALID_PARAM;
    }

    return err;
}

int jekv_debug_set_base_item(int id, const char *key, const char *type, const char *value)
{
    int err = 0;

    jekv_handle_t handle;

    handle = jekv_debug_jekv_get_handle(id);
    if (!handle) {
        return JEKV_ERR_INVALID_HANDLE;
    }

    err = jekv_debug_check_int_float(type, value);
    if (err != JEKV_ERR_OK) {
        jekv_log_error("bad param value");
        return err;
    }

    if (!strcmp(type, "U8") || !strcmp(type, "u8")) {
        err = jekv_set_u8(handle, key, (uint8_t)strtoul(value, NULL, 0));

    } else if (!strcmp(type, "I8") || !strcmp(type, "i8")) {
        err = jekv_set_i8(handle, key, (uint8_t)strtol(value, NULL, 0));

    } else if (!strcmp(type, "U16") || !strcmp(type, "u16")) {
        err = jekv_set_u16(handle, key, (uint16_t)strtoul(value, NULL, 0));

    } else if (!strcmp(type, "I16") || !strcmp(type, "i16")) {
        err = jekv_set_i16(handle, key, (uint16_t)strtol(value, NULL, 0));

    } else if (!strcmp(type, "U32") || !strcmp(type, "u32")) {
        err = jekv_set_u32(handle, key, (uint32_t)strtoul(value, NULL, 0));

    } else if (!strcmp(type, "I32") || !strcmp(type, "i32")) {
        err = jekv_set_i32(handle, key, (uint32_t)strtol(value, NULL, 0));

    } else if (!strcmp(type, "U64") || !strcmp(type, "u64")) {
        err = jekv_set_u64(handle, key, (uint64_t)strtoull(value, NULL, 0));

    } else if (!strcmp(type, "I64") || !strcmp(type, "i64")) {
        err = jekv_set_i64(handle, key, (uint64_t)strtoll(value, NULL, 0));

    } else if (!strcmp(type, "FLOAT") || !strcmp(type, "float")) {
        err = jekv_set_float(handle, key, strtof(value, NULL));
    } else {
        jekv_log_error("bad type");
        err = JEKV_ERR_INVALID_PARAM;
    }
    jekv_log_error("write err=%d", err);

    return err;
}

int jekv_debug_del(int id, const char *key)
{
    int err = JEKV_ERR_INVALID_HANDLE;
    if (jekv_debug_jekv_check_handle_id(id)) {
        err = jekv_del_key(g_cmd_handle[id], key);
    }
    return err;
}

int jekv_debug_del_group(int id)
{
    int err = JEKV_ERR_INVALID_HANDLE;

    if (jekv_debug_jekv_check_handle_id(id)) {
        err = jekv_del_group(g_cmd_handle[id]);
    }

    return err;
}

void jekv_debug_cmd(int argc, char *argv[])
{
    int err = 0;

    if ((3 == argc || 4 == argc) && !strcmp(argv[1], "print")) {
        err = jekv_print(argv[2], argc == 4 ? argv[3] : NULL);

    } else if (3 == argc && !strcmp(argv[1], "erase")) {
        jekv_debug_clear_partiton_handle(argv[2]);

        err = jekv_deinit(argv[2]);

        jekv_log_error("deinit,err=%d", err);

        err = jekv_erase(argv[2]);

        jekv_log_error("erase partition,err=%d", err);

    } else if (5 == argc && !strcmp(argv[1], "open")) {
        jekv_debug_jekv_open(argv[2], argv[3], (jekv_open_mode_t)strtoul(argv[4], NULL, 0));

    } else if (3 == argc && !strcmp(argv[1], "close")) {
        jekv_debug_jekv_close(strtoul(argv[2], NULL, 0));

    } else if (4 == argc && !strcmp(argv[1], "get")) {
        jekv_debug_cmd_get_item(strtoul(argv[2], NULL, 0), argv[3]);

    } else if (5 == argc && !strcmp(argv[1], "ws")) {
        if (jekv_debug_jekv_check_handle_id(strtoul(argv[2], NULL, 0))) {
            jekv_set_str(g_cmd_handle[strtoul(argv[2], NULL, 0)], argv[3], argv[4]);
        }

    } else if (5 == argc && !strcmp(argv[1], "wslen")) {
        jekv_debug_set_item_with_size(strtoul(argv[2], NULL, 0), argv[3], argv[4], JEKV_TYPE_STRING);

    } else if (5 == argc && !strcmp(argv[1], "wbin")) {
        jekv_debug_set_item_with_size(strtoul(argv[2], NULL, 0), argv[3], argv[4], JEKV_TYPE_BINARY);

    } else if (5 == argc && !strcmp(argv[1], "wblob")) {
        jekv_debug_set_item_with_size(strtoul(argv[2], NULL, 0), argv[3], argv[4], JEKV_TYPE_BLOB);

    } else if (6 == argc && !strcmp(argv[1], "wbase")) {
        jekv_debug_set_base_item(strtoul(argv[2], NULL, 0), argv[3], argv[4], argv[5]);

    } else if (4 == argc && !strcmp(argv[1], "del")) {
        jekv_debug_del(strtoul(argv[2], NULL, 0), argv[3]);

    } else if (3 == argc && !strcmp(argv[1], "delgroup")) {
        jekv_debug_del_group(strtoul(argv[2], NULL, 0));

    } else if (argc >= 2 && !strcmp(argv[1], "debug")) {
        jekv_debug_print_storage(1, 2);

    } else if (argc == 5 && !strcmp(argv[1], "dump")) {
        jekv_debug_print_sector(argv[2], strtoul(argv[3], NULL, 0), strtoul(argv[4], NULL, 0));

    } else if (argc == 3 && !strcmp(argv[1], "status")) {
        jekv_debug_print_status(argv[2]);

    } else {
        goto usage;
    }

    if (err != JEKV_ERR_OK) {
        jekv_log_error("err=%d", err);
    }

    return;

usage:
    JEKV_RAWE("\njekv usage:\n"
                    "  print          : kv print    <partition>\n"
                    "  erase          : kv erase    <partition> \n"
                    "  open handle    : kv open     <partition> <group> <mode>\n"
                    "  close handle   : kv close    <handle_index>\n"
                    "  get item       : kv get      <handle_index> <key>\n"
                    "  del item       : kv del      <handle_index> <key>\n"
                    "  set string item: kv ws       <handle_index> <key> <value>\n"
                    "  set string len : kv wslen    <handle_index> <key> <len>\n"
                    "  set binary len : kv wbin     <handle_index> <key> <len>\n"
                    "  set blob       : kv wblob    <handle_index> <key> <len>\n"
                    "  set base item  : kv wbase    <handle_index> <key> <type=[u8,i8,u16,i16,u32,i32,u64,i64,float]> <value> "
                    "\n"
                    "  del group      : kv delgroup <handle_index> \n"
                    "  dump sector    : kv dump     <partition> <sec_index> <size>\n"
                    "  status         : kv status\n"
                    "  debug          : kv debug\n");

    return;
}
