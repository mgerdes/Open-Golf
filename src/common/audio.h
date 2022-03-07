#ifndef _GOLF_AUDIO_H
#define _GOLF_AUDIO_H

#include <stdbool.h>

void golf_audio_init(void);
void golf_audio_start_sound(const char *name, const char *path, float volume, bool repeat, bool force);
void golf_audio_stop_sound(const char *name, float t);
void golf_audio_update(float dt);

#endif
