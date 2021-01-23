#ifndef _MSCRIPT_PARSER_H
#define _MSCRIPT_PARSER_H

#include <stdbool.h>

#define MSCRIPT_MAX_SYMBOL_LEN 32
#define MSCRIPT_MAX_FUNCTION_ARGS 16
#define MSCRIPT_MAX_STRUCT_MEMBERS 16

enum mscript_type_type {
    MSCRIPT_TYPE_VOID,
    MSCRIPT_TYPE_VOID_STAR,
    MSCRIPT_TYPE_INT,
    MSCRIPT_TYPE_FLOAT,
    MSCRIPT_TYPE_STRUCT,
    MSCRIPT_TYPE_ARRAY,
    MSCRIPT_TYPE_STRING,
};

struct mscript_type {
    enum mscript_type_type type;

    enum mscript_type_type array_type;
    char *struct_name; 
};

struct mscript;
struct mscript_program;

struct mscript *mscript_create(void);
struct mscript_program *mscript_load_program(struct mscript *mscript, const char *name);

#endif
