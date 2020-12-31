#define _CRT_SECURE_NO_WARNINGS

#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void m_log(const char *msg) {
    FILE *f = fopen("log.txt", "a");
    if (f) {
        fprintf(f, "%s", msg);
        fclose(f);
    }
}

void m_logf(const char *fmt, ...) {
    char str[1024];
    va_list arg;
    va_start(arg, fmt);
    vsprintf(str, fmt, arg);
    va_end(arg);
    m_log(str);
}
