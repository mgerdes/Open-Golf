#ifndef _GOLF_SCRIPT_H
#define _GOLF_SCRIPT_H

#include "golf/file.h"
#include "golf/maths.h"
#include "golf/string.h"

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
typedef vec_t(gs_val_type) vec_gs_val_type_t;

typedef struct gs_eval gs_eval_t;
typedef struct gs_val gs_val_t;
typedef vec_t(gs_val_t) vec_gs_val_t;
typedef struct gs_stmt gs_stmt_t;
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
        gs_stmt_t *fn_stmt;
        gs_val_t (*c_fn)(gs_eval_t *eval, gs_val_t *vals, int num_vals);
        const char *error_val;
    };
} gs_val_t;
typedef map_t(gs_val_t) map_gs_val_t;

typedef enum gs_token_type {
    GS_TOKEN_INT,
    GS_TOKEN_FLOAT,
    GS_TOKEN_SYMBOL,
    GS_TOKEN_CHAR,
    GS_TOKEN_STRING,
    GS_TOKEN_EOF,
} gs_token_type;

typedef struct gs_token {
    gs_token_type type;
    int line, col;
    union {
        int int_val;
        float float_val;
        char *symbol;
        char *string;
        char c;
    };
} gs_token_t;
typedef vec_t(gs_token_t) vec_gs_token_t;

#define MAX_ERROR_STRING_LEN 2048
typedef struct gs_parser {
    vec_gs_token_t tokens;
    int cur_token;

    bool error;
    char error_string[MAX_ERROR_STRING_LEN];
    gs_token_t error_token;
    vec_void_t allocated_memory;
} gs_parser_t;

typedef enum gs_binary_op_type {
    GS_BINARY_OP_ADD,
    GS_BINARY_OP_SUB,
    GS_BINARY_OP_MUL,
    GS_BINARY_OP_DIV,
    GS_BINARY_OP_LT,
    GS_BINARY_OP_GT,
    GS_BINARY_OP_LTE,
    GS_BINARY_OP_GTE,
    GS_BINARY_OP_EQ,
    GS_NUM_BINARY_OPS,
} gs_binary_op_type;

typedef enum gs_expr_type {
    GS_EXPR_BINARY_OP,
    GS_EXPR_ASSIGNMENT,
    GS_EXPR_INT,
    GS_EXPR_FLOAT,
    GS_EXPR_SYMBOL,
    GS_EXPR_STRING,
    GS_EXPR_CALL,
    GS_EXPR_MEMBER_ACCESS,
    GS_EXPR_ARRAY_ACCESS,
    GS_EXPR_ARRAY_DECL,
    GS_EXPR_CAST,
} gs_expr_type;

typedef struct gs_expr gs_expr_t;
typedef struct gs_expr {
    gs_expr_type type;
    gs_token_t token;
    union {
        int int_val;
        float float_val;
        const char *symbol;
        const char *string;

        struct {
            gs_binary_op_type type;
            gs_expr_t *left, *right;
        } binary_op;

        struct {
            gs_expr_t *left, *right;
        } assignment;

        struct {
            gs_expr_t *fn;
            int num_args;
            gs_expr_t **args;
        } call;

        struct {
            gs_expr_t *val;
            gs_token_t member;
        } member_access;

        struct {
            gs_expr_t *val, *arg;
        } array_access;

        struct {
            int num_args;
            gs_expr_t **args;
        } array_decl;

        struct {
            gs_val_type type;
            gs_expr_t *arg;
        } cast;
    }; 
} gs_expr_t;
typedef vec_t(gs_expr_t*) vec_gs_expr_t;

typedef enum gs_stmt_type {
    GS_STMT_IF,
    GS_STMT_FOR,
    GS_STMT_RETURN,
    GS_STMT_BLOCK,
    GS_STMT_EXPR,
    GS_STMT_VAR_DECL,
    GS_STMT_FN_DECL,
} gs_stmt_type;

typedef struct gs_stmt gs_stmt_t;
typedef struct gs_stmt {
    gs_stmt_type type;
    union {
        gs_expr_t *expr;

        struct {
            int num_conds;
            gs_expr_t **conds;
            gs_stmt_t **stmts;
            gs_stmt_t *else_stmt;
        } if_stmt;

        struct {
            gs_val_type decl_type;
            gs_token_t decl_symbol;
            gs_expr_t *init, *cond, *inc;
            gs_stmt_t *body;
        } for_stmt;

        struct {
            gs_expr_t *expr;
        } return_stmt;

        struct {
            int num_stmts;
            gs_stmt_t **stmts;
        } block_stmt;

        struct {
            gs_val_type type;
            int num_ids;
            gs_token_t *tokens;
            gs_expr_t *init;
        } var_decl;

        struct {
            gs_val_type return_type;
            gs_token_t symbol;
            int num_args;
            gs_val_type *arg_types;
            gs_token_t *arg_symbols;
            gs_stmt_t *body;
        } fn_decl;
    };
} gs_stmt_t;
typedef vec_t(gs_stmt_t*) vec_gs_stmt_t;

typedef struct gs_env {
    map_gs_val_t val_map;
} gs_env_t;
typedef vec_t(gs_env_t*) vec_gs_env_t;

typedef struct gs_eval {
    vec_void_t allocated_strings, allocated_lists;
    vec_gs_env_t env;
} gs_eval_t; 

typedef struct golf_script {
    char path[GOLF_FILE_MAX_PATH];
    const char *error;
    gs_parser_t parser;
    gs_eval_t eval; 
} golf_script_t;
typedef vec_t(golf_script_t*) vec_golf_script_ptr_t;

typedef struct golf_script_store {
    vec_golf_script_ptr_t scripts;
} golf_script_store_t;

void golf_script_store_init(void);
golf_script_store_t *golf_script_store_get(void);
bool golf_script_load(golf_script_t *script, const char *path, const char *data, int data_len);
bool golf_script_unload(golf_script_t *script);

#endif
