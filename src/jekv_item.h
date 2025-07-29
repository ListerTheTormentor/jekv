#ifndef __JEKV_ITEM_H__
#define __JEKV_ITEM_H__

#include <stdint.h>
#include "jekv_base.h"
#include "jekv_porting.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JEKV_SLICE_SIZE                32
#define JEKV_SLICE_NUM                 (JEKV_SECTOR_SIZE / 32)
#define JEKV_ENTRY_COUNT               (JEKV_SLICE_NUM - 1)

#define JEKV_SECTOR_STATE_OFF_SET      2

#define JEKV_SINGLE_ITEM_MAX_DATA_SIZE ((JEKV_ENTRY_COUNT - 1) * JEKV_SLICE_SIZE)
#define JEKV_ENTRY_DATA_OFFSET         JEKV_SLICE_SIZE

#define JEKV_GROUP_ID_MAX              0x3f                /* group id max     */
#define JEKV_GROUP_ID_ANY              JEKV_GROUP_ID_MAX   /* invlaid group id */
#define JEKV_GROUP_ITSELF_ID           0x0                 /* id for group itself  */

#define JEKV_SEG_ID_ANY                0xff /* Not use as valid seg id    */
#define JEKV_SEG_NUM_MAX               127

/**
  * @brief  kv type internal
  */
#define JEKV_TYPE_BLOB_SEG             JEKV_TYPE_MAX       /**< BLOB data segment */
#define JEKV_TYPE_ANY_WITHOUT_SEG      (JEKV_TYPE_MAX + 1) /**< Type any and not exclude blog segment */

/**
  * @brief  kv item state
  */
typedef enum {
    JEKV_ITEM_STATE_UNUSED = 0xff, /* 1111 1111 unused   */
    JEKV_ITEM_STATE_USING  = 0xfe, /* 1111 1110 using    */
    JEKV_ITEM_STATE_DROPED = 0xfc, /* 1111 1100 droped   */
} jekv_item_state_t;

typedef enum {
    JEKV_SEG_START_VER_0 = 0x0,  /* start from 0     */
    JEKV_SEG_START_VER_1 = 0x80, /* start from 0x80  */
    JEKV_SEG_START_ANY   = 0xff,
} jekv_seg_start_t;

typedef struct {
    uint8_t state;
    char name[JEKV_MAX_KEY_LEN]; /**< item name     */

    struct {
        uint8_t group_id : 6; /**< group id        */
        uint8_t recv1    : 2; /**< reseve          */
        uint8_t seg_id;       /**< seg id          */
        uint16_t type   : 4;  /**< type            */
        uint16_t length : 12; /**< length          */
    };

    uint32_t crc_item; /**< crc32 for head  */

    union {
        struct {               /*blob desc                     */
            uint32_t all_size; /**< for blob segs all size     */
            uint8_t seg_count; /**< for blob desc      */
            uint8_t seg_start; /**< for seg start      */
            uint16_t resv2;
        };

        struct {               /**< string(len > 8) or blob data   */
            uint32_t crc_data; /**< for data                       */
            uint32_t resv3;
        };
        uint8_t data[8]; /**< primitive data or (string <= 8) */
    };
} jekv_item_t;

#define JEKV_ITEM_CRC_LEN (JEKV_SLICE_SIZE - 1)

uint32_t jekv_item_crc_hash(const jekv_item_t *item);
uint32_t jekv_item_crc_head(const jekv_item_t *item);

int jekv_item_get_span(jekv_item_t *item);
void jekv_item_print_item_head(jekv_item_t *item);

/*
Init item, Not init crc and data field
*/
int jekv_item_init(jekv_item_t *item, uint8_t state, uint8_t gid, jekv_type_t type, const char *key, const void *data,
                     int size, uint8_t seg_id);

#ifdef __cplusplus
}
#endif

#endif
