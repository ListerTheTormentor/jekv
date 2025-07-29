
#ifndef __JEKV_PARTITION_H__
#define __JEKV_PARTITION_H__

#include <stdint.h>
#include "jekv_porting.h"
#include "jekv_item.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *dev;                             /**< flash device        */
    char name[JEKV_PARTITION_NAME_SIZE];   /**< partition name      */
    uint32_t offset;                       /**< partition offset    */
    uint16_t sec_num;                      /**< sector num          */
    uint16_t sec_size;                     /**< sector size         */
    uint32_t size;                         /**< all partitoin size  */
    uint8_t encrypted;                     /**< partition encrypted */
    uint8_t readonly;                      /**< partition readonly  */
} jekv_partition_t;

/*init partition*/
int jekv_pt_init(const char *partition_name, jekv_partition_t *pt);

/*erase all the partition*/
int jekv_pt_erase_all(jekv_partition_t* pt);

/*erase partition sector*/
int jekv_pt_erase(jekv_partition_t *pt, uint32_t address);

/*read with encryption if configured*/
int jekv_pt_read(jekv_partition_t *pt, uint32_t address, void *data, uint32_t length);

/*write with encryption if configured*/
int jekv_pt_write(jekv_partition_t *pt, uint32_t address, const void *data, uint32_t length);

/*read without encryption*/
int jekv_pt_read_raw(jekv_partition_t *pt, uint32_t address, void *data, uint32_t length);

/*read without encryption*/
int jekv_pt_write_raw(jekv_partition_t *pt, uint32_t address, const void *data, uint32_t length);

/*read item, The first 16 bytes are encrypted, and the last 16 bytes are not encrypted*/
int jekv_pt_read_item(jekv_partition_t *pt, uint32_t address, jekv_item_t *item);

/*write item, The first 16 bytes are encrypted, and the last 16 bytes are not encrypted*/
int jekv_pt_write_item(jekv_partition_t *pt, uint32_t address, const jekv_item_t *item);

#ifdef __cplusplus
}
#endif

#endif
