#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_sm"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_sector_manager.h"
#include "jekv_debug.h"
#include "jekv_log.h"

static int sm_init_default(jekv_sector_manager_t *sm, jekv_partition_t *pt)
{
    sm->sec_arr = JEKV_CALLOC(1, pt->sec_num * sizeof(jekv_sector_t));
    sm->pt      = pt;

    dl_list_init(&sm->active);
    dl_list_init(&sm->idle);

    return JEKV_ERR_OK;
}

static int sm_active_sector(jekv_sector_manager_t *sm)
{
    int err;
    jekv_sector_t *sec = NULL;

    sec = dl_list_first(&sm->idle, jekv_sector_t, list);

    if (sec->state == JEKV_SECTOR_STATE_CRASH || sec->state == JEKV_SECTOR_STATE_INVALID) {
        err = jekv_sector_erase(sec);
        if (err != JEKV_ERR_OK) {
            return err;
        }
    }

    sec->serial_number = sm->serial_number++;

    dl_list_del(&sec->list);
    dl_list_add_tail(&sm->active, &sec->list);

    return JEKV_ERR_OK;
}

static int sm_load_sectors(jekv_sector_manager_t *sm, jekv_partition_t *pt)
{
    int err = JEKV_ERR_OK;
    int i;

    jekv_sector_state_t state;
    jekv_sector_t *sec        = NULL;
    jekv_sector_t *entry      = NULL;
    jekv_sector_t *entry_next = NULL;
    int found;

    jekv_log_debug("load sectors");

    for (i = 0; i < pt->sec_num; i++) {
        sec = &sm->sec_arr[i];

        err = jekv_sector_load(pt, sec, i);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        state = (jekv_sector_state_t)(sec->state);

        if (state == JEKV_SECTOR_STATE_UNINIT || state == JEKV_SECTOR_STATE_CRASH || state == JEKV_SECTOR_STATE_INVALID) {
            /* invalid, unuse, crash sectors */
            jekv_log_debug("add %d 0x%x, state=%x, add to idle", i, sec->address, state);
            dl_list_add_tail(&(sm->idle), &sec->list);
        } else {
            /* using, full, deleting sectors */
            /* look up the position */
            found = 0;
            dl_list_for_each_safe(entry, entry_next, &sm->active, jekv_sector_t, list)
            {
                if (entry->serial_number > sec->serial_number) {
                    found = 1;
                    break;
                }
            }

            if (found && entry) {
                /*insert sector before entry, entry's SN is larger than current sector*/
                dl_list_add_tail(&entry->list, &sec->list);
                jekv_log_debug("insert sec_id=%d,sn=%u, state=%x, to active", i, entry->serial_number, state);
            } else {
                /*add to the and of active list*/
                dl_list_add_tail(&sm->active, &sec->list);
                jekv_log_debug("add sec_id=%d,sn=%u, state=%x, to active tail", i, entry->serial_number, state);
            }
        }
    }

    /* update global serial number */
    if (dl_list_empty(&sm->active)) {
        sm->serial_number = 1;
        err               = sm_active_sector(sm);
        return err;
    } else {
        entry             = dl_list_last(&sm->active, jekv_sector_t, list);
        sm->serial_number = entry->serial_number + 1;
        jekv_log_debug("last sn=%u", entry->serial_number);
    }

    dl_list_for_each(entry, &sm->active, jekv_sector_t, list)
    {
        jekv_log_debug("active sec_id=%d,sn=%u", entry->address / JEKV_SECTOR_SIZE, entry->serial_number);
    }

    return err;
}

static int sm_check_item_data(jekv_sector_t *sec, int index, jekv_item_t *item)
{
    int err = JEKV_ERR_OK;

    /*check have additional data*/
    if ((item->type == JEKV_TYPE_STRING || item->type == JEKV_TYPE_BINARY || item->type == JEKV_TYPE_BLOB_SEG) &&
        item->length > 8) {
        uint8_t *p = JEKV_MALLOC(item->length);

        if (p) {
            /*read data*/
            err = jekv_pt_read(sec->pt, sec->address + (index + 2) * JEKV_SLICE_SIZE, p, item->length);

            if (err == JEKV_ERR_OK) {
                /*check data crc*/
                uint32_t crc = jekv_port_crc32(UINT32_MAX, p, item->length);
                if (crc != item->crc_data) {
                    jekv_log_warning("crc:drop last item %d:%.*s", item->group_id, JEKV_MAX_KEY_LEN, item->name);

                    /*drop write imcomplete item*/
                    jekv_sector_erase_item(sec, index, item, true);

                    err = JEKV_ERR_FAIL;
                }
            }

            JEKV_FREE(p);
        }
    }

    return err;
}

static int sm_check_imcomplete_write(jekv_sector_manager_t *sm)
{
    int err;

    bool found     = false;
    int last_index = 0;
    int next_index = 0;
    jekv_seg_start_t seg_start;
    uint8_t seg_id;
    jekv_item_t item;

    jekv_sector_t *last = dl_list_last(&sm->active, jekv_sector_t, list);

    if (!last) {
        return JEKV_ERR_FAIL;
    }

    jekv_log_debug("power off imcomplete_write check");

    /*find last item*/
    while (1) {
        err = jekv_sector_find_item(last, JEKV_GROUP_ID_ANY, JEKV_TYPE_ANY, NULL, &next_index, &item, JEKV_SEG_ID_ANY,
                                      JEKV_SEG_START_ANY);
        if (err != JEKV_ERR_OK) {
            break;
        } else {
            found      = true;
            last_index = next_index;
            next_index += jekv_item_get_span(&item);
        }
    }

    if (found) {
        jekv_sector_t *entry = NULL;
        jekv_item_t old;
        int old_index;

        /*check last item data*/
        jekv_log_debug("check last data");

        if (sm_check_item_data(last, last_index, &item) != JEKV_ERR_OK) {
            /*The last item is not write OK, droped.*/
            return JEKV_ERR_OK;
        }

        jekv_log_debug("last:sec_index=%d,gid=%d,type=%d,name=%.*s,len=%d,seg_id=%d,seg_start=%d",
                    last->address / JEKV_SECTOR_SIZE, item.group_id, item.type, JEKV_MAX_KEY_LEN, item.name, item.length,
                    item.seg_id, item.seg_start);

        seg_id    = (item.type == JEKV_TYPE_BLOB_SEG ? item.seg_id : JEKV_SEG_ID_ANY);
        seg_start = JEKV_SEG_START_ANY;

        /*find old items*/
        dl_list_for_each(entry, &sm->active, jekv_sector_t, list)
        {
            old_index = 0;

            if (entry != last) {
                /*check other sector*/
                err = jekv_sector_find_item(entry, item.group_id, (jekv_type_t)item.type, item.name, &old_index, &old, seg_id, seg_start);
                if (err == JEKV_ERR_OK) {
                    jekv_log_debug("found double in sec 0x%x,del old", entry->address);
                    jekv_sector_erase_item(entry, old_index, &old, true);
                    break;
                }
            } else {
                /*check the last sector*/
                err = jekv_sector_find_item(entry, item.group_id, (jekv_type_t)item.type, item.name, &old_index, &old, seg_id, seg_start);
                if (err == JEKV_ERR_OK && old_index != last_index) {
                    jekv_log_debug("found double in last sector,del old");
                    jekv_sector_erase_item(entry, old_index, &old, true);
                    break;
                } else {
                    jekv_log_debug("not found. old_index=%d,last=%d", old_index, last_index);
                }
            }
        }
    }

    return JEKV_ERR_OK;
}

static int sm_check_imcomplete_gc(jekv_sector_manager_t *sm)
{
    int err;
    jekv_sector_t *it    = NULL;
    jekv_sector_t *entry = NULL;

    jekv_sector_t *last;
    jekv_sector_t *new_sec;

    jekv_log_debug("power off GC check");

    dl_list_for_each(entry, &sm->active, jekv_sector_t, list)
    {
        if (entry->state == JEKV_SECTOR_STATE_DELETTING) {
            /*found the deleting sector*/
            it = entry;
            jekv_log_debug("it=0x%x", it->address);
            break;
        }
    }

    if (!it) {
        /*No deleting sector*/
        return JEKV_ERR_OK;
    }

    last = dl_list_last(&sm->active, jekv_sector_t, list);

    if (last->state == JEKV_SECTOR_STATE_USING) {
        /* STEP0 : erase not complete sector*/

        jekv_log_debug("GC-0: erase the last");
        err = jekv_sector_erase(last);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        /*add the last to idle*/
        dl_list_del(&last->list);
        dl_list_add_tail(&sm->idle, &last->list);
    }

    /*make a new sector to the active list end*/
    err = sm_active_sector(sm);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /* STEP1 : Set new sector status to using*/
    jekv_log_debug("GC-1:set new using");
    new_sec = dl_list_last(&sm->active, jekv_sector_t, list);
    if (new_sec->state == JEKV_SECTOR_STATE_UNINIT) {
        /*write sector header*/
        err = jekv_sector_init(new_sec);
        if (err != JEKV_ERR_OK) {
            return err;
        }
    }
    /* STEP2 : Set dirst sector status to deleting, it seted before*/

    /* STEP3 : Copy dirtiest sector to the GC sector*/
    jekv_log_debug("GC-3:copy");
    err = jekv_sector_copy(new_sec, it);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    jekv_log_debug("GC-4:erase old");

    /* STEP4 : erase the dirtiest secto*/
    err = jekv_sector_erase(it);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /*remove the dirtiest sector from active*/
    dl_list_del(&it->list);

    /*Add the dirtiest sector to idle list*/
    dl_list_add_tail(&sm->idle, &it->list);

    return JEKV_ERR_OK;
}

static int sm_garbage_collection(jekv_sector_manager_t *sm, int need_size)
{
    int err;
    jekv_sector_t *entry = NULL;
    jekv_sector_t *entry_next;
    jekv_sector_t *dirtiest = NULL;

    jekv_sector_t *new_sec;

    int can_get_size;
    int most_dirty_size = 0;

    dl_list_for_each_safe(entry, entry_next, &sm->active, jekv_sector_t, list)
    {
        can_get_size = jekv_sm_get_gc_size(entry);

        if (can_get_size > most_dirty_size) {
            most_dirty_size = can_get_size;
            dirtiest        = entry;
        }
    }

    if (most_dirty_size >= need_size) {
        jekv_log_debug("GC:get dirtiest=0x%x,size=%d", dirtiest->address, most_dirty_size);
        jekv_log_debug("GC:active new sector");

        /*prepare the GC sector*/
        err = sm_active_sector(sm);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        /* STEP1 : Set new sector status to using*/
        jekv_log_debug("GC-1:set new using");
        new_sec = dl_list_last(&sm->active, jekv_sector_t, list);
        if (new_sec->state == JEKV_SECTOR_STATE_UNINIT) {
            /*write sector header*/
            err = jekv_sector_init(new_sec);
            if (err != JEKV_ERR_OK) {
                return err;
            }
        }

        JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_GC, JEKV_TRACE_GC_1_NEW_SECTOR);

        /* STEP2 : Set dirst sector status to deleting*/
        jekv_log_debug("GC-2:set deletting");
        err = jekv_sector_set_state(dirtiest, JEKV_SECTOR_STATE_DELETTING);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_GC, JEKV_TRACE_GC_2_SET_OLD_DELETING);

        /* STEP3 : Copy dirtiest sector to the GC sector*/
        jekv_log_debug("GC-3:copy");
        err = jekv_sector_copy(new_sec, dirtiest);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        jekv_log_debug("GC-4:erase old");

        JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_GC, JEKV_TRACE_GC_3_COPY);

        /* STEP4 : erase the dirtiest secto*/
        err = jekv_sector_erase(dirtiest);
        if (err != JEKV_ERR_OK) {
            return err;
        }

        JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_GC, JEKV_TRACE_GC_4_ERASE_OLD);

        /*remove the dirtiest sector from active*/
        dl_list_del(&dirtiest->list);

        /*Add the dirtiest sector to idle list*/
        dl_list_add_tail(&sm->idle, &dirtiest->list);

        jekv_log_debug("GC:ok");

        return JEKV_ERR_OK;
    } else {
        jekv_log_error("GC not do, need %d,only %d", need_size, most_dirty_size);
        return JEKV_ERR_NO_SPACE;
    }
}

int jekv_sm_load(jekv_sector_manager_t *storage_manager, jekv_partition_t *pt)
{
    int err;
    jekv_sector_manager_t *sm = storage_manager;
    jekv_sector_t *sec;

    /*init sector manager*/
    err = sm_init_default(sm, pt);
    if (err != JEKV_ERR_OK) {
        return JEKV_ERR_NO_MEM;
    }

    /*load sectors and update global sn to last + 1*/
    err = sm_load_sectors(sm, pt);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /* 1 : Check the last item wrote OK
       2 : Check whether the old data was deleted last time due to power failure
    */

    sm_check_imcomplete_write(sm);

    /* Check whether the old GC page was deleted due to power failure*/

    sm_check_imcomplete_gc(sm);

    /* Check GC sector exist*/
    if (!pt->readonly && dl_list_empty(&sm->idle)) {
        /* The last sector use to GC but not do, roll back */
        jekv_log_error("move last active to idle");

        sec = dl_list_last(&sm->active, jekv_sector_t, list);
        dl_list_del(&sec->list);
        dl_list_add_tail(&sm->idle, &sec->list);
    }

    jekv_log_debug("sec load end\n");

    return JEKV_ERR_OK;
}

int jekv_sm_unload(jekv_sector_manager_t *sm)
{
    jekv_sector_t *sec;

    jekv_log_debug("unload sectors in %s", sm->pt->name);

    /*remove hash table for each sector*/
    dl_list_for_each(sec, &sm->active, jekv_sector_t, list)
    {
        jekv_hash_clear(&sec->hash);
    }

    /*free sector array*/
    JEKV_FREE(sm->sec_arr);

    return JEKV_ERR_OK;
}

int jekv_sm_request_sector(jekv_sector_manager_t *sm, int need_size)
{
    int err;
    int num = dl_list_len(&sm->idle);

    if (num == 0) {
        jekv_log_error("no idle sector");
        return JEKV_ERR_NO_MEM;
    }

    if (num == 1) {
        /*no enough idle sector now , do GC*/
        err = sm_garbage_collection(sm, need_size);
    } else if (num > 1) {
        /*no enough idle sector now , do GC*/
        err = sm_active_sector(sm);
    } else {
        jekv_log_debug("no idle sector");
        err = JEKV_ERR_FAIL;
    }

    return err;
}

int jekv_sm_find_item(jekv_sector_manager_t *sm, uint8_t group_id, jekv_type_t type, const char *key, int *item_index,
                        jekv_sector_t **sector, jekv_item_t *item, uint8_t seg_index, jekv_seg_start_t seg_start)
{
    int err;
    jekv_sector_t *entry = NULL;

    /* Look up active sector list */
    dl_list_for_each(entry, &sm->active, jekv_sector_t, list)
    {
        err = jekv_sector_find_item(entry, group_id, type, key, item_index, item, seg_index, seg_start);
        if (err == JEKV_ERR_OK) {
            *sector = entry;
            return JEKV_ERR_OK;
        }
    }

    return JEKV_ERR_NOT_FOUND;
}

int jekv_sm_check_write_blob_size(jekv_sector_manager_t *sm, uint32_t size)
{
    jekv_sector_t *entry = NULL;

    int left_size      = size;
    int idle_num       = dl_list_len(&sm->idle);
    int max_write_size = (sm->pt->sec_num - 1) * JEKV_SINGLE_ITEM_MAX_DATA_SIZE;
    int free_size;
    int cur_seg_size;
    int seg_count = 0;

    /*check all partition size*/
    if (left_size > max_write_size) {
        return JEKV_ERR_NO_SPACE;
    }

    if (left_size > JEKV_SEG_NUM_MAX * JEKV_SINGLE_ITEM_MAX_DATA_SIZE) {
        return JEKV_ERR_INVALID_LENGTH;
    }

    /*check idle size*/
    left_size -= (idle_num - 1) * JEKV_SINGLE_ITEM_MAX_DATA_SIZE;

    /*-JEKV_SLICE_SIZE for addtional desc size*/
    if (left_size <= -JEKV_SLICE_SIZE) {
        return JEKV_ERR_OK;
    }

    /* check active sectors*/
    dl_list_for_each(entry, &sm->active, jekv_sector_t, list)
    {
        free_size = jekv_sm_get_gc_size(entry);

        if (left_size > 0 && seg_count >= JEKV_SEG_NUM_MAX) {
            jekv_log_debug("up to max segments");
            break;
        }
        if (left_size > 0) {
            /*check data write*/
            if (left_size + JEKV_SLICE_SIZE <= free_size) {
                /*can write all left data*/
                seg_count++;
                left_size = 0;

                if (entry->next_free_slice + 1 <= JEKV_ENTRY_COUNT) {
                    return JEKV_ERR_OK;
                } else {
                    /*need write desc next loop*/
                }
            } else {
                /* free space is larger than the size of minimum */

                if (free_size >= JEKV_BLOB_MIN_SEG_SIZE + JEKV_SLICE_SIZE) {
                    cur_seg_size = free_size - JEKV_SLICE_SIZE;

                    left_size -= cur_seg_size;
                    seg_count++;
                } else {
                    /*skip curren sector, it too small*/
                }
            }

        } else if (free_size >= JEKV_SLICE_SIZE) {
            /*check write descriptor*/
            return JEKV_ERR_OK;
        }
    }

    return JEKV_ERR_NO_SPACE;
}

int jekv_sm_get_status(jekv_sector_manager_t *sm, jekv_status_t *status)
{
    jekv_sector_t *entry = NULL;

    uint32_t used_slice   = 0;
    uint32_t droped_slice = 0;

    dl_list_for_each(entry, &sm->active, jekv_sector_t, list)
    {
        /*add sector header to statistics*/
        used_slice += entry->used_slice + 1;
        droped_slice += entry->droped_slice;
    }

    status->total_size  = sm->pt->sec_size * sm->pt->sec_num;
    status->using_size  = used_slice * JEKV_SLICE_SIZE;
    status->droped_size = droped_slice * JEKV_SLICE_SIZE;
    status->free_size   = status->total_size - status->using_size;

    return JEKV_ERR_OK;
}
