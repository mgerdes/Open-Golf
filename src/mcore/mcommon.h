#ifndef _MCOMMON_H
#define _MCOMMON_H

#if defined(__APPLE__)
    #if defined(TARGET_OS_IPHONE) && !TARGET_OS_IPHONE
        #define MARS_PLATFORM_MACOS (1)
    #else
        #define MARS_PLATFORM_IOS (1)
    #endif
#elif defined(__EMSCRIPTEN__)
    #define MARS_PLATFORM_HTML (1)
#elif defined(_WIN32)
    #define MARS_PLATFORM_WIN32 (1)
#elif defined(__ANDROID__)
    #define MARS_PLATFORM_ANDROID (1)
#elif defined(__linux__) || defined(__unix__)
    #define  MARS_PLATFORM_LINUX (1)
#else
#error "Unknown platform"
#endif

#include <stdbool.h>

void mstrncpy(char *dest, const char *src, int n);
unsigned char *mbase64_decode(const char *src, int len, int *dec_len); 
char *mbase64_encode(const unsigned char *src, int len);
bool mwrite_file(const char *path, unsigned char *data, int data_len);
bool mread_file(const char *path, unsigned char **data, int *data_len);
bool mstr_copy_line(char **str, char **line_buffer, int *line_buffer_cap);

#endif
