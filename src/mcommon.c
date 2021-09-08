#include "mcommon.h"

#include <string.h>

void mstrncpy(char *dest, const char *src, int n) {
    strncpy(dest, src, n);
    dest[n - 1] = 0;
}
