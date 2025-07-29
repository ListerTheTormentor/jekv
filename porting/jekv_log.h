#ifndef __JEKV_LOG_H__
#define __JEKV_LOG_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JKEV_LOG_NONE       0
#define JKEV_LOG_ERROR      1
#define JKEV_LOG_WARNING    2
#define JKEV_LOG_INFO       3
#define JKEV_LOG_DEBUG      4
#define JKEV_LOG_VERBOSE    5

#define CONFIG_JEKV_LOG_LEVEL      JKEV_LOG_NONE


#if !defined(LOG_TAG)
#define LOG_TAG "NO_TAG"
#endif


#if CONFIG_JEKV_LOG_LEVEL >= JKEV_LOG_ERROR
#define jekv_log_error(fmt, ...) jekv_log_printf(JKEV_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define jekv_log_error(fmt, ...) ((void)0);
#endif

#if CONFIG_JEKV_LOG_LEVEL >= JKEV_LOG_WARNING
#define jekv_log_warning(fmt, ...) jekv_log_printf(JKEV_LOG_WARNING, LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define jekv_log_warning(fmt, ...) ((void)0);
#endif

#if CONFIG_JEKV_LOG_LEVEL >= JKEV_LOG_INFO
#define jekv_log_info(fmt, ...) jekv_log_printf(JKEV_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define jekv_log_info(fmt, ...) ((void)0);
#endif

#if CONFIG_JEKV_LOG_LEVEL >= JKEV_LOG_DEBUG
#define jekv_log_debug(fmt, ...) jekv_log_printf(JKEV_LOG_DEBUG, LOG_TAG, fmt,  ##__VA_ARGS__)
#else
#define jekv_log_debug(fmt, ...) ((void)0);
#endif

#if CONFIG_JEKV_LOG_LEVEL >= JKEV_LOG_VERBOSE
#define jekv_log_verbose(fmt, ...) jekv_log_printf(JKEV_LOG_VERBOSE, LOG_TAG,  fmt, ##__VA_ARGS__)
#else
#define jekv_log_verbose(fmt, ...) ((void)0);
#endif


#define JEKV_RAWE(...)              printf( __VA_ARGS__)
#define JEKV_DUMPE(hint, width, buf, size) jekv_log_dump(LOG_TAG,hint, width, (void*)(buf), size)

int jekv_log_printf(int level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));
int jekv_log_dump(const char* tag, const char* name,int width,void* buf, int data_len);

#ifdef __cplusplus
}
#endif

#endif
