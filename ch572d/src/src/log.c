#include "log.h"
#include "usb/usb_logs.h"
#include <stdarg.h>
#include <stdio.h>

#define LOG_BUF_LEN 128

int log_printf(const char *fmt, ...)
{
    char buf[LOG_BUF_LEN];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) {
        return len;
    }

    if (len >= (int)sizeof(buf)) {
        buf[sizeof(buf) - 1] = '\0';
        len = (int)sizeof(buf) - 1;
    }

    usb_logs_push(buf);
    return len;
}