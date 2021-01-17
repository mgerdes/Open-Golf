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

struct mscript_function_decl_arg {
    struct mscript_type type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
};

struct mscript_function_decl {
    struct mscript_type return_type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1]; 
    int num_args;
    struct mscript_function_decl_arg args[MSCRIPT_MAX_FUNCTION_ARGS];
};

struct mscript_struct_decl_arg {
    struct mscript_type type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
};

struct mscript_struct_decl {
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    int num_members;
    struct mscript_struct_decl_arg members[MSCRIPT_MAX_STRUCT_MEMBERS];
};

struct mscript_program;
void mscript_compile_2(const char *prog_text);

#endif
