#include "audio.h"

#include "file.h"
#include "map.h"
#include "stb_vorbis.h"

typedef map_t(stb_vorbis*) map_stb_vorbis_t;
static map_stb_vorbis_t streams_map;

struct audio_sound {
    bool repeat, on, stopping;
    float stopping_length, stopping_t;
    float volume;
    int cur_pos; 
    stb_vorbis *stream;
};

typedef map_t(struct audio_sound) map_audio_sound_t;
map_audio_sound_t audio_sound_map;

void audio_init(void) {
    map_init(&streams_map);
    map_init(&audio_sound_map);

	struct directory dir;
	directory_init(&dir, "assets/audio", false);
    for (int i = 0; i < dir.num_files; i++) {
        struct file f = dir.files[i];
        file_load_data(&f);
        int err;
        stb_vorbis *stream = stb_vorbis_open_memory((unsigned char*)f.data, f.data_len, &err, NULL);
        assert(stream && !err);
        stb_vorbis_seek(stream, 0);
        map_set(&streams_map, f.name, stream);
    }
    directory_deinit(&dir);
}

void audio_start_sound(const char *name, const char *filename, float volume, bool repeat, bool force) {
    stb_vorbis **stb_stream = map_get(&streams_map, filename);
    if (!stb_stream) {
        return;
    }

    struct audio_sound *sound = map_get(&audio_sound_map, name);     
    if (!sound) {
        struct audio_sound s = { 0 };
        s.on = false;
        map_set(&audio_sound_map, name, s);
        sound = map_get(&audio_sound_map, name);
    }

    if (!sound->on || force) {
        sound->on = true;
        sound->volume = volume;
        sound->stopping = false;
        sound->repeat = repeat;
        sound->cur_pos = 0;
        sound->stream = *stb_stream;
    }
}

void audio_stop_sound(const char *name, float t) {
    struct audio_sound *sound = map_get(&audio_sound_map, name); 
    if (!sound) {
        return;
    }
    if (sound->stopping || !sound->on) {
        return;
    }

    sound->stopping = true;
    sound->stopping_length = t;
    sound->stopping_t = 0.0f;
    if (t == 0.0f) {
        sound->on = false;
    }
}

void audio_get_samples(float *buffer, int num_samples, float dt) {
    for (int i = 0; i < num_samples; i++) {
        buffer[i] = 0.0f;
    }

    float *buffer2 = malloc(sizeof(float)*num_samples);

    const char *key;
    map_iter_t iter = map_iter(&m);
    while ((key = map_next(&audio_sound_map, &iter))) {
        struct audio_sound *s = map_get(&audio_sound_map, key);
        float scale = 1.0f;
        if (!s->on) continue;
        if (s->stopping) {
            s->stopping_t += dt;
            if (s->stopping_t > s->stopping_length) {
                s->stopping_t = s->stopping_length;
                s->stopping = false;
                s->on = false;
            }
            scale = 1.0f - s->stopping_t/s->stopping_length;
        }

        stb_vorbis_seek(s->stream, s->cur_pos);
        int n = stb_vorbis_get_samples_float(s->stream, 1, &buffer2, num_samples);
        for (int j = n; j < num_samples; j++) {
            buffer2[j] = 0.0f;
        }
        for (int j = 0; j < num_samples; j++) {
            float a = buffer[j];
            float b = s->volume*scale*buffer2[j];
            float sign = (a + b) > 0.0f ? 1.0f : -1.0f;
            buffer[j] = a + b - a*b*sign;
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
    free(buffer2);
}
