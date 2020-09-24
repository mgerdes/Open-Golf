#define _CRT_SECURE_NO_WARNINGS

#include "profiler.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "sokol_time.h"

static struct profiler_state *_profiler_state;

static struct profiler_section *alloc_section() {
    assert(_profiler_state->section_pool_num_free > 0);
    int idx = _profiler_state->section_pool_free_idx[--_profiler_state->section_pool_num_free];
    return &_profiler_state->section_pool[idx];
}

static void free_section(struct profiler_section *section) {
    int idx = (int) (section - _profiler_state->section_pool);
    _profiler_state->section_pool_free_idx[_profiler_state->section_pool_num_free++] = idx;
}

struct profiler_state *profiler_get_state(void) {
    return _profiler_state;
}

void profiler_init(void) {
    _profiler_state = malloc(sizeof(struct profiler_state));
    _profiler_state->is_paused = false;
    _profiler_state->num_frames = 0;
    _profiler_state->section_pool_num_free = PROFILER_SECTION_POOL_SIZE;
    for (int i = 0; i < PROFILER_SECTION_POOL_SIZE; i++) {
        _profiler_state->section_pool_free_idx[i] = i;
    }

    _profiler_state->record_allocations = false;
    map_init(&_profiler_state->section_info_map);
    _profiler_state->record_allocations = true;
    _profiler_state->cur_section = NULL;
}

void profiler_pause(void) {
    _profiler_state->is_paused = true;
}

void profiler_start_frame(void) {
    _profiler_state->num_section_pushes = 0;
    _profiler_state->num_section_pops = 0;
    profiler_push_section("frame");
}

void profiler_finish_frame(void) {
    profiler_pop_section();
    assert(_profiler_state->num_section_pushes == _profiler_state->num_section_pops);
}

void profiler_push_section(const char *name) {
    struct profiler_section *section = alloc_section();
    strncpy(section->name, name, PROFILER_SECTION_MAX_NAME_LEN);
    section->name[PROFILER_SECTION_MAX_NAME_LEN - 1] = 0;
    section->start_time = stm_now();
    section->num_vars = 0;

    section->parent = _profiler_state->cur_section;
    _profiler_state->cur_section = section;
    _profiler_state->num_section_pushes++;
}

void profiler_pop_section(void) {
    struct profiler_section *section = _profiler_state->cur_section;
    section->end_time = stm_now();
    section->dt = (float) stm_ms(stm_diff(section->end_time, section->start_time));

    struct profiler_section_info *info = map_get(&_profiler_state->section_info_map, section->name);
    if (!info) {
        struct profiler_section_info info;
        strncpy(info.name, section->name, PROFILER_SECTION_MAX_NAME_LEN);
        info.name[PROFILER_SECTION_MAX_NAME_LEN - 1] = 0;
        info.n = 1;
        info.avg_dt = section->dt;
        map_set(&_profiler_state->section_info_map, section->name, info);
    }
    else {
        if (info->n > 60) {
            info->n = 0;
            info->avg_dt = 0.0f;
        }
        info->avg_dt = info->avg_dt + (section->dt - info->avg_dt) / (info->n + 1);
        info->n++;
    }

    _profiler_state->cur_section = section->parent;
    _profiler_state->num_section_pops++;

    free_section(section);
}

void profiler_section_add_var(const char *name, int val) {
    struct profiler_section *section = _profiler_state->cur_section;
    if (section->num_vars < PROFILER_SECTION_MAX_VARS) {
        strncpy(section->var_names[section->num_vars], name, PROFILER_SECTION_MAX_VAR_NAME_LEN);
        section->var_names[section->num_vars][PROFILER_SECTION_MAX_VAR_NAME_LEN - 1] = 0;
        section->var_values[section->num_vars] = val;
        section->num_vars++;
    }
}
