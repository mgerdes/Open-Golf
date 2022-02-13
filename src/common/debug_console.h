#ifndef _GOLF_DEBUG_CONSOLE_H
#define _GOLF_DEBUG_CONSOLE_H

void golf_debug_console_init(void);
void golf_debug_console_update(float dt);
void golf_debug_console_add_tab(const char *name, void (*fn)(void));

#endif
