#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_sec"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_item.h"
#include "jekv_log.h"

/*for valid item*/
int jekv_item_get_span(jekv_item_t *item)
{
    int span = 1;

    if (item->length > 8) {
        /*extra data length*/
        span += (item->length + (JEKV_SLICE_SIZE - 1)) / JEKV_SLICE_SIZE;
    }

    return span;
}

uint32_t jekv_item_crc_hash(const jekv_item_t *item)
{
    return jekv_port_crc32(UINT32_MAX, &item->name, sizeof(item->name) + 2);
}

uint32_t jekv_item_crc_head(const jekv_item_t *item)
{
    uint32_t crc;

    crc = jekv_port_crc32(UINT32_MAX, &item->name, sizeof(item->name) + 4);
    crc = jekv_port_crc32(crc, &item->data, 8);

    return crc;
}

/*can't init blob descriptor*/
int jekv_item_init(jekv_item_t *item, uint8_t state, uint8_t group_id, jekv_type_t type, const char *key,
                     const void *data, int size, uint8_t seg_id)
{
    int key_len = strnlen(key, JEKV_MAX_KEY_LEN);

    memset(item, 0xff, sizeof(*item));

    item->state    = state;
    item->group_id = group_id;
    item->type     = type;
    item->length   = size;
    item->seg_id   = seg_id;
    item->recv1    = 0;

    memcpy(item->name, key, key_len);

    if (key_len < JEKV_MAX_KEY_LEN) {
        item->name[key_len] = 0;
    }

    /*
    init data area if the data is Not NULL
    */
    if (data) {
        if (size > 8) {
            /*multi entry item*/
            item->crc_data = jekv_port_crc32(UINT32_MAX, data, size);
            item->crc_item = jekv_item_crc_head(item);

        } else {
            /*one entry item*/
            memcpy(item->data, data, size);
            item->crc_item = jekv_item_crc_head(item);
        }
    }

    return JEKV_ERR_OK;
}

void jekv_item_print_item_head(jekv_item_t *item)
{
    jekv_log_debug("state=0x%x", item->state);
    jekv_log_debug("group_id=0x%x", item->group_id);
    jekv_log_debug("seg_id=0x%x", item->seg_id);
    jekv_log_debug("type=0x%x", item->type);
    jekv_log_debug("length=0x%x", item->length);

    if (item->type == JEKV_TYPE_BLOB) {
        jekv_log_debug("all_size=0x%x", item->all_size);
        jekv_log_debug("seg_count=0x%x", item->seg_count);
        jekv_log_debug("seg_start=0x%x", item->seg_start);
    }

    return;
}
