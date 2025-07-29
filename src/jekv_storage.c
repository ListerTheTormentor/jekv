#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_store"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_storage.h"
#include "jekv_debug.h"
#include "jekv_log.h"

typedef struct {
    struct dl_list list;               /**< blob check list          */
    char name[JEKV_MAX_KEY_LEN + 1];   /**< blob desc name           */
    uint8_t group_id;                  /**< group id                 */
    uint8_t seg_count;                 /**< segment count            */
    uint8_t count_seg_cnt;             /**< calculated segment count */
    uint8_t seg_start;                 /**< segment start            */
    uint32_t desc_data_size;           /**< blob data all size       */
    uint32_t count_data_size;          /**< calculated segment size  */
} jekv_blob_into_t;

/*
find group id or find a free id for new group
*/
static int storage_find_group(jekv_storage_t *storage, const char *group, uint8_t *id)
{
    jekv_group_t *entry = NULL;
    uint32_t i;

    uint64_t id_map = 0;

    /* Look up storage from storage list */
    dl_list_for_each(entry, &storage->group_list, jekv_group_t, list)
    {
        if (!strcmp(entry->name, group)) {
            *id = entry->id;
            return JEKV_ERR_OK;
        }

        id_map |= ((uint64_t)1) << entry->id;
    }

    /* Get a free id */
    for (i = 1; i < JEKV_GROUP_ID_MAX; i++) {
        if (!(id_map & (((uint64_t)1) << i))) {
            break;
        }
    }

    *id = i;
    jekv_log_debug("get free id=%d", i);

    return JEKV_ERR_FAIL;
}

static int storage_add_group(jekv_storage_t *storage, uint8_t group_id, const char *group_name, int max_size)
{
    jekv_group_t *group_node;

    group_node = JEKV_CALLOC(1, sizeof(*group_node));
    if (!group_node) {
        return JEKV_ERR_NO_MEM;
    }

    /*init the group node and add it to list*/
    dl_list_init(&group_node->list);
    group_node->id = group_id;
    snprintf(group_node->name, sizeof(group_node->name), "%.*s", max_size, group_name);

    dl_list_add_tail(&storage->group_list, &group_node->list);

    return JEKV_ERR_OK;
}

static int storage_load_groups(jekv_storage_t *storage)
{
    int err;
    int item_index = 0;
    jekv_item_t item;

    struct dl_list *active = &storage->sm.active;

    jekv_sector_t *entry = NULL;

    /* Look up storage from storage list */
    dl_list_for_each(entry, active, jekv_sector_t, list)
    {
        item_index = 0;

        while (1) {
            err = jekv_sector_find_item(entry, JEKV_GROUP_ITSELF_ID, JEKV_TYPE_UINT8, NULL, &item_index, &item,
                                          JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);
            if (err == JEKV_ERR_OK) {
                storage_add_group(storage, item.data[0], item.name, JEKV_MAX_KEY_LEN);
                jekv_log_debug("add group %.*s", JEKV_MAX_KEY_LEN, item.name);

                item_index += jekv_item_get_span(&item);
            } else {
                break;
            }
        }
    }

    jekv_log_debug("%s","group load end\n");

    return JEKV_ERR_OK;
}

static int storage_cmp_find_item(jekv_storage_t *storage, jekv_sector_t *find_sector, int item_index,
                                 jekv_item_t *find_item, const void *data, uint32_t size)
{
    int err;

    if (!find_sector) {
        return JEKV_ERR_FAIL;
    }

    if (size != find_item->length) {
        return JEKV_ERR_FAIL;
    }

    if (size <= 8) {
        return memcmp(data, find_item->data, size) == 0 ? JEKV_ERR_OK : JEKV_ERR_FAIL;
    } else {
        uint8_t *p = JEKV_MALLOC(size);
        if (p) {
            err = jekv_pt_read(find_sector->pt, find_sector->address + (item_index + 2) * JEKV_SLICE_SIZE, p, size);
            jekv_log_debug("read data, index=%d,size=%d,err=%d", item_index, size, err);
            if (err == JEKV_ERR_OK) {
                err = (memcmp(data, p, size) == 0 ? JEKV_ERR_OK : JEKV_ERR_FAIL);
            }
            JEKV_FREE(p);
            return err;
        } else {
            return JEKV_ERR_FAIL;
        }
    }
}

static int storage_blob_init_info(jekv_storage_t *storage, struct dl_list *info)
{
    int err;
    struct dl_list *active = &storage->sm.active;
    jekv_sector_t *it    = NULL;
    int item_index;
    jekv_item_t item;
    jekv_blob_into_t *blob;

    dl_list_for_each(it, active, jekv_sector_t, list)
    {
        item_index = 0;

        while (1) {
            /*find blob descriptor*/
            err = jekv_sector_find_item(it, JEKV_GROUP_ID_ANY, JEKV_TYPE_BLOB, NULL, &item_index, &item,
                                          JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);
            if (err != JEKV_ERR_OK) {
                break;
            }

            blob = JEKV_MALLOC(sizeof(*blob));
            if (!blob) {
                return JEKV_ERR_NO_MEM;
            }

            /*record blob infomation*/

            dl_list_init(&blob->list);

            memcpy(blob->name, item.name, JEKV_MAX_KEY_LEN);
            blob->name[JEKV_MAX_KEY_LEN] = 0;

            blob->group_id       = item.group_id;
            blob->seg_count      = item.seg_count;
            blob->seg_start      = item.seg_start;
            blob->desc_data_size = item.all_size;

            blob->count_seg_cnt   = 0;
            blob->count_data_size = 0;

            dl_list_add_tail(info, &blob->list);

            /*goto next item*/
            item_index += jekv_item_get_span(&item);
        }
    }

    return JEKV_ERR_OK;
}

static int storage_blob_deinit_info(struct dl_list *info)
{
    jekv_sector_t *it = NULL;
    jekv_sector_t *next;

    dl_list_for_each_safe(it, next, info, jekv_sector_t, list)
    {
        dl_list_del(&it->list);
        JEKV_FREE(it);
    }

    return JEKV_ERR_OK;
}

static int storage_blob_check_match(jekv_storage_t *storage, struct dl_list *info)
{
    int err;
    struct dl_list *active = &storage->sm.active;
    jekv_sector_t *it    = NULL;
    int item_index;
    jekv_item_t item;
    jekv_blob_into_t *desc = NULL;
    jekv_blob_into_t *desc_next;
    jekv_sector_t *found_sec = NULL;

    jekv_log_debug("blob check match");

    if (!dl_list_len(info)) {
        jekv_log_debug("No blob desc");
        return JEKV_ERR_OK;
    }

    dl_list_for_each(it, active, jekv_sector_t, list)
    {
        item_index = 0;

        while (1) {
            /*find blob segments*/
            err = jekv_sector_find_item(it, JEKV_GROUP_ID_ANY, JEKV_TYPE_BLOB_SEG, NULL, &item_index, &item,
                                          JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);
            if (err != JEKV_ERR_OK) {
                break;
            }

            dl_list_for_each(desc, info, jekv_blob_into_t, list)
            {
                if (!strncmp(item.name, desc->name, JEKV_MAX_KEY_LEN) && item.group_id == desc->group_id &&
                    item.seg_id >= desc->seg_start &&
                    item.seg_id < (desc->seg_start == JEKV_SEG_START_VER_1 ? JEKV_SEG_START_ANY : JEKV_SEG_START_VER_1)) {
                    /*statistic segments count and segments size*/
                    jekv_log_debug("count seg: %.*s, desc=%s,seg_id=%d,gid=[%d,%d]", JEKV_MAX_KEY_LEN, item.name, desc->name,
                                 item.seg_id, item.group_id, desc->group_id);
                    desc->count_seg_cnt++;
                    desc->count_data_size += item.length;
                    break;
                }
            }

            /*goto next item*/
            item_index += jekv_item_get_span(&item);
        }
    }

    /*find and erase dismatch blob descriptor*/
    dl_list_for_each_safe(desc, desc_next, info, jekv_blob_into_t, list)
    {
        if (desc->count_seg_cnt != desc->seg_count || desc->count_data_size != desc->desc_data_size) {
            jekv_log_debug("blob mismatch,name=%s,count=%u,%u, size=%u,%u", desc->name, desc->count_seg_cnt, desc->seg_count,
                        desc->count_data_size, desc->desc_data_size);

            /*erase blob descriptor*/
            err = jekv_sm_find_item(&storage->sm, desc->group_id, JEKV_TYPE_BLOB, desc->name, &item_index, &found_sec,
                                      &item, JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);
            if (err == JEKV_ERR_OK) {
                jekv_log_debug("erase blob desc %s", desc->name);
                jekv_sector_erase_item(found_sec, item_index, &item, true);
            }

            /*remove from list and delete it*/
            dl_list_del(&desc->list);
            JEKV_FREE(desc);
        }
    }

    jekv_log_debug("blob check match end\n");

    return JEKV_ERR_OK;
}

static int storage_blob_check_drop(jekv_storage_t *storage, struct dl_list *info)
{
    int err;
    struct dl_list *active = &storage->sm.active;
    jekv_sector_t *it    = NULL;
    int item_index;
    jekv_item_t item;
    jekv_blob_into_t *desc = NULL;
    bool found;

    jekv_log_debug("blob check drop");

    dl_list_for_each(it, active, jekv_sector_t, list)
    {
        item_index = 0;

        while (1) {
            /*find blob segments*/
            err = jekv_sector_find_item(it, JEKV_GROUP_ID_ANY, JEKV_TYPE_BLOB_SEG, NULL, &item_index, &item,
                                          JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);
            if (err != JEKV_ERR_OK) {
                jekv_log_debug("no seg: 0x%x,slice_index=%d", it->address, item_index);
                break;
            }

            jekv_log_debug("find seg: %.*s,seg_id=%d", JEKV_MAX_KEY_LEN, item.name, item.seg_id);

            found = 0;
            dl_list_for_each(desc, info, jekv_blob_into_t, list)
            {
                /*look for the blob descriptor*/
                if (!strncmp(item.name, desc->name, strnlen(desc->name, JEKV_MAX_KEY_LEN)) &&
                    item.group_id == desc->group_id && item.seg_id >= desc->seg_start &&
                    item.seg_id < (desc->seg_start == JEKV_SEG_START_VER_1 ? JEKV_SEG_START_ANY : JEKV_SEG_START_VER_1)) {
                    /*match*/
                    found = 1;
                    break;
                }
            }

            jekv_log_debug("find seg: %.*s,seg_id=%d, found_desc=%d", JEKV_MAX_KEY_LEN, item.name, item.seg_id, found);

            if (!found) {
                /*the blob descriptor is droped, erase the segment*/
                jekv_sector_erase_item(it, item_index, &item, true);
            }

            item_index += jekv_item_get_span(&item);
        }
    }

    jekv_log_debug("blob check drop end\n");

    return JEKV_ERR_OK;
}

static int storage_blob_check(jekv_storage_t *storage)
{
    int err;
    struct dl_list info = DL_LIST_HEAD_INIT(info);

    jekv_log_debug("start blob check");

    /*init blob desc information*/
    err = storage_blob_init_info(storage, &info);
    if (err != JEKV_ERR_OK) {
        goto BLOB_CHECK_END;
    }

    jekv_log_debug("get blob info num=%d", dl_list_len(&info));

    /*check length and segments count match*/
    err = storage_blob_check_match(storage, &info);
    if (err != JEKV_ERR_OK) {
        goto BLOB_CHECK_END;
    }

    /*check and erase the droped segments*/
    err = storage_blob_check_drop(storage, &info);
    if (err != JEKV_ERR_OK) {
        goto BLOB_CHECK_END;
    }

BLOB_CHECK_END:

    jekv_log_debug("end blob check\n");

    /*delete blob info*/
    storage_blob_deinit_info(&info);

    return err;
}

int jekv_storage_init(jekv_partition_t *pt, jekv_storage_t **storage)
{
    int err;
    jekv_storage_t *store = NULL;

    store = JEKV_CALLOC(1, sizeof(*store));
    if (!store) {
        return JEKV_ERR_NO_MEM;
    }

    store->pt = *pt;
    dl_list_init(&store->group_list);

    /*sector manager load */
    err = jekv_sm_load(&store->sm, &store->pt);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /*load groups */
    err = storage_load_groups(store);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /* check blob data*/
    err = storage_blob_check(store);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    *storage = store;

    jekv_log_debug("pt=%s", store->pt.name);

    return JEKV_ERR_OK;
}

int jekv_storage_deinit(jekv_storage_t *storage)
{
    jekv_group_t *entry;
    jekv_group_t *next;

    /*delete groups*/
    dl_list_for_each_safe(entry, next, &storage->group_list, jekv_group_t, list)
    {
        jekv_log_debug("del group %s", entry->name);

        dl_list_del(&entry->list);

        JEKV_FREE(entry);
    }

    /*unload*/
    jekv_sm_unload(&storage->sm);

    return JEKV_ERR_OK;
}

int jekv_storage_open_group(jekv_storage_t *storage, const char *group, bool create_new, uint8_t *group_id)
{
    int err;

    /*check create before*/
    err = storage_find_group(storage, group, group_id);
    if (err == JEKV_ERR_OK) {
        /*found*/
        return JEKV_ERR_OK;
    }

    if (*group_id == JEKV_GROUP_ID_ANY) {
        return JEKV_ERR_NO_SPACE;
    }

    jekv_log_debug("open group %s", group);

    /*create new group*/
    err = jekv_storage_write_item(storage, JEKV_GROUP_ITSELF_ID, JEKV_TYPE_UINT8, group, group_id, sizeof(*group_id));
    if (err != JEKV_ERR_OK) {
        jekv_log_debug("open group,write fail %s", group);
        return err;
    }

    err = storage_add_group(storage, *group_id, group, strnlen(group, JEKV_MAX_KEY_LEN));

    return err;
}

int jekv_storage_erase_group_id(jekv_storage_t *storage, uint8_t group_id)
{
    int err;
    jekv_group_t *entry = NULL;
    jekv_group_t *next;

    /* Look up group list */
    dl_list_for_each_safe(entry, next, &storage->group_list, jekv_group_t, list)
    {
        if (entry->id == group_id) {
            err = jekv_storage_del_item(storage, JEKV_GROUP_ITSELF_ID, JEKV_TYPE_UINT8, entry->name);
            if (err == JEKV_ERR_OK) {
                dl_list_del(&entry->list);
                JEKV_FREE(entry);
            }
            return err;
        }
    }

    return JEKV_ERR_OK;
}

jekv_group_t *jekv_storage_find_group_by_name(jekv_storage_t *storage, const char *group_name)
{
    jekv_group_t *entry = NULL;

    /* Look up group list */
    dl_list_for_each(entry, &storage->group_list, jekv_group_t, list)
    {
        if (!strcmp(entry->name, group_name)) {
            return entry;
        }
    }

    return NULL;
}

jekv_group_t *jekv_storage_find_group_by_id(jekv_storage_t *storage, uint8_t group_id)
{
    jekv_group_t *entry = NULL;

    /* Look up group list */
    dl_list_for_each(entry, &storage->group_list, jekv_group_t, list)
    {
        if (entry->id == group_id) {
            return entry;
        }
    }

    return NULL;
}

static int storage_get_non_blob_write_req_size(jekv_type_t type, uint32_t size)
{
    int request_size = 0;

    if (type == JEKV_TYPE_STRING || type == JEKV_TYPE_BINARY) {
        if (size <= 8) {
            request_size = JEKV_SLICE_SIZE;
        } else {
            request_size = JEKV_SLICE_SIZE + size;
        }
    } else if (size < JEKV_SLICE_SIZE) {
        request_size = JEKV_SLICE_SIZE;
    } else {
        jekv_log_error("%s","error!");
        request_size = JEKV_SLICE_SIZE;
    }

    return request_size;
}

static int storage_write_blob_desc(jekv_sector_t *sec, uint8_t group_id, const char *key, uint32_t size, uint8_t seg_count,
                                   jekv_seg_start_t seg_start)
{
    jekv_item_t desc_item;

    desc_item.all_size  = size;
    desc_item.seg_count = seg_count;
    desc_item.seg_start = seg_start;
    desc_item.resv2     = 0xffff;

    jekv_log_debug("write desc, key=%s,size=%u,seg_count=%d,seg_start=%d", key, size, seg_count, seg_start);

    return jekv_sector_write_item(sec, group_id, JEKV_TYPE_BLOB, key, desc_item.data, sizeof(desc_item.data),
                                    JEKV_SEG_ID_ANY);
}

static int storage_cmp_blob(jekv_storage_t *storage, jekv_item_t *item, const void *data, uint32_t size)
{
    int seg_count = item->seg_count;
    int seg_start = item->seg_start;

    uint32_t offset = 0;
    jekv_item_t seg;
    jekv_sector_t *seg_sec;
    int seg_index;
    int i = 0;

    int err = JEKV_ERR_NOT_FOUND;

    uint8_t *pdata;

    /* check blob size */
    if (item->all_size != size) {
        jekv_log_debug("blob old:new=%u:%u", item->all_size, size);
        return JEKV_ERR_FAIL;
    }

    pdata = JEKV_MALLOC(JEKV_SINGLE_ITEM_MAX_DATA_SIZE);
    if (!pdata) {
        return JEKV_ERR_NO_MEM;
    }

    for (i = 0; i < seg_count; i++) {
        seg_index = 0;
        seg_sec   = NULL;

        /*look for segments data */
        err = jekv_sm_find_item(&storage->sm, item->group_id, (jekv_type_t)JEKV_TYPE_BLOB_SEG, item->name, &seg_index, &seg_sec, &seg,
                                  seg_start + i, (jekv_seg_start_t)seg_start);
        if (err != JEKV_ERR_OK) {
            jekv_log_debug("not found %d", i);
            break;
        }

        /*check segments data size */
        if (seg.length + offset > size) {
            jekv_log_debug("size out %u,%u", seg.length + offset, size);
            break;
        }

        /*read blob segments data*/
        err = jekv_sector_read_item_data(seg_sec, seg_index, &seg, pdata, seg.length);
        if (err != JEKV_ERR_OK) {
            break;
        }

        /*compare blob segments data*/
        if (memcmp(pdata, (uint8_t *)data + offset, seg.length)) {
            /*not same*/
            jekv_log_debug("seg same err");
            break;
        }

        offset += seg.length;
    }

    JEKV_FREE(pdata);

    if (i == seg_count && offset == size) {
        /*all segements same*/
        return JEKV_ERR_OK;
    } else {
        /*not same*/
        jekv_log_debug("blob cmp cnt=%u:%u,size=%u:%u", i, seg_count, offset, size);
        return JEKV_ERR_FAIL;
    }
}

static int storage_read_blob(jekv_storage_t *storage, jekv_item_t *item, void *data, uint32_t size)
{
    int seg_count = item->seg_count;
    int seg_start = item->seg_start;

    uint32_t offset = 0;
    jekv_item_t seg;
    jekv_sector_t *seg_sec;
    int seg_index;
    int i = 0;

    int err = JEKV_ERR_NOT_FOUND;

    for (i = 0; i < seg_count; i++) {
        seg_index = 0;
        seg_sec   = NULL;

        jekv_log_debug("read seg: %d,[seg_index=%d,seg_start=%d]", i, seg_start + i, seg_start);

        /*look for segments*/
        err = jekv_sm_find_item(&storage->sm, item->group_id, (jekv_type_t)JEKV_TYPE_BLOB_SEG, item->name, &seg_index, &seg_sec, &seg,
                                  seg_start + i, (jekv_seg_start_t)seg_start);
        if (err != JEKV_ERR_OK) {
            jekv_log_debug("find seg %d, err=%d", i, err);
            return err;
        }

        /*check segments length*/
        if (seg.length + offset > size) {
            jekv_log_debug("bad length");
            return JEKV_ERR_INVALID_LENGTH;
        }

        jekv_log_debug("start read seg %d, size=%d", i, seg.length);

        /*read segments data*/
        err = jekv_sector_read_item_data(seg_sec, seg_index, &seg, (uint8_t *)data + offset, seg.length);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        offset += seg.length;
    }

    jekv_log_debug("i=%d,seg_count=%d,offset=%d,size=%d", i, seg_count, offset, size);

    if (i == seg_count && offset == size) {
        /*read all segments data ok*/
        return JEKV_ERR_OK;
    } else {
        if (offset > 0) {
            return JEKV_ERR_INVALID_LENGTH;
        } else {
            return JEKV_ERR_NOT_FOUND;
        }
    }
}

static int storage_erase_blob(jekv_storage_t *storage, jekv_sector_t *find_sector, int found_index, jekv_item_t *item)
{
    int seg_count = item->seg_count;
    int seg_start = item->seg_start;

    jekv_item_t seg;
    jekv_sector_t *seg_sec;
    int seg_index;
    int i = 0;

    int err = JEKV_ERR_NOT_FOUND;

    /*delete blob descriptor*/
    jekv_log_debug("erase desc %.*s:%d ", JEKV_MAX_KEY_LEN, item->name, i);
    err = jekv_sector_erase_item(find_sector, found_index, item, true);

    JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_BLOB, JEKV_TRACE_AFTER_ERASE_OLD_DESC);

    for (i = 0; i < seg_count; i++) {
        seg_index = 0;
        seg_sec   = NULL;

        /*look for segments*/
        err = jekv_sm_find_item(&storage->sm, item->group_id, (jekv_type_t)JEKV_TYPE_BLOB_SEG, item->name, &seg_index, &seg_sec, &seg,
                                  seg_start + i, (jekv_seg_start_t)seg_start);
        if (err != JEKV_ERR_OK) {
            jekv_log_debug("find erase seg %.*s:%d err", JEKV_MAX_KEY_LEN, item->name, i);
            break;
        }

        /*delete segments*/
        jekv_log_debug("erase seg %.*s:%d ", JEKV_MAX_KEY_LEN, seg.name, i);
        jekv_sector_erase_item(seg_sec, seg_index, &seg, true);
    }

    return err;
}

static int storage_write_blob(jekv_storage_t *storage, uint8_t group_id, const char *key, const void *data, uint32_t dataSize,
                              jekv_seg_start_t seg_start)
{
    int err = JEKV_ERR_OK;

    jekv_sector_t *start_sec;
    jekv_sector_t *sec;
    int free_size;

    int left_size = dataSize;
    int offset    = 0;
    int cur_seg_size;

    uint8_t seg_id = seg_start;

    /*get current sector*/
    sec = jekv_sm_get_current_sector(&storage->sm);
    if (!sec) {
        jekv_log_error("%s","no valid sector");
        return JEKV_ERR_FAIL;
    }

    if ((sec->droped_slice > 0 && left_size + JEKV_SLICE_SIZE > jekv_sm_get_free_size(sec)) ||
        left_size > (JEKV_SEG_NUM_MAX - 1) * JEKV_SINGLE_ITEM_MAX_DATA_SIZE) {
        /*
        sector is dirty and need split: request a sector first, let the segments are written to the slimed secctors,
        so the free size and the gc size are same. then the check size and the write size are matched.
        */

        jekv_sm_request_sector(&storage->sm, JEKV_SLICE_SIZE);
        sec = jekv_sm_get_current_sector(&storage->sm);
    }

    start_sec = sec;

    while (1) {
        /*too many segments*/
        if (seg_id - seg_start >= JEKV_SEG_NUM_MAX && left_size > 0) {
            jekv_log_debug("too many segs");
            break;
        }

        /* get free size for write */
        free_size = jekv_sm_get_free_size(sec);

        jekv_log_debug("free_size=%d,left=%d", free_size, left_size);

        if (left_size > 0) {
            /*check data write*/
            if (left_size + JEKV_SLICE_SIZE <= free_size) {
                /*can write all left data*/
                jekv_log_debug("blob w: %d,left=%d", seg_id, left_size);
                err = jekv_sector_write_item(sec, group_id, JEKV_TYPE_BLOB_SEG, key, (uint8_t *)data + offset, left_size,
                                               seg_id++);
                if (err != JEKV_ERR_OK) {
                    return err;
                }
                left_size = 0;

                if (sec->next_free_slice + 1 <= JEKV_ENTRY_COUNT) {
                    jekv_log_debug("blob w: desc");

                    JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_BLOB, JEKV_TRACE_AFTER_WRITE_ALL_SEG);

                    return storage_write_blob_desc(sec, group_id, key, dataSize, seg_id - seg_start, (jekv_seg_start_t)seg_start);
                } else {
                    jekv_log_debug("blob w: skip desc");
                    /*need write desc next loop*/
                }
            } else {
                /* free space is larger than the size of minimum */

                if (free_size >= JEKV_BLOB_MIN_SEG_SIZE + JEKV_SLICE_SIZE) {
                    cur_seg_size = free_size - JEKV_SLICE_SIZE;
                    /*write_segment*/
                    jekv_log_debug("blob w: %d,left=%d", seg_id, cur_seg_size);
                    err = jekv_sector_write_item(sec, group_id, JEKV_TYPE_BLOB_SEG, key, (uint8_t *)data + offset,
                                                   cur_seg_size, seg_id++);
                    if (err != JEKV_ERR_OK) {
                        return err;
                    }
                    offset += cur_seg_size;
                    left_size -= cur_seg_size;

                    JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_BLOB, JEKV_TRACE_AFTER_WRITE_A_SEG);

                } else {
                    /*skip curren sector, it too small*/
                    jekv_log_debug("blob w: skip free_size=%d", free_size);
                }
            }

        } else if (free_size >= JEKV_SLICE_SIZE) {
            /*check write descriptor*/
            jekv_log_debug("blob w: desc");

            JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_BLOB, JEKV_TRACE_AFTER_WRITE_ALL_SEG);

            return storage_write_blob_desc(sec, group_id, key, dataSize, seg_id - seg_start, (jekv_seg_start_t)seg_start);
        }

        jekv_log_debug("blob write: request next sector, left=%d", left_size);

        /*request a sector*/
        err = jekv_sm_request_sector(&storage->sm, JEKV_SLICE_SIZE);
        if (err != JEKV_ERR_OK) {
            jekv_log_debug("%s","blob w: req fail");
            return JEKV_ERR_NO_SPACE;
        }

        /*get current sector*/
        sec = jekv_sm_get_current_sector(&storage->sm);
        if (!sec) {
            jekv_log_error("%s","no valid sector");
            return JEKV_ERR_FAIL;
        }

        if (sec == start_sec) {
            jekv_log_debug("%s","Cycle to start point");
            return JEKV_ERR_NO_SPACE;
        }
    }

    return JEKV_ERR_NO_SPACE;
}

int jekv_storage_write_item(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, const char *key,
                              const void *data, uint32_t size)
{
    int err;
    jekv_sector_t *find_sector = NULL;
    jekv_sector_t *cur_sector  = NULL;
    int found_item_index         = 0;
    int request_size;

    jekv_item_t item;
    jekv_seg_start_t seg_start = JEKV_SEG_START_VER_0;

    err = jekv_sm_find_item(&storage->sm, group_id, (jekv_type_t)JEKV_TYPE_ANY_WITHOUT_SEG, key, &found_item_index, &find_sector, &item,
                              JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);

    if (!(err == JEKV_ERR_OK || err == JEKV_ERR_NOT_FOUND)) {
        return err;
    }

    jekv_log_debug("find %s err=%d", key, err);

    if (type == JEKV_TYPE_BLOB) {
        /*compare old blob*/
        if (find_sector && type == item.type) {
            err = storage_cmp_blob(storage, &item, data, size);
            if (err == JEKV_ERR_OK) {
                jekv_log_debug("blob: found same, not write");
                return err;
            }

            /*find old blob*/
            if (item.seg_start == JEKV_SEG_START_VER_0) {
                seg_start = JEKV_SEG_START_VER_1;
            } else {
                seg_start = JEKV_SEG_START_VER_0;
            }

            jekv_log_debug("blob write: cmp old,err=%d,old_start=%d,new_start=%d", err, item.seg_start, seg_start);
        }

        /*check free space is enough*/
        err = jekv_sm_check_write_blob_size(&storage->sm, size);
        if (err != JEKV_ERR_OK) {
            jekv_log_error("blob write: fail,size=%u,err=%d", size, err);
            return err;
        }

        jekv_log_debug("%s","blob write: check space ok");

        /* write blob*/
        err = storage_write_blob(storage, group_id, key, data, size, seg_start);
        if (err != JEKV_ERR_OK) {
            jekv_log_debug("%s","write blob fail");
            return err;
        }

        jekv_log_debug("%s","blob write: OK");

    } else {
        /*same value data, not need write again*/
        if (find_sector && type == item.type) {
            err = storage_cmp_find_item(storage, find_sector, found_item_index, &item, data, size);
            if (err == JEKV_ERR_OK) {
                jekv_log_debug("%s","found same, do nothing");
                return err;
            }
            jekv_log_debug("cmp %s err=%d", key, err);
        }

        cur_sector = jekv_sm_get_current_sector(&storage->sm);
        if (!cur_sector) {
            jekv_log_error("%s","no valid sector");
            return JEKV_ERR_OK;
        }

        err = jekv_sector_write_item(cur_sector, group_id, type, key, data, size, JEKV_SEG_ID_ANY);
        if (err == JEKV_ERR_SECTOR_FULL) {
            /*full*/
            jekv_log_debug("%s","write cur sec full");

            request_size = storage_get_non_blob_write_req_size(type, size);

            /*request a sector*/
            err = jekv_sm_request_sector(&storage->sm, request_size);
            if (err != JEKV_ERR_OK) {
                return err;
            }

            /*get current sector*/
            cur_sector = jekv_sm_get_current_sector(&storage->sm);
            if (!cur_sector) {
                jekv_log_debug("%s","no valid sector");
                return JEKV_ERR_OK;
            }

            /*write item*/
            err = jekv_sector_write_item(cur_sector, group_id, type, key, data, size, JEKV_SEG_ID_ANY);
            if (err != JEKV_ERR_OK) {
                jekv_log_debug("write next sec, err=%d", err);
                return err;
            } else {
                jekv_log_debug("%s","write next sec ok");
            }

        } else if (err != JEKV_ERR_OK) {
            /*error*/
            jekv_log_debug("write error, err=%d", err);
            return err;
        } else {
            /*OK*/
            jekv_log_debug("%s","write to cur sec ok");
        }
    }

    if (find_sector) {
        if (item.type == JEKV_TYPE_BLOB) {
            JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_BLOB, JEKV_TRACE_AFTER_MODIFY_NEW);

            /*erase old blob data*/
            err = storage_erase_blob(storage, find_sector, found_item_index, &item);

            jekv_log_debug("blob erase old: err=%d", err);

        } else {
            /*erase old value*/
            JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_WRITE, JEKV_TRACE_BEFPRE_ERASE_OLD);

            err = jekv_sector_erase_item(find_sector, found_item_index, &item, true);

            jekv_log_debug("erase old type=%d: err=%d", item.type, err);
        }
    }

    return err;
}

int jekv_storage_read_item(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, const char *key, void *data,
                             uint32_t *size)
{
    int err;
    jekv_sector_t *find_sector = NULL;
    int found_item_index         = 0;
    uint32_t data_size;
    jekv_item_t item;

    err = jekv_sm_find_item(&storage->sm, group_id, type, key, &found_item_index, &find_sector, &item, JEKV_SEG_ID_ANY,
                              JEKV_SEG_START_ANY);
    if (err != JEKV_ERR_OK) {
        jekv_log_debug("read %s not found", key);
        return err;
    }

    /*get data size*/
    data_size = (type == JEKV_TYPE_BLOB ? item.all_size : item.length);

    /*check read size*/
    if (*size < data_size) {
        *size = data_size;
        return JEKV_ERR_VALUE_TOO_LONG;
    }

    *size = data_size;

    if (type == JEKV_TYPE_BLOB) {
        jekv_log_debug("%s","start read blob:");

        /*read blob segments*/
        err = storage_read_blob(storage, &item, data, data_size);

        if (err == JEKV_ERR_NOT_FOUND || err == JEKV_ERR_INVALID_LENGTH) {
            /*read blob segments fail, delete item*/
            jekv_sector_erase_item(find_sector, found_item_index, &item, true);
        }
    } else {
        /*read non blob items*/
        err = jekv_sector_read_item_data(find_sector, found_item_index, &item, data, data_size);
    }

    return err;
}

int jekv_storage_del_item(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, const char *key)
{
    int err;
    jekv_sector_t *find_sector = NULL;
    int found_item_index         = 0;

    jekv_item_t item;

    err = jekv_sm_find_item(&storage->sm, group_id, type, key, &found_item_index, &find_sector, &item, JEKV_SEG_ID_ANY,
                              JEKV_SEG_START_ANY);
    if (err != JEKV_ERR_OK) {
        jekv_log_debug("read %s not found", key);
        return err;
    }

    if (item.type == JEKV_TYPE_BLOB) {
        /* erase blob */
        err = storage_erase_blob(storage, find_sector, found_item_index, &item);
    } else {
        /* erase item */
        err = jekv_sector_erase_item(find_sector, found_item_index, &item, true);
        jekv_log_debug("del %s,err=%d", key, err);
    }

    return err;
}

int jekv_storage_del_group(jekv_storage_t *storage, uint8_t group_id)
{
    int err;
    jekv_sector_t *entry = NULL;
    jekv_sector_t *next;
    jekv_item_t item;

    int start_index = 0;

    /* Look up group list */
    dl_list_for_each_safe(entry, next, &storage->sm.active, jekv_sector_t, list)
    {
        start_index = 0;

        while (1) {
            err = jekv_sector_find_item(entry, group_id, (jekv_type_t)(JEKV_TYPE_ANY_WITHOUT_SEG), NULL, &start_index, &item,
                                          JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);

            if (err == JEKV_ERR_NOT_FOUND) {
                /*go to next sector*/
                break;
            } else if (err != JEKV_ERR_OK) {
                return err;
            }

            /*del blob segment*/
            if (item.type == JEKV_TYPE_BLOB) {
                jekv_log_error("%s","del blob item");
                err = storage_erase_blob(storage, entry, start_index, &item);

            } else {
                /*del normal item itself*/
                err = jekv_sector_erase_item(entry, start_index, &item, true);
            }

            /*go to next item*/
            start_index += jekv_item_get_span(&item);
        }
    }
    return JEKV_ERR_OK;
}

int jekv_storage_find_key(jekv_storage_t *storage, uint8_t group_id, const char *key, jekv_item_t *item)
{
    int err;
    jekv_sector_t *find_sector = NULL;
    int found_item_index         = 0;

    err = jekv_sm_find_item(&storage->sm, group_id, (jekv_type_t)JEKV_TYPE_ANY_WITHOUT_SEG, key, &found_item_index, &find_sector, item,
                              JEKV_SEG_ID_ANY, JEKV_SEG_START_ANY);
    if (err != JEKV_ERR_OK) {
        jekv_log_debug("find %s not found", key);
        return err;
    }

    return err;
}
