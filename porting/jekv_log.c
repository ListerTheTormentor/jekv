#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "jekv_log.h"

__attribute__((weak)) int jekv_log_printf(int level, const char *tag, const char *format, ...)
{
    va_list list;
    va_start(list, format);

    printf("[%s] ",tag);
    vprintf(format,list);
    printf("\n");

    va_end(list);

    return 0;
}

__attribute__((weak)) int jekv_log_dump(const char* tag, const char* name,int width,void* buf, int data_len)
{
    uint8_t* data = (uint8_t*)buf;

    if (!(tag && name && width > 0 && width <= 32 && data && data_len > 0)) {
        return -1;
    }

    uint8_t *ptr_line;
    char line_buf[10 + 9 + 1 + width * 4]; /*10 bytes address, 9 bytes space, 4 * witdh character*/
    char *p_line;
    int line_size;
    uint32_t count = 0;
    int32_t len    = data_len;

    jekv_log_printf(JKEV_LOG_ERROR, tag, "dump [%s]\r\n", name);

    while (len > 0) {
        if (len > width) {
            line_size = width;
        } else {
            line_size = len;
        }

        /*current line*/
        ptr_line = data;
        p_line   = line_buf;

        /*add address*/
        p_line += sprintf(p_line, "0x%04x ", count);

        /*dump hex*/
        for (int i = 0; i < width; i++) {
            if ((i & 7) == 0) {
                p_line += sprintf(p_line, " ");
            }
            if (i < line_size) {
                p_line += sprintf(p_line, " %02x", (uint8_t)ptr_line[i]);
            } else {
                p_line += sprintf(p_line, "   ");
            }
        }

        /*dump characters*/
        p_line += sprintf(p_line, "  ");
        for (int i = 0; i < line_size; i++) {
            if (isprint((int)ptr_line[i])) {
                p_line += sprintf(p_line, "%c", ptr_line[i]);
            } else {
                p_line += sprintf(p_line, ".");
            }
        }

        /*output line*/
        printf(name, "%s\r\n", line_buf);

        data = (uint8_t *)data + line_size;
        len -= line_size;
        count += line_size;
    }

    return 0;
}