#ifndef _AUDIO_H
#define _AUDIO_H

#include <stdbool.h>

void audio_init(void);
void audio_start_sound(const char *name, const char *filename, float volume, bool repeat, bool force);
void audio_stop_sound(const char *name, float t);
void audio_get_samples(float *buffer, int num_samples, float dt);

#endif
