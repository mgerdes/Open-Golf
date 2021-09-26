#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "3rd_party/dbgtools/callstack.h"

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

static void _print_callstack() {
    void *addresses[256];
    int i;
    int num_addresses = callstack(0, addresses, 256);

    callstack_symbol_t symbols[256];
    char symbols_buffer[1024];
    num_addresses = callstack_symbols( addresses, symbols, num_addresses, symbols_buffer, 1024 );

    for (i = 0; i < num_addresses; i++) {
        printf( "%3d) %-50s %s(%u)\n", i, symbols[i].function, symbols[i].file, symbols[i].line );
    }
}

static void _log(enum _mlog_level level, const char *fmt, va_list arg) {
    vprintf(fmt, arg);
    printf("\n");
    if (level == _mlog_level_warning) {
        _print_callstack();
        if (_state.entry_count < 32) {
            _mlog_entry_t *entry = &_state.entries[_state.entry_count++];
            vsnprintf(entry->msg, 1024, fmt, arg);
        }
    }
    else if (level == _mlog_level_error) {
        _print_callstack();
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
