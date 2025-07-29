#include <string.h>
#include <stdlib.h>

#define LOG_TAG "jekv_hash"
#include "jekv_porting.h"
#include "jekv_base.h"
#include "jekv_item.h"
#include "jekv_sector.h"
#include "jekv_log.h"

#define JEKV_HASH_ENLARGE_STEP 10

int jekv_hash_init(jekv_hash_t *h)
{
    memset(h, 0, sizeof(*h));
    return JEKV_ERR_OK;
}

int jekv_hash_append(jekv_hash_t *h, const jekv_item_t *item, uint32_t index)
{
    int new_num;

    /*full*/
    if (h->count >= JEKV_ENTRY_COUNT) {
        return JEKV_ERR_NO_SPACE;
    }

    /*need enlarge*/
    if (h->size <= h->count) {
        new_num = h->size + JEKV_HASH_ENLARGE_STEP;
        jekv_log_verbose("count=%d,(old_size-->new_size) = (%u -->%u)", h->count, h->size, new_num);
        h->hash_table = JEKV_REALLOC(h->hash_table, new_num * sizeof(jekv_hash_node_t));
        if (!h->hash_table) {
            return JEKV_ERR_NO_MEM;
        }
        h->size = new_num;
    }

    /*add to last*/
    h->hash_table[h->count].hash = jekv_item_crc_hash(item);
    h->hash_table[h->count].id   = (uint8_t)index;

    jekv_log_debug("insert %.*s --> %d", JEKV_MAX_KEY_LEN, item->name, h->count);

    h->count++;

    return JEKV_ERR_OK;
}

int jekv_hash_erase(jekv_hash_t *h, const uint32_t index)
{
    int i;

    for (i = 0; i < h->count; i++) {
        if (h->hash_table[i].index == index) {
            /*Not the last one*/
            if (i != h->count - 1) {
                /* Move the following items to the front , keep the index order*/
                memcpy(&h->hash_table[i], &h->hash_table[i + 1], (h->count - i - 1) * sizeof(jekv_hash_node_t));
            }

            h->count--;

            jekv_log_debug("erase %d", i);

            return JEKV_ERR_OK;
        }
    }

    return JEKV_ERR_NOT_FOUND;
}

int jekv_hash_find(jekv_hash_t *h, uint32_t start, const jekv_item_t *item)
{
    int i;
    uint32_t crc;

    crc = jekv_item_crc_hash(item);

    for (i = 0; i < h->count; i++) {
        if (h->hash_table[i].index >= start && h->hash_table[i].index != JEKV_HASH_INVALID &&
            h->hash_table[i].hash == (crc & 0xffffff)) {
            jekv_log_debug("found %.*s at %d", JEKV_MAX_KEY_LEN, item->name, i);
            return h->hash_table[i].index;
        }
    }

    return JEKV_HASH_INVALID;
}

void jekv_hash_clear(jekv_hash_t *h)
{
    jekv_log_debug("clear");

    if (h->hash_table) {
        JEKV_FREE(h->hash_table);
        memset(h, 0, sizeof(*h));
    }
    return;
}
