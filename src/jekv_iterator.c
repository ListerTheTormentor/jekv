#define LOG_TAG "jekv_iterator"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_item.h"
#include "jekv_sector.h"
#include "jekv_iterator.h"
#include "jekv_storage.h"
#include "jekv_partition_manager.h"
#include "jekv_log.h"

static int iterator_find(jekv_storage_t *storage, uint8_t group_id, jekv_type_t type, jekv_iterator_t *output_iterator)
{
    jekv_iterator_info_t *it;

    it = JEKV_CALLOC(1, sizeof(*it));

    if (!it) {
        jekv_log_verbose("No mem");
        return JEKV_ERR_NO_MEM;
    }

    it->storage  = storage;
    it->group_id = group_id;
    it->type     = type;

    *output_iterator = (jekv_iterator_t)it;

    /*find next*/
    return jekv_iterator_next(output_iterator);
}

int jekv_iterator_find(const char *partition_name, const char *group, jekv_type_t type, jekv_iterator_t *output_iterator)
{
    jekv_group_t *group_node;
    jekv_storage_t *storage;

    uint8_t group_id = JEKV_GROUP_ID_ANY;

    storage = jekv_ptm_find_storage(partition_name);
    if (!storage) {
        jekv_log_debug("Not init");
        return JEKV_ERR_NOT_INIT;
    }

    if (group) {
        group_node = jekv_storage_find_group_by_name(storage, group);
        if (group_node) {
            group_id = group_node->id;
        } else {
            jekv_log_debug("group %s not found", group);
            return JEKV_ERR_NOT_FOUND;
        }
    }

    return iterator_find(storage, group_id, type, output_iterator);
}

int jekv_iterator_find_by_handle(jekv_handle_t handle, jekv_type_t type, jekv_iterator_t *output_iterator)
{
    jekv_handle_info_t *h = (jekv_handle_info_t *)handle;

    return iterator_find(h->storage, h->group_id, type, output_iterator);
}

int jekv_iterator_next(jekv_iterator_t *iterator)
{
    int err;
    jekv_iterator_info_t *it  = (jekv_iterator_info_t *)(*iterator);
    jekv_sector_manager_t *sm = &it->storage->sm;
    jekv_sector_t *sec        = NULL;
    jekv_item_t item;
    jekv_group_t *group;
    jekv_type_t look_up_type;

    /*don't look up blob segment*/
    look_up_type = (it->type == JEKV_TYPE_ANY ? (jekv_type_t)(JEKV_TYPE_ANY_WITHOUT_SEG) : it->type);

    if (!it->sector) {
        /*first*/
        it->sector      = dl_list_first(&sm->active, jekv_sector_t, list);
        it->slice_index = 0;
    }

    jekv_log_debug("iterator next: gid=%d,type=%d,index=%d", it->group_id, it->type, it->slice_index);

    for (sec = it->sector; &sec->list != &sm->active;) {
        err = jekv_sector_find_item(sec, it->group_id, look_up_type, NULL, &it->slice_index, &item, JEKV_SEG_ID_ANY,
                                      JEKV_SEG_START_ANY);

        if (err == JEKV_ERR_OK) {
            it->slice_index += jekv_item_get_span(&item);

            /*found*/

            /*get group*/
            group = NULL;
            if (item.group_id > 0) {
                group = jekv_storage_find_group_by_id(it->storage, item.group_id);
            }

            /*set or clear group name*/
            if (group) {
                snprintf(it->info.group, sizeof(it->info.group), "%s", group->name);
            } else {
                it->info.group[0] = 0;
            }

            snprintf(it->info.key, sizeof(it->info.key), "%.*s", JEKV_MAX_KEY_LEN, item.name);

            it->info.type     = (jekv_type_t)item.type;
            it->info.length   = (item.type == JEKV_TYPE_BLOB ? item.all_size : item.length);
            it->info.group_id = item.group_id;

            return err;

        } else {
            /*error*/
            jekv_log_debug("find err=%d", err);

            /*next sector*/
            sec             = dl_list_entry(sec->list.next, jekv_sector_t, list);
            it->sector      = sec;
            it->slice_index = 0;

            jekv_log_verbose("state=%x, sec offset=%u, sn=%d, next=%d, used=%d, droped=%d\r\n", sec->state, sec->address,
                        sec->serial_number, sec->next_free_slice, sec->used_slice, sec->droped_slice);
        }
    }

    /*iterator end*/
    JEKV_FREE(it);
    *iterator = NULL;

    return JEKV_ERR_NOT_FOUND;
}

int jekv_iterator_info(jekv_iterator_t iterator, jekv_entry_t *info)
{
    *info = ((jekv_iterator_info_t *)iterator)->info;
    return JEKV_ERR_OK;
}

int jekv_iterator_data(jekv_iterator_t iterator, void *data, uint32_t *data_len)
{
    jekv_iterator_info_t *it = (jekv_iterator_info_t *)iterator;
    jekv_entry_t *pinfo      = &it->info;

    return jekv_storage_read_item(it->storage, pinfo->group_id, pinfo->type, pinfo->key, data, data_len);
}

int jekv_iterator_release(jekv_iterator_t iterator)
{
    if (iterator) {
        JEKV_FREE(iterator);
    }
    return JEKV_ERR_OK;
}

static void iterator_print_value(const char *group, const char *key, jekv_type_t type, const void *p, uint32_t length)
{
    switch (type) {
        case JEKV_TYPE_STRING:
        {
            jekv_log_info("%s:%s=%s", group, key, (length > 0 ? (char *)p : ""));
            break;
        }
        case JEKV_TYPE_INT32:
        {
            jekv_log_info("%s:%s=%d", group, key, *((int *)p));
            break;
        }
        case JEKV_TYPE_UINT32:
        {
            jekv_log_info("%s:%s=%u", group, key, *((unsigned int *)p));
            break;
        }
        case JEKV_TYPE_INT8:
        {
            jekv_log_info("%s:%s=%d", group, key, *((signed char *)p));
            break;
        }
        case JEKV_TYPE_INT16:
        {
            jekv_log_info("%s:%s=%d", group, key, *((short *)p));
            break;
        }
        case JEKV_TYPE_UINT8:
        {
            jekv_log_info("%s:%s=%u", group, key, *((unsigned char *)p));
            break;
        }
        case JEKV_TYPE_UINT16:
        {
            jekv_log_info("%s:%s=%u", group, key, *((unsigned short *)p));
            break;
        }
        case JEKV_TYPE_INT64:
        {
            jekv_log_info("%s:%s=%" PRId64, group, key, (int64_t)(*((int64_t *)p)));
            break;
        }
        case JEKV_TYPE_UINT64:
        {
            jekv_log_info("%s:%s=%" PRIu64, group, key, (uint64_t)(*((uint64_t *)p)));
            break;
        }
        case JEKV_TYPE_DOUBLE:
        {
            jekv_log_info("%s:%s=%lf", group, key, *((double *)p));
            break;
        }
        case JEKV_TYPE_BINARY:
        case JEKV_TYPE_BLOB:
        {
            jekv_log_info("%s:%s", group, key);
            JEKV_DUMPE(key, 32, p, length);
            break;
        }
        default:
        {
            jekv_log_error("error type %d", type);
            break;
        }
    }
}

int jekv_iterator_print(const char *partition_name, const char *group_name)
{
    int err;
    jekv_iterator_t iterator = NULL;
    jekv_entry_t entry;
    uint8_t buf[32];
    uint32_t length;

    if (!partition_name) {
        return JEKV_ERR_INVALID_PARAM;
    }

    jekv_log_debug("start print,pt=%s,group=%s", partition_name, group_name ? group_name : "NULL");

    err = jekv_iterator_find(partition_name, group_name, (jekv_type_t)(JEKV_TYPE_ANY_WITHOUT_SEG), &iterator);
    if (err == JEKV_ERR_OK) {
        jekv_log_debug("find %s iterator=%p", partition_name, iterator);
        while (iterator) {
            jekv_iterator_info(iterator, &entry);

            jekv_log_debug("get iterator info %s:%s,length=%u", entry.group, entry.key, entry.length);

            if (entry.length <= sizeof(buf)) {
                length = sizeof(buf);
                err    = jekv_iterator_data(iterator, buf, &length);
                if (err == JEKV_ERR_OK) {
                    iterator_print_value(entry.group, entry.key, entry.type, buf, length);
                }
            } else {
                char *p = JEKV_MALLOC(entry.length);
                if (!p) {
                    break;
                }

                length = entry.length;
                err    = jekv_iterator_data(iterator, p, &length);
                if (err == JEKV_ERR_OK) {
                    iterator_print_value(entry.group, entry.key, entry.type, p, length);
                }
                JEKV_FREE(p);
            }
            jekv_iterator_next(&iterator);
        }
        jekv_iterator_release(iterator);
    } else {
        jekv_log_debug("find %s error", partition_name);
    }

    return JEKV_ERR_OK;
}
