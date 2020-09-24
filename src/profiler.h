#ifndef _PROFILER_H
#define _PROFILER_H

#include <stdbool.h>

#include "array.h"
#include "map.h"
#include "sokol_time.h"

#define PROFILER_SECTION_POOL_SIZE 10000
#define PROFILER_SECTION_MAX_NAME_LEN 32
#define PROFILER_SECTION_MAX_VARS 5
#define PROFILER_SECTION_MAX_VAR_NAME_LEN 16

struct profiler_section_info {
    char name[PROFILER_SECTION_MAX_NAME_LEN];
    int n;
    float avg_dt;
};
typedef map_t(struct profiler_section_info) map_profiler_section_info_t;
array_t(struct profiler_section_info, array_profiler_section_info);

struct profiler_section {
    struct profiler_section *parent;

    int num_vars;
    char var_names[PROFILER_SECTION_MAX_VARS][PROFILER_SECTION_MAX_VAR_NAME_LEN];
    int var_values[PROFILER_SECTION_MAX_VARS];

    char name[PROFILER_SECTION_MAX_NAME_LEN];
    uint64_t start_time, end_time;
    float dt;
};

struct profiler_state {
    bool is_paused;
    int num_frames;
    struct profiler_section *cur_section;
    int num_section_pushes, num_section_pops;

    map_profiler_section_info_t section_info_map;

    struct profiler_section section_pool[PROFILER_SECTION_POOL_SIZE];
    int section_pool_num_free, section_pool_free_idx[PROFILER_SECTION_POOL_SIZE];

    bool record_allocations;
};

struct profiler_state *profiler_get_state(void);
void profiler_init(void);
void profiler_pause(void);
void profiler_start_frame(void);
void profiler_finish_frame(void);
void profiler_push_section(const char *name);
void profiler_pop_section(void);
void profiler_section_add_var(const char *name, int val);

#endif
