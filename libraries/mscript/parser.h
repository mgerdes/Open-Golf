#ifndef _MSCRIPT_PARSER_H
#define _MSCRIPT_PARSER_H

#include <stdbool.h>

#define MSCRIPT_MAX_SYMBOL_LEN 31
#define MSCRIPT_MAX_FUNCTION_ARGS 15
#define MSCRIPT_MAX_STRUCT_MEMBERS 15

enum mscript_type_type {
    MSCRIPT_TYPE_VOID,
    MSCRIPT_TYPE_VOID_STAR,
    MSCRIPT_TYPE_INT,
    MSCRIPT_TYPE_FLOAT,
    MSCRIPT_TYPE_VEC3,
    MSCRIPT_TYPE_BOOL,
    MSCRIPT_TYPE_CHAR_STAR,
    MSCRIPT_TYPE_STRUCT,
    MSCRIPT_TYPE_ENUM,
    MSCRIPT_TYPE_ARRAY,
};

struct mscript;
typedef struct mscript mscript_t;

struct mscript_program;
typedef struct mscript_program mscript_program_t;

struct mscript_type;
typedef struct mscript_type mscript_type_t;

mscript_t *mscript_create(void);
mscript_program_t *mscript_load_program(mscript_t *mscript, const char *name);

#endif
