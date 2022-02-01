#ifndef _GOLF_LOG_H
#define _GOLF_LOG_H

typedef enum golf_log_level {
    GOLF_LOG_LEVEL_NOTE,
    GOLF_LOG_LEVEL_WARNING,
    GOLF_LOG_LEVEL_ERROR,
} golf_log_level_t;

void golf_log_init(void);
void golf_log(golf_log_level_t level, const char *fmt, ...);
void golf_log_note(const char *fmt, ...);
void golf_log_warning(const char *fmt, ...);
void golf_log_error(const char *fmt, ...);

int golf_log_get_entry_count(void);
char *golf_log_get_entry(int i);

#endif
