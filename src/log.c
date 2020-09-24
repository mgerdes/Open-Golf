#define _CRT_SECURE_NO_WARNINGS

#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void minigolf_log(const char *msg) {
    FILE *f = fopen("log.txt", "a");
    if (f) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(f, "%02d:%02d | %s\n", tm.tm_hour, tm.tm_min, msg);
        fclose(f);
    }
}

void minigolf_logf(const char *fmt, ...) {
    char str[1024];
    va_list arg;
    va_start(arg, fmt);
    vsprintf(str, fmt, arg);
    va_end(arg);
    minigolf_log(str);
}
