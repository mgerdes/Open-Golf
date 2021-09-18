#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "mcommon.h"

typedef struct _mlog_entry {
    char msg[1024];
} _mlog_entry_t;

typedef struct _mlog_state {
    int entry_count;
    _mlog_entry_t entries[32];
} _mlog_state_t;

_mlog_state_t _state;

typedef enum _mlog_level {
    _mlog_level_note,
    _mlog_level_warning,
    _mlog_level_error,
} _mlog_level_t;

void mlog_init(void) {
    _state.entry_count = 0;
}

static void _log(enum _mlog_level level, const char *fmt, va_list arg) {
    vprintf(fmt, arg);
    if (level == _mlog_level_warning) {
        if (_state.entry_count < 32) {
            _mlog_entry_t *entry = &_state.entries[_state.entry_count++];
            vsnprintf(entry->msg, 1024, fmt, arg);
        }
    }
    else if (level == _mlog_level_error) {
        assert(false);
    }
}

void mlog_note(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(_mlog_level_note, fmt, ap);
    va_end(ap);
}

void mlog_warning(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(_mlog_level_warning, fmt, ap);
    va_end(ap);
}

void mlog_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(_mlog_level_error, fmt, ap);
    va_end(ap);
}

int mlog_get_entry_count(void) {
    return _state.entry_count;
}

char *mlog_get_entry(int i) {
    return _state.entries[i].msg;
}
