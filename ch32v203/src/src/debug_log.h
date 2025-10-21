#include <stdio.h>

#ifdef NDEBUG
#define LOG_DEBUG(fmt, ...) ((void)0)  /* No-op for debug logs when NDEBUG is defined */
#else
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#endif

#define LOG_INFO(fmt, ...)  printf("[INFO] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("[WARN] " fmt "\r\n", ##__VA_ARGS__)