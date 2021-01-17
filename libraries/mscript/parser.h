#ifndef _MSCRIPT_PARSER_H
#define _MSCRIPT_PARSER_H

#include <stdbool.h>

#define MSCRIPT_MAX_SYMBOL_LEN 16
#define MSCRIPT_MAX_FUNCTION_ARGS 16
#define MSCRIPT_MAX_STRUCT_MEMBERS 16

enum mscript_type_type {
    MSCRIPT_TYPE_VOID,
    MSCRIPT_TYPE_INT,
    MSCRIPT_TYPE_FLOAT,
    MSCRIPT_TYPE_STRUCT,
    MSCRIPT_TYPE_ARRAY,
};

struct mscript_type {
    enum mscript_type_type type;

    enum mscript_type_type array_type;
    char *struct_name; 
};

struct mscript_program;
void mscript_compile_2(const char *prog_text);

#endif
