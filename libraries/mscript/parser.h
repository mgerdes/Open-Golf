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
    MSCRIPT_TYPE_CHAR_STAR,
    MSCRIPT_TYPE_STRUCT,
    MSCRIPT_TYPE_ARRAY,
};

struct mscript;
struct mscript_program;
struct mscript_type;

struct mscript *mscript_create(void);
struct mscript_program *mscript_load_program(struct mscript *mscript, const char *name);

#endif
