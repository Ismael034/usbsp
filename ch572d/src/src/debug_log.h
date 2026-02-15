#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include "log.h"

// CH32V203 uses printf("[LEVEL] ...\r\n"). Mirror that style on CH572D,
// but route output over the USB CDC "logs" interface via log_printf().

// Uncomment to suppress debug logs.
// #define NDEBUG

#ifdef NDEBUG
#define LOG_DEBUG(fmt, ...) ((void)0)
#else
#define LOG_DEBUG(fmt, ...) log_printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#endif

#define LOG_INFO(fmt, ...)  log_printf("[INFO] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_printf("[WARN] " fmt "\r\n", ##__VA_ARGS__)

#endif

