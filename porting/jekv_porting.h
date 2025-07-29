#ifndef __JEKV_PORTING_H__
#define __JEKV_PORTING_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JKEV_USE_FREERTOS   1

#define JKEV_PLATFORM_PC    1
#define JKEV_PLATFORM_XX    2

#define JKEV_PLATFORM_USE   JKEV_PLATFORM_XX


#define JEKV_MALLOC(size)           malloc(size)
#define JEKV_REALLOC(ptr, size)     realloc(ptr, size)
#define JEKV_CALLOC(nelem, elsize)  calloc(nelem, elsize)
#define JEKV_FREE(ptr)              free(ptr)

typedef struct {
    uint32_t offset;                   /**< partition offset */
    uint32_t size;                     /**< partition size   */
} jkvs_partition_item_t;

extern size_t strnlen(const char *s, size_t maxlen);

int jekv_port_init(void);
int jekv_port_deinit(void);
int jekv_port_is_init(void);
int jekv_port_mutex_lock(void);
int jekv_port_mutex_unlock(void);

/* flash porting interface*/
void* jekv_partition_open(const char *partition_name);
int jekv_partition_get_info(const char* partition_name, jkvs_partition_item_t* info);
int jekv_partition_erase(void* dev, uint32_t offset, uint32_t size);
int jekv_partition_read(void* dev, uint32_t offset, uint8_t* data, uint32_t length);
int jekv_partition_write(void* dev, uint32_t offset, uint8_t* data, uint32_t length);

uint32_t jekv_port_crc32(uint32_t crc, const void *buf, uint32_t len);
void jekv_port_power_off(int type, int stage);

#ifdef __cplusplus
}
#endif

#endif
