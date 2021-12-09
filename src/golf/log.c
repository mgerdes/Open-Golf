#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "dbgtools/callstack.h"
#include "golf/log.h"

typedef struct _golf_log_entry {
    char msg[1024];
} _golf_log_entry_t;

typedef struct _golf_log_state {
    int entry_count;
    _golf_log_entry_t entries[32];
} _golf_log_state_t;

_golf_log_state_t _state;

void golf_log_init(void) {
    _state.entry_count = 0;
}

static void _print_callstack(void) {
    void *addresses[256];
    int i;
    int num_addresses = callstack(3, addresses, 256);

    callstack_symbol_t symbols[256];
    char symbols_buffer[2048];
    num_addresses = callstack_symbols(addresses, symbols, num_addresses, symbols_buffer, 2048 );

    for (i = 0; i < num_addresses; i++) {
        printf( "%3d) %-50s %s(%u)\n", i, symbols[i].function, symbols[i].file, symbols[i].line );
    }
}

static void _log(golf_log_level_t level, const char *fmt, va_list arg) {
    if (level == GOLF_LOG_LEVEL_WARNING) {
        printf("WARNING: ");
    }
    else if (level == GOLF_LOG_LEVEL_ERROR) {
        printf("ERROR: ");
    }
    vprintf(fmt, arg);
    printf("\n");
    if (level == GOLF_LOG_LEVEL_WARNING) {
        //_print_callstack();
        //if (_state.entry_count < 32) {
            //_golf_log_entry_t *entry = &_state.entries[_state.entry_count++];
            //vsnprintf(entry->msg, 1024, fmt, arg);
        //}
    }
    else if (level == GOLF_LOG_LEVEL_ERROR) {
        _print_callstack();
        assert(false);
    }
}

void golf_log(golf_log_level_t level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(level, fmt, ap);
    va_end(ap);
}

void golf_log_note(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(GOLF_LOG_LEVEL_NOTE, fmt, ap);
    va_end(ap);
}

void golf_log_warning(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(GOLF_LOG_LEVEL_WARNING, fmt, ap);
    va_end(ap);
}

void golf_log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _log(GOLF_LOG_LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

int golf_log_get_entry_count(void) {
    return _state.entry_count;
}

char *golf_log_get_entry(int i) {
    return _state.entries[i].msg;
}
