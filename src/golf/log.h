#ifndef _GOLF_LOG_H
#define _GOLF_LOG_H

void golf_log_init(void);
void golf_log_note(const char *fmt, ...);
void golf_log_warning(const char *fmt, ...);
void golf_log_error(const char *fmt, ...);

int golf_log_get_entry_count(void);
char *golf_log_get_entry(int i);

#endif
