#ifndef __JEKV_BASE_H__
#define __JEKV_BASE_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup JEKV_Macros Macros
 * @brief JEKV Macros
 */

/**
 * @addtogroup JEKV_Macros
 * @{
 */

/**
  * @brief  Flash partition name size
  * size of partition name
  */
#define JEKV_PARTITION_NAME_SIZE 16

/**
  * @brief  Flash sector size
  * size of sector
  */
#define JEKV_SECTOR_SIZE      4096

/**
  * @brief  kv handle max num
  * Max num of kv handle
  */
#define JEKV_MAX_HANDLE_NUM     64

/**
  * @brief  kv key or group name max size
  * Max size of kv key, maximum 15 valid characters support, not include '\0'
  */
#define JEKV_MAX_KEY_LEN        15

/**
  * @brief  Mininum BLOB segment size, Not include item segment header
  */
#define JEKV_BLOB_MIN_SEG_SIZE  512

/**
  * @brief  kv error code
  */
#define JEKV_ERR_BASE           -30000               /**< Base number of error codes    */
#define JEKV_ERR_OK             0                    /**< OK                            */
#define JEKV_ERR_FAIL           (JEKV_ERR_BASE - 1)  /**< failure                       */
#define JEKV_ERR_INVALID_PARAM  (JEKV_ERR_BASE - 2)  /**< invalid params                */
#define JEKV_ERR_NOT_INIT       (JEKV_ERR_BASE - 3)  /**< The kv is not initialized     */
#define JEKV_ERR_AREADY_INIT    (JEKV_ERR_BASE - 4)  /**< The kv is aready initialized  */
#define JEKV_ERR_NO_MEM         (JEKV_ERR_BASE - 5)  /**< No enough memory              */
#define JEKV_ERR_NOT_FOUND      (JEKV_ERR_BASE - 6)  /**< gourp or key not found        */
#define JEKV_ERR_READ_ONLY      (JEKV_ERR_BASE - 7)  /**< partition is read only        */
#define JEKV_ERR_NO_SPACE       (JEKV_ERR_BASE - 8)  /**< no enough space for saving    */
#define JEKV_ERR_INVALID_HANDLE (JEKV_ERR_BASE - 9)  /**< invalid handle                */
#define JEKV_ERR_SECTOR_FULL    (JEKV_ERR_BASE - 10) /**< sector is full                */
#define JEKV_ERR_INVALID_LENGTH (JEKV_ERR_BASE - 11) /**< invalid data length           */
#define JEKV_ERR_VALUE_TOO_LONG (JEKV_ERR_BASE - 12) /**< Value is too long             */
#define JEKV_ERR_CALL_IN_ISR    (JEKV_ERR_BASE - 13) /**< call in isr error             */

/**
 * @}
 */

/**
 * @defgroup JEKV_Enumerations KV Enumerations
 * @brief WinnerMicro KV Enumerations
 */

/**
 * @addtogroup JEKV_Enumerations
 * @{
 */

/**
* @enum     jekv_open_mode_t
* @brief    kv open mode
*/
typedef enum {
    JEKV_OP_READ_ONLY  = 0, /**< read only mode      */
    JEKV_OP_READ_WRITE = 1, /**< read and write mode */
    JEKV_OP_MAX,
} jekv_open_mode_t;

/**
* @enum     jekv_type_t
* @brief    kv item data type
*/
typedef enum {
    JEKV_TYPE_ANY,    /**< any type             */
    JEKV_TYPE_STRING, /**< string end with '\0', max length 4031 */
    JEKV_TYPE_INT8,   /**< int8_t               */
    JEKV_TYPE_UINT8,  /**< uint8_t              */
    JEKV_TYPE_INT16,  /**< int16_t              */
    JEKV_TYPE_UINT16, /**< uint16_t             */
    JEKV_TYPE_INT32,  /**< int32_t              */
    JEKV_TYPE_UINT32, /**< uint32_t             */
    JEKV_TYPE_INT64,  /**< int64_t              */
    JEKV_TYPE_UINT64, /**< uint64_t             */
    JEKV_TYPE_DOUBLE, /**< float data           */
    JEKV_TYPE_BINARY, /**< small binary data, size from 1~4032, not split to segments */
    JEKV_TYPE_BLOB,   /**< Binary Large OBject, It will be divided into multiple segments (1~127) for storage.
                          Except the last piece, other segments are no less than 512 bytes. In addition,
                          a description item will also be stored ,all the size no larger than 512064 */
    JEKV_TYPE_MAX
} jekv_type_t;

/**
 * @}
 */

/**
 * @defgroup JEKV_Structures KV Structures
 * @brief WinnerMicro KV Structures
 */

/**
 * @addtogroup JEKV_Structures
 * @{
 */

/**
  * @brief  kv handle , handle read and write items
  */
typedef void *jekv_handle_t;

/**
  * @brief  iterator , handle for poll all kv items
  */
typedef struct jekv_iterator_info_t *jekv_iterator_t;

/**
 * @struct  jekv_entry_t
 * @brief   iterator entry information
 */
typedef struct {
    char group[JEKV_MAX_KEY_LEN + 1]; /**< item group     */
    char key[JEKV_MAX_KEY_LEN + 1];   /**< item key       */
    jekv_type_t type;                 /**< item type      */
    uint8_t group_id;                   /**< group id       */
    uint32_t length;                      /**< data length    */
} jekv_entry_t;

/**
 * @struct  jekv_status_t
 * @brief   kv status
 */
typedef struct {
    uint32_t total_size;  /**< total size         */
    uint32_t using_size;  /**< using item size    */
    uint32_t droped_size; /**< drop item size     */
    uint32_t free_size;   /**< free size          */
    uint8_t group_num;    /**< num of groups      */
    uint8_t handle_num;   /**< num of the handles */
} jekv_status_t;

/**
 * @}
 */

/**
 * @defgroup JEKV_Functions KV Functions
 * @brief WinnerMicro KV Functions
 */

/**
 * @addtogroup JEKV_Functions
 * @{
 */

/**
  * @brief  Initialize kv
  *
  * @param[in]  partition_name kv partition name
  *
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_NO_MEM: no memery
  *    - JEKV_ERR_FAIL: failed
  * @note This API must be called before all other kv API can be called
  */
int jekv_init(const char *partition_name);

/**
  * @brief  Deinitialize kv
  *
  * @param[in]  partition_name kv partition name
  *
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_FAIL: failed
  * @note The handle open by the api jekv_open must be closed before jekv_deinit called.
  */
int jekv_deinit(const char *partition_name);

/**
  * @brief  erase the whole kv partition
  *
  * @param[in]  partition_name kv partition name
  *
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_FAIL: failed
  *
  * @warning All the data in the partition will be lost after jekv_erase called. jekv_deinit shoud be called
  *     first if the partition is initialized. It is usually called when setting factory recovery.
  *     It is better to reboot the system after calling this interface.
  */
int jekv_erase(const char *partition_name);

/**
  * @brief  Show all kv information
  *
  * @param[in]  partition_name kv partition name
  * @param[in]  group_name kv group name, NULL for all group in partition
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_FAIL: failed
  */
int jekv_print(const char *partition_name, const char *group_name);

/**
  * @brief  open kv operation handle
  *
  * @param[in]  partition_name kv partition name, it will use default kv partition if partition_name is null.
  * @param[in]  group_name kv group name, max characters is 15 .
  * @param[in]  mode read write or only read, @ref jekv_open_mode_t
  * @param[out]  handle kv operation handle
  *
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_FAIL: failed
  * @note shoud be closed by jekv_close if it is not used.
  */
int jekv_open(const char *partition_name, const char *group_name, jekv_open_mode_t mode, jekv_handle_t *handle);

/**
  * @brief  close kv operation handle
  *
  * @param[in]  handle kv operation handle,obtained from jekv_open.
  *
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_FAIL: failed
  */
int jekv_close(jekv_handle_t handle);

/**
 * @brief  Get string by key name
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    string data
 * @param[inout]  length    input out_value buffer size, output real value size
 *                          return JEKV_ERR_INVALID_PARAM if buf is not enough;
 *                          "hello" need 6 bytes, the out_value will be: "hello"\0
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error or buffer is not enough
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_str(jekv_handle_t handle, const char *key, char *out_value, uint32_t *length);

/**
 * @brief  Save string data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  value    string data,must be end of \0
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_str(jekv_handle_t handle, const char *key, const char *value);

/**
 * @brief  Get small binary data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]     key      kv name
 * @param[out]    data     binary data
 * @param[inout]  data_len binary data length
 *
 * Notes:
 *   If data is empty, data_len returns the actual length to hold the data.
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error or buffer is not enough
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_binary(jekv_handle_t handle, const char *key, void *data, uint32_t *data_len);

/**
 * @brief  Save small binary data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  data     binary data
 * @param[in]  data_len binary data length
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_binary(jekv_handle_t handle, const char *key, const void *data, uint32_t data_len);

/**
 * @brief  Get Binary Large OBject
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]     key      kv name
 * @param[out]    blob     blob data
 * @param[inout]  blob_len blob data length
 *
 * Notes:
 *   If blob is empty, blob_len returns the actual length to hold the data.
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error or buffer is not enough
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_blob(jekv_handle_t handle, const char *key, void *blob, uint32_t *blob_len);

/**
 * @brief  Save binary data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  blob     binary data
 * @param[in]  blob_len binary data length
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_blob(jekv_handle_t handle, const char *key, const void *blob, uint32_t blob_len);
/**
 * @brief  Save int8 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    int8 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_i8(jekv_handle_t handle, const char *key, int8_t value);

/**
 * @brief  Save uint8 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    uint8 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_u8(jekv_handle_t handle, const char *key, uint8_t value);

/**
 * @brief  Save int16 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    int16 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_i16(jekv_handle_t handle, const char *key, int16_t value);

/**
 * @brief  Save uint16 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    uint16 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_u16(jekv_handle_t handle, const char *key, uint16_t value);

/**
 * @brief  Save int32 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    int32 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_i32(jekv_handle_t handle, const char *key, int32_t value);

/**
 * @brief  Save uint32 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    uint32 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_u32(jekv_handle_t handle, const char *key, uint32_t value);

/**
 * @brief  Save int64 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    int64 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_i64(jekv_handle_t handle, const char *key, int64_t value);

/**
 * @brief  Save uint64 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    uint64 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_u64(jekv_handle_t handle, const char *key, uint64_t value);

/**
 * @brief  Save float data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[in]  value    float value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL save item fail
 */
int jekv_set_float(jekv_handle_t handle, const char *key, double value);

/**
 * @brief  Get int8 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    int8 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_i8(jekv_handle_t handle, const char *key, int8_t *out_value);

/**
 * @brief  Get uint8 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    uint8 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_u8(jekv_handle_t handle, const char *key, uint8_t *out_value);

/**
 * @brief  Get int16 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    int16 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_i16(jekv_handle_t handle, const char *key, int16_t *out_value);

/**
 * @brief  Get uint16 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    uint16 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_u16(jekv_handle_t handle, const char *key, uint16_t *out_value);

/**
 * @brief  Get int32 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    int32 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_i32(jekv_handle_t handle, const char *key, int32_t *out_value);

/**
 * @brief  Get uint32 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    uint32 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_u32(jekv_handle_t handle, const char *key, uint32_t *out_value);

/**
 * @brief  Get int64 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    int64 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_i64(jekv_handle_t handle, const char *key, int64_t *out_value);

/**
 * @brief  Get uint64 data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    uint64 value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_u64(jekv_handle_t handle, const char *key, uint64_t *out_value);

/**
 * @brief  Get float data
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key      kv name
 * @param[out]  out_value    float value
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_get_float(jekv_handle_t handle, const char *key, double *out_value);

/**
 * @brief  Delete the kv data for the key
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key kv name
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND item is not found
 */
int jekv_del_key(jekv_handle_t handle, const char *key);

/**
  * @brief  reset kv. It is usually called at factory settings, and the system is restarted after the kv reset.
  *
  * @param[in]  handle kv operation handle,obtained from jekv_open.
  *
  * @return
  *    - JEKV_ERR_OK: succeed
  *    - JEKV_ERR_FAIL: failed
  * @warning Calling this interface will clear all group data and cannot be recovered.
  */
int jekv_del_group(jekv_handle_t handle);

/**
 * @brief  find iterator, used for poll all or specified items.
 *
 * @param[in]  partition_name  kv partition name
 * @param[in]  group    group name
 * @param[in]  type    data type,JEKV_TYPE_ANY for all types
 * @param[out]  output_iterator    iterator
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL fail
 * @note The output iterator handle must be release by jekv_release_iterator
 */
int jekv_entry_find(const char *partition_name, const char *group, jekv_type_t type, jekv_iterator_t *output_iterator);

/**
 * @brief  find iterator, used for poll all or specified items.
 *
 * @param[in]  handle  kv operation handle, obtained from jekv_open
 * @param[in]  type    data type,JEKV_TYPE_ANY for all types
 * @param[out]  output_iterator    iterator
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL fail
 * @note The output iterator handle must be release by jekv_release_iterator
 */
int jekv_entry_find_by_handle(jekv_handle_t handle, jekv_type_t type, jekv_iterator_t *output_iterator);

/**
 * @brief  change to next iterator
 *
 * @param[inout]  iterator  iterator to next item
 *                out iterator:
 *                  ! NULL : next item
 *                  NULL   : iterator end, the memery is released
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL iterator end or fail
 */
int jekv_entry_next(jekv_iterator_t *iterator);

/**
 * @brief  get iterator info
 *
 * @param[in]  iterator  iterator
 * @param[out]  info    iterator entry information
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL fail
 */
int jekv_entry_info(jekv_iterator_t iterator, jekv_entry_t *info);

/**
 * @brief  get iterator data
 *
 * @param[in]  iterator  iterator
 * @param[out]  data    iterator entry information
 * @param[inout]  data_len  data len, input buffer len, output real len,
 *                     return JEKV_ERR_INVALID_PARAM if input data_len is not enough
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_FAIL fail
 */
int jekv_entry_data(jekv_iterator_t iterator, void *data, uint32_t *data_len);

/**
 * @brief  get iterator data
 *
 * @param[in]  iterator  iterator
 *             release iteraotr memory
 *
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 * @note
 *  iterator can be NULL
 */
int jekv_release_iterator(jekv_iterator_t iterator);

/**
 * @brief  get item type and size
 *
 * @param[in]  handle kv operation handle,obtained from jekv_open.
 * @param[in]  key  kv item name
 * @param[out]  type  data type
 * @param[out]  size  data size
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 *         - JEKV_ERR_NOT_FOUND
 */
int jekv_get_info(jekv_handle_t handle, const char *key, jekv_type_t *type, uint32_t *size);

/**
 * @brief  get kv status
 *
 * @param[out]  partition_name  kv partition name
 * @param[out]  status  kv status, @ref jekv_status_t
 * @return
 *         - JEKV_ERR_OK on success
 *         - JEKV_ERR_INVALID_PARAM param error
 */
int jekv_get_status(const char *partition_name, jekv_status_t *status);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* end of __JEKV_H__ */
