#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define LOG_TAG "jekv_pt"
#include "jekv_porting.h"
#include "jekv_partition.h"
#include "jekv_base.h"
#include "jekv_log.h"

int jekv_pt_init(const char *partition_name, jekv_partition_t *pt)
{
    jkvs_partition_item_t ptable;

    if (jekv_partition_get_info(partition_name, &ptable) != JEKV_ERR_OK) {
        jekv_log_error("read pt %s fail", partition_name);
        return JEKV_ERR_FAIL;
    }

    pt->dev = jekv_partition_open(partition_name);
    snprintf(pt->name, sizeof(pt->name), "%s", partition_name);
    pt->offset    = ptable.offset;
    pt->sec_size  = JEKV_SECTOR_SIZE;
    pt->sec_num   = ptable.size / pt->sec_size;
    pt->size      = ptable.size;
    pt->encrypted = 0;
    pt->readonly  = 0;

    jekv_log_debug("pt init, pt=%p,name=%s,offset=0x%x,size=0x%x,sec_num=%d", pt, partition_name, pt->offset, pt->sec_size,
                pt->sec_num);

    if ((pt->sec_num < 2 && !pt->readonly) || pt->sec_num < 1) {
        return JEKV_ERR_FAIL;
    } else {
        return JEKV_ERR_OK;
    }
}

int jekv_pt_erase_all(jekv_partition_t* pt)
{
    return jekv_partition_erase(pt->dev, pt->offset, pt->size);
}

int jekv_pt_erase(jekv_partition_t *pt, uint32_t address)
{
    jekv_log_debug("erase %s, 0x%x", pt->name, address);

    if (pt->readonly) {
        return JEKV_ERR_READ_ONLY;
    }

    if (!(address + JEKV_SECTOR_SIZE <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    return jekv_partition_erase(pt->dev, (pt->offset + address), JEKV_SECTOR_SIZE);
}

int jekv_pt_read(jekv_partition_t *pt, uint32_t address, void *data, uint32_t length)
{
    jekv_log_debug("read %s, off=0x%x,len=%u", pt->name, address, length);

    if (!(address + length <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    return jekv_partition_read(pt->dev, pt->offset + address, data, length);
}

int jekv_pt_write(jekv_partition_t *pt, uint32_t address, const void *data, uint32_t length)
{
    jekv_log_debug("write %s, off=0x%x,len=%u", pt->name, address, length);

    if (!(address + length <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    if (pt->readonly) {
        return JEKV_ERR_READ_ONLY;
    }

    return jekv_partition_write(pt->dev, pt->offset + address, (uint8_t *)data, length);
}

int jekv_pt_read_raw(jekv_partition_t *pt, uint32_t address, void *data, uint32_t length)
{
    jekv_log_verbose("read raw %s, off=0x%x,len=%u", pt->name, address, length);

    if (!(address + length <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    return jekv_partition_read(pt->dev, pt->offset + address, data, length);
}

int jekv_pt_write_raw(jekv_partition_t *pt, uint32_t address, const void *data, uint32_t length)
{
    jekv_log_debug("write raw %s, off=0x%x,len=%u", pt->name, address, length);

    if (!(address + length <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    if (pt->readonly) {
        return JEKV_ERR_READ_ONLY;
    }

    return jekv_partition_write(pt->dev, pt->offset + address, (uint8_t *)data, length);
}

int jekv_pt_read_item(jekv_partition_t *pt, uint32_t address, jekv_item_t *item)
{
    jekv_log_verbose("read item %s, off=0x%x,len=%u", pt->name, address, (uint32_t)sizeof(*item));

    if (!(address + sizeof(*item) <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    return jekv_partition_read(pt->dev, pt->offset + address, (uint8_t *)item, (uint32_t)sizeof(*item));
}

int jekv_pt_write_item(jekv_partition_t *pt, uint32_t address, const jekv_item_t *item)
{
    jekv_log_debug("write item %s, off=0x%x,len=%u", pt->name, address, (uint32_t)sizeof(*item));

    if (!(address + sizeof(*item) <= pt->size)) {
        return JEKV_ERR_INVALID_PARAM;
    }

    if (pt->readonly) {
        return JEKV_ERR_READ_ONLY;
    }

    return jekv_partition_write(pt->dev, pt->offset + address, (uint8_t *)item, sizeof(*item));
}
