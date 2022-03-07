#include "common/audio.h"

#include "3rd_party/stb/stb_vorbis.h"
#include "sokol/sokol_audio.h"

#include "common/data.h"
#include "common/map.h"

typedef struct _sound {
    bool repeat, on, stopping;
    float stopping_length, stopping_t;
    float volume;
    int cur_pos;
    stb_vorbis *stream;
} _sound_t;

typedef map_t(_sound_t) _map_sound_t;

static _map_sound_t _sounds;

void golf_audio_init(void) {
    saudio_setup(&(saudio_desc){
            .sample_rate = 44100,
            .buffer_frames = 1024,
            .packet_frames = 64,
            .num_packets = 32, 
            });

    golf_data_load("data/audio/confirmation_002.ogg", false);
    golf_data_load("data/audio/drop_001.ogg", false);
    golf_data_load("data/audio/drop_003.ogg", false);
    golf_data_load("data/audio/error_008.ogg", false);
    golf_data_load("data/audio/footstep_grass_004.ogg", false);
    golf_data_load("data/audio/impactPlank_medium_000.ogg", false);
    golf_data_load("data/audio/in_water.ogg", false);
    map_init(&_sounds, "audio");
}

void golf_audio_start_sound(const char *name, const char *path, float volume, bool repeat, bool force) {
    golf_audio_t *audio = golf_data_get_audio(path);

    _sound_t *sound = map_get(&_sounds, name);
    if (!sound) {
        _sound_t s = { 0 };
        s.on = false;
        map_set(&_sounds, name, s);
        sound = map_get(&_sounds, name);
    }

    if (!sound->on || force) {
        sound->on = true;
        sound->volume = volume;
        sound->stopping = false;
        sound->repeat = repeat;
        sound->cur_pos = 0;
        sound->stream = (stb_vorbis*)audio->stb_vorbis_stream;
    }
}

void golf_audio_stop_sound(const char *name, float t) {
    _sound_t *sound = map_get(&_sounds, name);
    if (!sound) {
        return;
    }
    if (sound->stopping || !sound->on) {
        return;
    }

    sound->stopping = true;
    sound->stopping_length = t;
    sound->stopping_t = 0;
    if (t == 0) {
        sound->on = false;
    }
}

void golf_audio_update(float dt) {
    int num_samples = saudio_expect();
    if (num_samples <= 0) {
        return;
    }

    float *buffer = golf_alloc(sizeof(float) * num_samples);
    float *buffer2 = golf_alloc(sizeof(float) * num_samples);
    for (int i = 0; i < num_samples; i++) {
        buffer[i] = 0;
    }

    const char *key;
    map_iter_t iter = map_iter(&m);
    while ((key = map_next(&_sounds, &iter))) {
        _sound_t *s = map_get(&_sounds, key);
        float scale = 1;
        if (!s->on) continue;
        if (s->stopping) {
            s->stopping_t += dt;
            if (s->stopping_t > s->stopping_length) {
                s->stopping_t = s->stopping_length;
                s->stopping = false;
                s->on = false;
            }
            scale = 1 - s->stopping_t / s->stopping_length;
        }

        stb_vorbis_seek(s->stream, s->cur_pos);
        int n = stb_vorbis_get_samples_float(s->stream, 1, &buffer2, num_samples);
        for (int j = n; j < num_samples; j++) {
            buffer2[j] = 0;
        }
        for (int j = 0; j < num_samples; j++) {
            float a = buffer[j];
            float b = s->volume * scale * buffer2[j];
            float sign = (float) ((a + b) > 0 ? 1 : -1);
            buffer[j] = a + b - a * b * sign;
        }
        if (n == 0) {
            if (s->repeat) {
                s->cur_pos = 0;
            }
            else {
                s->on = false;
            }
        }
        s->cur_pos += n;
    }

    saudio_push(buffer, num_samples);
    golf_free(buffer2);
    golf_free(buffer);
}
