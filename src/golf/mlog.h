#ifndef _MLOG_H
#define _MLOG_H

void mlog_init(void);
void mlog_note(const char *fmt, ...);
void mlog_warning(const char *fmt, ...);
void mlog_error(const char *fmt, ...);

int mlog_get_entry_count(void);
char *mlog_get_entry(int i);

#endif
