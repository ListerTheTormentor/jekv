#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_sec"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_sector_manager.h"
#include "jekv_sector.h"
#include "jekv_debug.h"
#include "jekv_log.h"

#define JEKV_SECTOR_CRC_LEN 24

static uint32_t sector_crc32(jekv_sector_header_t *header)
{
    return jekv_port_crc32(UINT32_MAX, &header->serial_number, JEKV_SECTOR_CRC_LEN);
}

int jekv_sector_set_state(jekv_sector_t *sec, jekv_sector_state_t state)
{
    sec->state = state;
    return jekv_pt_write_raw(sec->pt, sec->address + JEKV_SECTOR_STATE_OFF_SET, &sec->state, sizeof(sec->state));
}

int jekv_sector_erase(jekv_sector_t *sec)
{
    int err;

    jekv_sector_set_state(sec, JEKV_SECTOR_STATE_CRASH);

    /*erase sector*/
    err = jekv_pt_erase(sec->pt, sec->address);
    if (err != JEKV_ERR_OK) {
        return err;
    }

    /*reset sector attibute*/
    sec->state   = JEKV_SECTOR_STATE_UNINIT;
    sec->version = CONFIG_NVS_VER_NUM;

    sec->next_free_slice = 0;
    sec->used_slice      = 0;
    sec->droped_slice    = 0;

    jekv_hash_clear(&sec->hash);

    return JEKV_ERR_OK;
}

static uint32_t sector_get_next_address(jekv_sector_t *sec)
{
    return sec->address + JEKV_ENTRY_DATA_OFFSET + sec->next_free_slice * JEKV_SLICE_SIZE;
}

int jekv_sector_init(jekv_sector_t *sec)
{
    jekv_sector_header_t header;

    sec->state   = JEKV_SECTOR_STATE_USING;
    sec->version = CONFIG_NVS_VER_NUM;

    sec->next_free_slice = 0;
    sec->used_slice      = 0;
    sec->droped_slice    = 0;

    memset(&header, 0xff, sizeof(header));

    header.magic         = JEKV_SECTOR_MAGIC;
    header.state         = JEKV_SECTOR_STATE_USING;
    header.version       = CONFIG_NVS_VER_NUM;
    header.serial_number = sec->serial_number;
    header.crc32         = sector_crc32(&header);

    return jekv_pt_write_raw(sec->pt, sec->address, &header, sizeof(header));
}

/*
    update the follow 3 attribute and hash list
    sec->next_free_slice
    sec->used_slice
    sec->droped_slice
*/
static int sector_update_slice_and_hash(jekv_sector_t *sec)
{
    jekv_item_t item;
    uint32_t offset;
    int err;
    int i;

    jekv_log_debug("before update == %d %d %d", sec->next_free_slice, sec->used_slice, sec->droped_slice);

    /*point to first item, skip sector header */
    offset = sec->address + JEKV_SLICE_SIZE;

    for (i = 0; i < JEKV_ENTRY_COUNT;) {
        err = jekv_pt_read_raw(sec->pt, offset, &item, sizeof(item));
        if (err != JEKV_ERR_OK) {
            sec->state = JEKV_SECTOR_STATE_INVALID;
            jekv_log_debug("read fail:sec=%d,offset=0x%x", i, sec->address);
            return err;
        }

        if (item.state == JEKV_ITEM_STATE_UNUSED) {
            /*to last slice*/
            jekv_log_debug("end:sec=%d,offset=0x%x", i, sec->address);
            sec->next_free_slice = i;
            break;
        } else {
            int span = jekv_item_get_span(&item);

            if (item.state == JEKV_ITEM_STATE_USING) {
                if (item.crc_item == jekv_item_crc_head(&item)) {
                    jekv_hash_append(&sec->hash, &item, i);

                    /*using slice*/
                    sec->used_slice += span;
                    jekv_log_debug("add using %.*s", JEKV_MAX_KEY_LEN, item.name);

                } else {
                    /*crc fail: drop the item not write done, droped_slice will change in function*/
                    sec->used_slice += span;
                    sec->droped_slice += span;

                    jekv_log_debug("crc fail: drop %.*s", JEKV_MAX_KEY_LEN, item.name);
                    err = jekv_sector_erase_item(sec, (uint8_t)i, &item, false);
                    if (err != JEKV_ERR_OK) {
                        sec->state = JEKV_SECTOR_STATE_INVALID;
                        return err;
                    }
                }

            } else if (item.state == JEKV_ITEM_STATE_DROPED) {
                /*droped slice*/
                jekv_log_debug("droped entry, item=%.*s,span=%d", JEKV_MAX_KEY_LEN, item.name, span);
                sec->droped_slice += span;
                sec->used_slice += span;
            }

            i += span;
            offset += span * JEKV_SLICE_SIZE;

            jekv_log_debug("loop i=%d,offset=0x%x,%d", i, offset, offset);
        }
    }

    if (i >= JEKV_ENTRY_COUNT) {
        /*to the sector end*/
        sec->next_free_slice = JEKV_ENTRY_COUNT;
    }

    jekv_log_debug("end update == %d %d %d", sec->next_free_slice, sec->used_slice, sec->droped_slice);

    return JEKV_ERR_OK;
}

static int sector_check_empty(jekv_partition_t *pt, jekv_sector_t *sec, jekv_sector_header_t *header)
{
    int err          = JEKV_ERR_FAIL;
    uint32_t *pblock = NULL;
    uint32_t *p      = NULL;
    uint32_t *pend   = NULL;

    sec->state = header->state;

    pblock = JEKV_MALLOC(pt->sec_size);
    if (!pblock) {
        jekv_log_debug("no men");
        return JEKV_ERR_NO_MEM;
    }

    /*read sector*/
    err = jekv_pt_read_raw(pt, sec->address, pblock, pt->sec_size);
    if (err != JEKV_ERR_OK) {
        sec->state = JEKV_SECTOR_STATE_INVALID;
        jekv_log_debug("check read raw fail");
    } else {
        p    = pblock;
        pend = (uint32_t *)(((char *)pblock) + pt->sec_size);

        /*check sector is empty*/
        while (p < pend) {
            if (*p != 0xffffffff) {
                sec->state = JEKV_SECTOR_STATE_CRASH;
                jekv_log_debug("check crash");
                break;
            }
            p++;
        }
    }

    JEKV_FREE(pblock);

    return err;
}

/*index not include section header*/
int jekv_sector_erase_item(jekv_sector_t *sec, int index, jekv_item_t *item, bool erase_hash)
{
    int err;

    uint32_t offset = sec->address + (index + 1) * JEKV_SLICE_SIZE;
    uint8_t state   = JEKV_ITEM_STATE_DROPED;
    int span        = jekv_item_get_span(item);

    jekv_log_debug("erase %.*s,span=%d", JEKV_MAX_KEY_LEN, item->name, span);
    sec->droped_slice += span;

    err = jekv_pt_write_raw(sec->pt, offset, &state, sizeof(state));

    if (erase_hash) {
        jekv_hash_erase(&sec->hash, index);
    }

    return err;
}

int jekv_sector_load(jekv_partition_t *pt, jekv_sector_t *sec, int sec_index)
{
    int err;
    jekv_sector_header_t header;

    sec->address      = sec_index * pt->sec_size;
    sec->used_slice   = 0;
    sec->droped_slice = 0;
    sec->pt           = pt;

    jekv_hash_init(&sec->hash);

    err = jekv_pt_read_raw(pt, sec->address, &header, sizeof(header));
    if (err != JEKV_ERR_OK) {
        sec->state = JEKV_SECTOR_STATE_INVALID;
        jekv_log_info("read raw error");
        return err;
    }

    sec->state = header.state;

    if (header.state == JEKV_SECTOR_STATE_UNINIT) {
        /* check empty sector */
        err = sector_check_empty(pt, sec, &header);
        if (err != JEKV_ERR_OK) {
            jekv_log_info("check fail %d", sec_index);
            return err;
        }
    } else if (header.magic != JEKV_SECTOR_MAGIC || header.crc32 != sector_crc32(&header)) {
        /* bad sector */
        sec->state = JEKV_SECTOR_STATE_CRASH;
        jekv_log_debug("check crash %d", sec_index);
    } else if (header.version != CONFIG_NVS_VER_NUM) {
        /* kv version changed, erase old version data default */
        sec->state = JEKV_SECTOR_STATE_CRASH;
        jekv_log_warning("new version [%d,%d], del %d", header.version, CONFIG_NVS_VER_NUM, sec_index);
    } else {
        /* good sector */
        sec->serial_number = header.serial_number;
        sec->state         = header.state;
        jekv_log_debug("check %d ok", sec_index);
    }

    if (sec->state == JEKV_SECTOR_STATE_FULL || sec->state == JEKV_SECTOR_STATE_USING ||
        sec->state == JEKV_SECTOR_STATE_DELETTING) {
        return sector_update_slice_and_hash(sec);
    }

    jekv_log_debug("load %d, state=%x,header.state=%x", sec_index, sec->state, header.state);

    return JEKV_ERR_OK;
}

int jekv_sector_write_item_data(jekv_sector_t *sec, jekv_item_t *item, const void *extra_data, uint32_t len, int entry_cnt)
{
    uint32_t offset = sector_get_next_address(sec);

    jekv_pt_write_item(sec->pt, offset, item);

    JEKV_TRACE_POWER_OFF(JEKV_TRACE_TYPE_WRITE, JEKV_TRACE_BEFORE_WRITE_DATA);

    if (extra_data) {
        jekv_pt_write(sec->pt, offset + JEKV_SLICE_SIZE, extra_data, len);
    }

    sec->next_free_slice += entry_cnt;
    sec->used_slice += entry_cnt;

    return JEKV_ERR_OK;
}

int jekv_sector_read_item_data(jekv_sector_t *sec, int found_slice_index, jekv_item_t *item, void *data, uint32_t size)
{
    if (size <= 8) {
        memcpy(data, item->data, size);
        return JEKV_ERR_OK;
    } else {
        /*skip sector header and item itself*/
        uint32_t offset = sec->address + (found_slice_index + 1 + 1) * JEKV_SLICE_SIZE;

        return jekv_pt_read(sec->pt, offset, data, size);
    }
}

int jekv_sector_write_item(jekv_sector_t *sec, uint8_t gid, jekv_type_t type, const char *key, const void *data,
                             uint32_t size, uint8_t seg_id)
{
    int err;
    jekv_item_t item;
    int write_cntry;

    if (sec->state == JEKV_SECTOR_STATE_INVALID) {
        jekv_log_debug("w:bad state");
        return JEKV_ERR_FAIL;
    }

    if (sec->state == JEKV_SECTOR_STATE_FULL) {
        jekv_log_debug("w:full");
        return JEKV_ERR_SECTOR_FULL;
    }

    if (sec->state == JEKV_SECTOR_STATE_UNINIT) {
        err = jekv_sector_init(sec);
        if (err != JEKV_ERR_OK) {
            jekv_log_debug("w:sec init err");
            return err;
        }
    }

    if (size > JEKV_SINGLE_ITEM_MAX_DATA_SIZE) {
        jekv_log_debug("w:bad size=%d", size);
        return JEKV_ERR_VALUE_TOO_LONG;
    }

    uint32_t totalSize = JEKV_SLICE_SIZE;
    uint32_t entry_cnt = 1;

    /*calculate use entrys*/
    if (size > 8) {
        uint32_t roundedSize = (size + JEKV_SLICE_SIZE - 1) & (~(JEKV_SLICE_SIZE - 1));
        totalSize += roundedSize;
        entry_cnt += roundedSize / JEKV_SLICE_SIZE;
    }

    if (sec->next_free_slice + entry_cnt > JEKV_ENTRY_COUNT) {
        /*data size out of sector free size*/
        jekv_log_debug("w:bad cnt,free=%d,entry_cnt=%d", sec->next_free_slice, entry_cnt);
        return JEKV_ERR_SECTOR_FULL;
    }

    jekv_item_init(&item, JEKV_ITEM_STATE_USING, gid, type, key, data, size, seg_id);

    write_cntry = sec->next_free_slice;

    if (size > 8) {
        /*multi entry item, write item head and data*/
        err = jekv_sector_write_item_data(sec, &item, data, size, entry_cnt);

    } else {
        /*one entry item*/
        err = jekv_sector_write_item_data(sec, &item, NULL, 0, entry_cnt);
    }

    /*write OK, add to hash table*/
    if (err == JEKV_ERR_OK) {
        jekv_hash_append(&sec->hash, &item, write_cntry);
    }

    jekv_log_debug("write 0x%x | gid=%d,type=%d,key=%.*s,size=%d,err=%d", sec->address, item.group_id, item.type,
                JEKV_MAX_KEY_LEN, item.name, item.length, err);

    if (err == JEKV_ERR_SECTOR_FULL && sec->state != JEKV_SECTOR_STATE_FULL) {
        jekv_sector_set_state(sec, JEKV_SECTOR_STATE_FULL);
    }

    return err;
}

int jekv_sector_find_item(jekv_sector_t *sec, uint8_t group_id, jekv_type_t type, const char *key, int *item_index,
                            jekv_item_t *item, uint8_t seg_index, jekv_seg_start_t seg_start)
{
    uint32_t start = *item_index;
    uint32_t end   = sec->next_free_slice;

    uint32_t offset;
    int slice_index;
    int span;
    int err;
    int key_len = 0;

    jekv_log_debug("sec find: gid=%d,type=%d,key=%.*s,index=%d, seg=%d,%d", group_id, type, JEKV_MAX_KEY_LEN,
                (key ? key : "null"), *item_index, seg_index, seg_start);

    if (sec->state == JEKV_SECTOR_STATE_CRASH || sec->state == JEKV_SECTOR_STATE_INVALID ||
        sec->state == JEKV_SECTOR_STATE_UNINIT) {
        jekv_log_debug("find: bad state");
        return JEKV_ERR_NOT_FOUND;
    }

    if (start >= JEKV_ENTRY_COUNT) {
        jekv_log_debug("find err: start=%d", start);
        return JEKV_ERR_NOT_FOUND;
    }

    if (end > JEKV_ENTRY_COUNT) {
        end = JEKV_ENTRY_COUNT;
    }

    jekv_log_debug("read item start =%d,end=%d", start, end);

    while (start < end) {
        if (key) {
            /*The key is valid, so the hash value can be calculated and compared*/

            jekv_item_init(item, JEKV_ITEM_STATE_USING, group_id, type, key, NULL, 0, seg_index);

            jekv_log_debug("start =%d", start);

            slice_index = jekv_hash_find(&sec->hash, start, item);
            if (slice_index < 0) {
                return JEKV_ERR_NOT_FOUND;
            }

            /*found hash match*/
            start = slice_index;
            jekv_log_debug("found start =%d", start);
        }

        /*point to first item, skip sector header */
        offset = sec->address + (start + 1) * JEKV_SLICE_SIZE;

        jekv_log_debug("read item start =%d,offset=%d", start, offset);

        err = jekv_pt_read_item(sec->pt, offset, item);
        if (err != JEKV_ERR_OK) {
            sec->state = JEKV_SECTOR_STATE_INVALID;
            return err;
        }

        span = jekv_item_get_span(item);

        jekv_log_debug("found start =%d,span=%d", start, span);

        if (item->state == JEKV_ITEM_STATE_DROPED) {
            /*droped*/
            start += span;

            jekv_log_debug("found drop");

        } else if (item->state == JEKV_ITEM_STATE_USING) {
            if (key) {
                key_len = strnlen(key, JEKV_MAX_KEY_LEN);
            }

            /*using, check item match*/
            if (item->state == JEKV_ITEM_STATE_USING && (group_id == JEKV_GROUP_ID_ANY || group_id == item->group_id) &&
                ((type == JEKV_TYPE_ANY || type == item->type) ||
                 (type == JEKV_TYPE_ANY_WITHOUT_SEG && item->type != JEKV_TYPE_BLOB_SEG)) &&
                (seg_index == JEKV_SEG_ID_ANY || seg_index == item->seg_id) &&
                (seg_start == JEKV_SEG_START_ANY || (item->seg_id >= seg_start && item->seg_id - seg_start < 0x80)) &&
                (!key || !strncmp(item->name, key, key_len))) {
                *item_index = start;
                jekv_log_debug("found match,start=%d,type=%d,item.type=%d", start, type, item->type);

                return JEKV_ERR_OK;

            } else {
                if (type == JEKV_TYPE_BLOB_SEG && item->type == JEKV_TYPE_BLOB_SEG) {
                    jekv_log_debug("for: group_id=%d,type=%d,seg_index=%d,seg_start=%d,key=%.*s", group_id, type, seg_index,
                                seg_start, JEKV_MAX_KEY_LEN, key ? key : "NULL");
                    jekv_log_debug("it: group_id=%d,type=%d,seg_index=%d,seg_start=%d,key=%.*s", item->group_id, item->type,
                                item->seg_id, item->seg_start, JEKV_MAX_KEY_LEN, item->name);
                } else if (type == JEKV_TYPE_BLOB && item->type == JEKV_TYPE_BLOB) {
                    jekv_log_debug("for: group_id=%d,type=%d,seg_index=%d,seg_start=%d,key=%.*s", group_id, type, seg_index,
                                seg_start, JEKV_MAX_KEY_LEN, key ? key : "NULL");
                    jekv_log_debug("it: group_id=%d,type=%d,seg_index=%d,seg_start=%d,key=%.*s", item->group_id, item->type,
                                item->seg_id, item->seg_start, JEKV_MAX_KEY_LEN, item->name);
                }
                start += span;
            }
        } else {
            /*unusing*/
            jekv_log_debug("not found ");
            return JEKV_ERR_NOT_FOUND;
        }
    }

    return JEKV_ERR_NOT_FOUND;
}

int jekv_sector_copy(jekv_sector_t *dst, jekv_sector_t *src)
{
    int err;

    jekv_item_t item;
    int span;

    uint32_t src_index = 0;
    uint32_t dst_index = 0;

    while (src_index < JEKV_ENTRY_COUNT) {
        /*read item form source sector*/
        err = jekv_pt_read_item(src->pt, src->address + (src_index + 1) * JEKV_SLICE_SIZE, &item);
        if (err != JEKV_ERR_OK) {
            src->state = JEKV_SECTOR_STATE_INVALID;
            return err;
        }

        /*get item span*/

        span = jekv_item_get_span(&item);
        if (item.state == JEKV_ITEM_STATE_DROPED) {
            /*droped item, skip it*/
            src_index += span;

            jekv_log_debug("found drop");

        } else if (item.state == JEKV_ITEM_STATE_USING) {
            /*using item, start copy*/

            /*write item to the dest sector*/
            err = jekv_pt_write_item(dst->pt, dst->address + (dst_index + 1) * JEKV_SLICE_SIZE, &item);
            if (err != JEKV_ERR_OK) {
                dst->state = JEKV_SECTOR_STATE_INVALID;
                return err;
            }

            /*need copy item data*/
            if (span > 1) {
                /*malloc memory for item data*/
                int data_size = item.length;
                uint8_t *p    = JEKV_MALLOC(data_size);
                if (!p) {
                    return JEKV_ERR_NO_MEM;
                }

                /*read item data from the source sector*/
                err = jekv_pt_read(src->pt, src->address + (src_index + 2) * JEKV_SLICE_SIZE, p, data_size);
                if (err != JEKV_ERR_OK) {
                    src->state = JEKV_SECTOR_STATE_INVALID;
                    JEKV_FREE(p);
                    return err;
                }

                /*write item data to the dest sector*/
                err = jekv_pt_write(dst->pt, dst->address + (dst_index + 2) * JEKV_SLICE_SIZE, p, data_size);
                if (err != JEKV_ERR_OK) {
                    dst->state = JEKV_SECTOR_STATE_INVALID;
                    JEKV_FREE(p);
                    return err;
                }

                JEKV_FREE(p);
            }

            /*update dst sector info*/
            jekv_hash_append(&dst->hash, &item, dst_index);
            dst->used_slice += span;
            dst->next_free_slice += span;

            /*loop next item*/
            src_index += span;
            dst_index += span;

        } else {
            /*unusing, copy end*/
            dst->next_free_slice = dst->used_slice;
            jekv_log_debug("copy to end,src_index=0x%x,dst_used=%d,dst_next_free=%d", src_index, dst->used_slice,
                        dst->next_free_slice);
            break;
        }
    }

    jekv_log_debug("copy end");

    return JEKV_ERR_OK;
}
