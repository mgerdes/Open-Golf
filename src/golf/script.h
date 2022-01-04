#ifndef _GOLF_SCRIPT_H
#define _GOLF_SCRIPT_H

#include "golf/maths.h"
#include "golf/string.h"

typedef struct golf_script golf_script_t;
typedef struct gs_eval gs_eval_t;
typedef struct gs_val gs_val_t;
typedef vec_t(gs_val_t) vec_gs_val_t;

typedef enum gs_val_type {
    GS_VAL_VOID,
    GS_VAL_BOOL,
    GS_VAL_INT,
    GS_VAL_FLOAT,
    GS_VAL_VEC2,
    GS_VAL_VEC3,
    GS_VAL_LIST,
    GS_VAL_STRING,
    GS_VAL_FN,
    GS_VAL_C_FN,
    GS_VAL_ERROR,
    GS_VAL_NUM_TYPES,
} gs_val_type;

typedef struct gs_fn {
    gs_val_type return_type;
    const char *name;
    int num_args;
    gs_val_type *arg_types;
    const char **arg_names;
    void *stmt;
} gs_fn_t;

typedef struct gs_val {
    gs_val_type type;
    bool is_return;
    union {
        bool bool_val;
        int int_val;
        float float_val;
        vec2 vec2_val;
        vec3 vec3_val;
        vec_gs_val_t *list_val;
        golf_string_t *string_val;
        gs_fn_t *fn_val;
        gs_val_t (*c_fn)(gs_eval_t *eval, gs_val_t *vals, int num_vals);
        const char *error_val;
    };
} gs_val_t;

golf_script_t *golf_script_new(const char *src);

void golf_script_init(void);

#endif
