#define _CRT_SECURE_NO_WARNINGS

#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void m_log(const char *msg) {
    printf("%s", msg);
}

void m_logf(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
}
