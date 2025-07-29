#include "jekv_handler.h"

int jekv_handler_open(jekv_storage_t *storage, uint8_t group_id, jekv_open_mode_t mode, jekv_handle_info_t **handle)
{
    jekv_handle_info_t *ctx = JEKV_CALLOC(1, sizeof(*ctx));
    if (ctx) {
        dl_list_init(&ctx->list);
        ctx->storage  = storage;
        ctx->group_id = group_id;
        ctx->mode     = mode;
        ctx->valid    = 1;
        *handle       = ctx;
        return JEKV_ERR_OK;
    } else {
        return JEKV_ERR_NO_MEM;
    }
}

int jekv_handler_close(jekv_handle_info_t *handle)
{
    JEKV_FREE(handle);
    return JEKV_ERR_OK;
}
