#define _CRT_SECURE_NO_WARNINGS
#include "mscript/parser.h"

#include <assert.h>
#include <stdbool.h>

#include "array.h"
#include "vec.h"
#include "file.h"
#include "hotloader.h"
#include "log.h"
#include "map.h"
#include "sokol_time.h"

struct _ms_stmt;
typedef struct _ms_stmt _ms_stmt_t;

struct _ms_expr;
typedef struct _ms_expr _ms_expr_t;

struct parser;
struct compiler;
struct vm;

enum opcode_type {
    OPCODE_IADD,
    OPCODE_FADD,
    OPCODE_ISUB,
    OPCODE_FSUB,
    OPCODE_IMUL,
    OPCODE_FMUL,
    OPCODE_IDIV,
    OPCODE_FDIV,
    OPCODE_ILTE,
    OPCODE_FLTE,
    OPCODE_ILT,
    OPCODE_FLT,
    OPCODE_IGTE,
    OPCODE_FGTE,
    OPCODE_IGT,
    OPCODE_FGT,
    OPCODE_IEQ,
    OPCODE_FEQ,
    OPCODE_INEQ,
    OPCODE_FNEQ,
    OPCODE_IINC,
    OPCODE_FINC,
    OPCODE_NOT,
    OPCODE_F2I,
    OPCODE_I2F,
    OPCODE_COPY,
    OPCODE_INT,
    OPCODE_FLOAT,
    OPCODE_LOCAL_STORE,
    OPCODE_LOCAL_LOAD,
    OPCODE_JF,
    OPCODE_JMP,
    OPCODE_CALL,
    OPCODE_RETURN,
    OPCODE_PUSH,
    OPCODE_POP,
    OPCODE_ARRAY_CREATE,
    OPCODE_ARRAY_STORE,
    OPCODE_ARRAY_LOAD,
    OPCODE_ARRAY_LENGTH,
    OPCODE_DEBUG_PRINT_INT,
    OPCODE_DEBUG_PRINT_FLOAT,
    OPCODE_DEBUG_PRINT_BOOL,
    OPCODE_DEBUG_PRINT_STRING,
    OPCODE_DEBUG_PRINT_STRING_CONST,

    // Intermediate opcodes
    OPCODE_INTERMEDIATE_LABEL,
    OPCODE_INTERMEDIATE_FUNC,
    OPCODE_INTERMEDIATE_CALL,
    OPCODE_INTERMEDIATE_JMP,
    OPCODE_INTERMEDIATE_JF,
    OPCODE_INTERMEDIATE_STRING,
};

struct opcode {
    enum opcode_type type;

    union {
        int label;
        int int_val;
        float float_val;
        int size;
        char string[MSCRIPT_MAX_SYMBOL_LEN + 1];

        struct {
            int offset, size;
        } load_store;

        struct {
            int label, args_size;
        } call;

        char *intermediate_string;
    };
};
typedef vec_t(struct opcode) _vec_opcode_t;

typedef enum _ms_const_val_type {
    _MS_CONST_VAL_INT,
    _MS_CONST_VAL_FLOAT,
    _MS_CONST_VAL_BOOL,
    _MS_CONST_VAL_OBJECT,
} _ms_const_val_type_t;

typedef struct _ms_const_val {
    _ms_const_val_type_t type;
    union {
        float float_val;
        int int_val;
        bool bool_val;

        struct {
            int num_args;
            struct _ms_const_val *args;
        } object;
    };
} _ms_const_val_t;

static _ms_const_val_t _ms_const_val_float(float float_val);
static _ms_const_val_t _ms_const_val_int(int int_val);
static _ms_const_val_t _ms_const_val_bool(bool bool_val);
static _ms_const_val_t _ms_const_val_object(int num_args, _ms_const_val_t *args);

typedef enum _ms_lvalue_type {
    LVALUE_INVALID,
    LVALUE_LOCAL,
    LVALUE_ARRAY,
} _ms_lvalue_type_t;

typedef struct _ms_lvalue {
    _ms_lvalue_type_t type;
    int offset;
} _ms_lvalue_t;

_ms_lvalue_t _ms_lvalue_invalid(void);
_ms_lvalue_t _ms_lvalue_local(int offset);
_ms_lvalue_t _ms_lvalue_array(void);

typedef enum _ms_token_type {
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_SYMBOL,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_EOF,
} _ms_token_type_t;

typedef struct _ms_token {
    _ms_token_type_t type;
    int line, col;
    union {
        char *symbol;
        char *string;
        int int_val;
        float float_val;
        char char_val;
    };
} _ms_token_t;
typedef vec_t(_ms_token_t) _vec_ms_token_t;

static bool is_char_digit(char c);
static bool is_char_start_of_symbol(char c);
static bool is_char_part_of_symbol(char c);
static bool is_char(char c);
static _ms_token_t _ms_token_number(const char *text, int *len, int line, int col);
static _ms_token_t _ms_token_char(char c, int line, int col);
static _ms_token_t _ms_token_string(mscript_program_t *program, const char *text, int *len, int line, int col);
static _ms_token_t _ms_token_symbol(const char *text, int *len, int line, int col);
static _ms_token_t _ms_token_eof(int line, int col);
static void _ms_tokenize(mscript_program_t *program);

typedef struct _ms_parsed_type {
    char string[MSCRIPT_MAX_SYMBOL_LEN + 3];
} _ms_parsed_type_t;
typedef vec_t(_ms_parsed_type_t) _vec_ms_parsed_type_t;

static _ms_parsed_type_t _ms_parsed_type(const char *name, bool is_array);

typedef struct _ms_mem {
    size_t bytes_allocated;
    vec_void_t ptrs;
} _ms_mem_t;

static void _ms_mem_init(_ms_mem_t *mem);
static void _ms_mem_deinit(_ms_mem_t *mem);
static void *_ms_mem_alloc(_ms_mem_t *mem, size_t size);

struct parser {
    const char *prog_text;

    _ms_mem_t mem;

    int token_idx;
    _vec_ms_token_t tokens;

    char *error;
    _ms_token_t error_token;
};

static void parser_init(struct parser *parser, const char *prog_text);
static void parser_deinit(struct parser *program);
static _ms_token_t peek(mscript_program_t *program);
static _ms_token_t peek_n(mscript_program_t *program, int n);
static void eat(mscript_program_t *program); 
static bool match_char(mscript_program_t *program, char c);
static bool match_char_n(mscript_program_t *program, int n, ...);
static bool match_symbol(mscript_program_t *program, const char *symbol);
static bool match_symbol_n(mscript_program_t *program, int n, ...);
static bool match_eof(mscript_program_t *program);
static bool check_type(mscript_program_t *program);
static bool is_char_token(_ms_token_t tok, char c);

typedef enum _ms_symbol_type {
    _MS_SYMBOL_LOCAL_VAR,
    _MS_SYMBOL_GLOBAL_VAR,
    _MS_SYMBOL_CONST,
    _MS_SYMBOL_FUNCTION,
    _MS_SYMBOL_TYPE,
} _ms_symbol_type_t;

typedef struct _ms_function_decl_arg {
    mscript_type_t *type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
} _ms_function_decl_arg_t;

typedef struct _ms_function_decl {
    struct array_int labels; 
    _vec_opcode_t opcodes;
    int block_size, args_size;
    mscript_type_t *return_type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1]; 
    int num_args;
    _ms_function_decl_arg_t args[MSCRIPT_MAX_FUNCTION_ARGS];
} _ms_function_decl_t;
typedef vec_t(_ms_function_decl_t*) _vec_ms_function_decl_ptr_t;

typedef struct _ms_global_decl {
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    mscript_type_t *type;

    bool has_initial_value;
    _ms_const_val_t inital_value;
} _ms_global_decl_t;
typedef vec_t(_ms_global_decl_t*) _vec_ms_global_decl_ptr_t;

typedef struct _ms_const_decl {
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    mscript_type_t *type;
    _ms_const_val_t value;
} _ms_const_decl_t;
typedef vec_t(_ms_const_decl_t) _vec_ms_const_decl_t;
static _ms_const_decl_t _ms_const_decl(const char *name, mscript_type_t *type, _ms_const_val_t value);

typedef struct _ms_symbol {
    _ms_symbol_type_t type;

    union {
        struct {
            mscript_type_t *type;
            int offset;
        } local_var;

        struct {
            _ms_global_decl_t *decl;
            int offset;
        } global_var;

        _ms_const_decl_t const_decl;
        _ms_function_decl_t *function_decl;
        mscript_type_t *type_val;
    };
} _ms_symbol_t;
typedef map_t(_ms_symbol_t) _map_ms_symbol_t;

typedef struct _ms_symbol_block {
    int size, total_size;
    _map_ms_symbol_t symbol_map;
} _ms_symbol_block_t;
typedef vec_t(_ms_symbol_block_t) _vec_ms_symbol_block_t;

typedef struct _ms_symbol_table {
    int globals_size;
    _map_ms_symbol_t global_symbol_map;
    _vec_ms_symbol_block_t blocks;
} _ms_symbol_table_t;

static void _ms_symbol_table_init(_ms_symbol_table_t *table);
static void _ms_symbol_table_push_block(_ms_symbol_table_t *table);
static void _ms_symbol_table_pop_block(_ms_symbol_table_t *table);
static void _ms_symbol_table_add_local_var(_ms_symbol_table_t *table, const char *name, mscript_type_t *type);
static void _ms_symbol_table_add_global_decl(_ms_symbol_table_t *table, _ms_global_decl_t *decl);
static void _ms_symbol_table_add_const_decl(_ms_symbol_table_t *table, _ms_const_decl_t decl);
static void _ms_symbol_table_add_function_decl(_ms_symbol_table_t *table, _ms_function_decl_t *decl);
static void _ms_symbol_table_add_type(_ms_symbol_table_t *table, mscript_type_t *type);
static mscript_type_t *_ms_symbol_table_get_type(_ms_symbol_table_t *table, const char *name);
static _ms_function_decl_t *_ms_symbol_table_get_function_decl(_ms_symbol_table_t *table, const char *name);
static _ms_symbol_t *_ms_symbol_table_get(_ms_symbol_table_t *table, const char *name);

static void _ms_verify_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_if_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_for_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_return_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_block_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_expr_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_variable_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return);
static void _ms_verify_import_function_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_verify_function_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt);

static void _ms_verify_expr_with_cast(mscript_program_t *program, _ms_expr_t **expr, mscript_type_t *type);
static void _ms_verify_expr_lvalue(mscript_program_t *program, _ms_expr_t *expr);
static void _ms_verify_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_unary_op_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_binary_op_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_call_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_debug_print_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_member_access_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_assignment_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_int_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_float_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_symbol_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_null_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_string_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_array_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_array_access_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_object_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);
static void _ms_verify_cast_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type);

static void opcode_iadd(mscript_program_t *program);
static void opcode_fadd(mscript_program_t *program);
static void opcode_isub(mscript_program_t *program);
static void opcode_fsub(mscript_program_t *program);
static void opcode_imul(mscript_program_t *program);
static void opcode_fmul(mscript_program_t *program);
static void opcode_idiv(mscript_program_t *program);
static void opcode_fdiv(mscript_program_t *program);
static void opcode_ilte(mscript_program_t *program);
static void opcode_flte(mscript_program_t *program);
static void opcode_ilt(mscript_program_t *program);
static void opcode_flt(mscript_program_t *program);
static void opcode_igte(mscript_program_t *program);
static void opcode_fgte(mscript_program_t *program);
static void opcode_igt(mscript_program_t *program);
static void opcode_fgt(mscript_program_t *program);
static void opcode_ieq(mscript_program_t *program);
static void opcode_feq(mscript_program_t *program);
static void opcode_ineq(mscript_program_t *program);
static void opcode_fneq(mscript_program_t *program);
static void opcode_iinc(mscript_program_t *program);
static void opcode_finc(mscript_program_t *program);
static void opcode_not(mscript_program_t *program);
static void opcode_f2i(mscript_program_t *program);
static void opcode_i2f(mscript_program_t *program);
static void opcode_copy(mscript_program_t *program, int offset, int size);
static void opcode_int(mscript_program_t *program, int val);
static void opcode_float(mscript_program_t *program, float val);
static void opcode_local_store(mscript_program_t *program, int offset, int size);
static void opcode_local_load(mscript_program_t *program, int offset, int size);
static void opcode_jf(mscript_program_t *program, int label);
static void opcode_jmp(mscript_program_t *program, int label);
static void opcode_call(mscript_program_t *program, char *str);
static void opcode_return(mscript_program_t *program, int size);
static void opcode_push(mscript_program_t *program, int size);
static void opcode_pop(mscript_program_t *program, int size);
static void opcode_array_create(mscript_program_t *program, int size);
static void opcode_array_store(mscript_program_t *program, int size);
static void opcode_array_load(mscript_program_t *program, int size);
static void opcode_array_length(mscript_program_t *program);
static void opcode_debug_print_int(mscript_program_t *program);
static void opcode_debug_print_float(mscript_program_t *program);
static void opcode_debug_print_bool(mscript_program_t *program);
static void opcode_debug_print_string(mscript_program_t *program);
static void opcode_debug_print_string_const(mscript_program_t *program, char *str);
static void opcode_intermediate_label(mscript_program_t *program, int label);
static void opcode_intermediate_func(mscript_program_t *program, char *str);
static void opcode_intermediate_call(mscript_program_t *program, char *str);
static void opcode_intermediate_jmp(mscript_program_t *program, int label);
static void opcode_intermediate_jf(mscript_program_t *program, int label);
static void opcode_intermediate_string(mscript_program_t *program, char *string);

struct compiler {
    int cur_label;
    _ms_function_decl_t *cur_function_decl;
};

static void compiler_init(mscript_program_t *program);
static void compiler_deinit(mscript_program_t *program);
static void compiler_push_opcode(mscript_program_t *program, struct opcode op);
static int compiler_new_label(mscript_program_t *program);

static void compile_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_if_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_for_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_return_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_block_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_expr_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_function_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt);
static void compile_variable_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt);

static void compile_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_lvalue_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_unary_op_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_binary_op_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_call_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_debug_print_type(mscript_program_t *program, mscript_type_t *type);
static void compile_debug_print_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_array_access_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_member_access_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_assignment_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_int_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_float_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_symbol_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_null_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_string_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_array_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_object_expr(mscript_program_t *program, _ms_expr_t *expr);
static void compile_cast_expr(mscript_program_t *program, _ms_expr_t *expr);

struct vm_array {
    int member_size;
    struct array_char array;
};
array_t(struct vm_array, array_vm_array)

struct vm {
    struct array_char stack;
    struct array_vm_array arrays;
    struct array_char strings;
};

static void vm_init(mscript_program_t *program);
static void vm_run(mscript_program_t *program);

typedef enum _ms_expr_type {
    _MS_EXPR_UNARY_OP,
    _MS_EXPR_BINARY_OP,
    _MS_EXPR_CALL,
    _MS_EXPR_DEBUG_PRINT,
    _MS_EXPR_ARRAY_ACCESS,
    _MS_EXPR_MEMBER_ACCESS,
    _MS_EXPR_ASSIGNMENT,
    _MS_EXPR_INT,
    _MS_EXPR_FLOAT,
    _MS_EXPR_SYMBOL,
    _MS_EXPR_NULL,
    _MS_EXPR_STRING,
    _MS_EXPR_ARRAY,
    _MS_EXPR_OBJECT,
    _MS_EXPR_CAST,
} _ms_expr_type_t;

typedef enum unary_op_type {
    _MS_UNARY_OP_POST_INC,
    _MS_UNARY_OP_LOGICAL_NOT,
} _ms_unary_op_type_t;

typedef enum _ms_binary_op_type {
    _MS_BINARY_OP_ADD,
    _MS_BINARY_OP_SUB,
    _MS_BINARY_OP_MUL,
    _MS_BINARY_OP_DIV,
    _MS_BINARY_OP_LTE,
    _MS_BINARY_OP_LT,
    _MS_BINARY_OP_GTE,
    _MS_BINARY_OP_GT,
    _MS_BINARY_OP_EQ,
    _MS_BINARY_OP_NEQ,
} _ms_binary_op_type_t;

struct _ms_expr {
    _ms_expr_type_t type;
    _ms_token_t token;

    union {
        struct {
            _ms_unary_op_type_t type;
            _ms_expr_t *operand;
        } unary_op;

        struct {
            _ms_binary_op_type_t type;
            _ms_expr_t *left, *right;
        } binary_op;

        struct {
            _ms_expr_t *left, *right;
        } assignment;

        struct {
            _ms_expr_t *left, *right;
        } array_access;

        struct {
            _ms_expr_t *left;
            char *member_name;
        } member_access;

        struct {
            _ms_expr_t *function;
            int num_args;
            _ms_expr_t **args;
        } call;

        struct {
            int num_args;
            _ms_expr_t **args;
        } debug_print;

        struct {
            int num_args;
            _ms_expr_t **args;
        } array;

        struct {
            int num_args;
            char **names;
            _ms_expr_t **args;
        } object;

        struct {
            _ms_parsed_type_t type;
            _ms_expr_t *arg;
        } cast;

        bool bool_value;
        int int_val;
        float float_val;
        char *symbol;
        char *string;
    };

    // set by precompiler
    bool is_const;
    _ms_const_val_t const_val;
    mscript_type_t *result_type;
    _ms_lvalue_t lvalue;
};
typedef vec_t(_ms_expr_t *) _vec_expr_ptr_t;

typedef enum _ms_stmt_type {
    _MS_STMT_IF,
    _MS_STMT_RETURN,
    _MS_STMT_BLOCK,
    _MS_STMT_FUNCTION_DECLARATION,
    _MS_STMT_GLOBAL_DECLARATION,
    _MS_STMT_VARIABLE_DECLARATION,
    _MS_STMT_STRUCT_DECLARATION,
    _MS_STMT_ENUM_DECLARATION,
    _MS_STMT_IMPORT,
    _MS_STMT_IMPORT_FUNCTION,
    _MS_STMT_EXPR,
    _MS_STMT_FOR,
} _ms_stmt_type_t;

struct _ms_stmt {
    _ms_stmt_type_t type;
    _ms_token_t token;

    union {
        struct {
            int num_stmts;
            _ms_expr_t **conds;
            _ms_stmt_t **stmts;
            _ms_stmt_t *else_stmt;
        } if_stmt;

        struct {
            _ms_expr_t *expr;
        } return_stmt;

        struct {
            int num_stmts;
            _ms_stmt_t **stmts;
        } block;

        struct {
            _ms_token_t token;
            _ms_parsed_type_t return_type;
            char *name;
            int num_args;
            _ms_parsed_type_t *arg_types;
            char **arg_names;
            _ms_stmt_t *body;
        } function_declaration;

        struct {
            _ms_parsed_type_t type;
            char *name;
            _ms_expr_t *init_expr;
        } global_declaration;

        struct {
            _ms_parsed_type_t type;
            char *name;
            _ms_expr_t *assignment_expr;
        } variable_declaration;

        struct {
            char *name;
            int num_members;
            _ms_parsed_type_t *member_types;
            char **member_names;
        } struct_declaration;

        struct {
            char *name;
            int num_values;
            char **value_names;
        } enum_declaration;

        struct {
            _ms_expr_t *init, *cond, *inc;
            _ms_stmt_t *body;
        } for_stmt;

        struct {
            char *program_name;
        } import;

        struct {
            _ms_parsed_type_t return_type;
            char *name;
            int num_args;
            _ms_parsed_type_t *arg_types;
            char **arg_names;
        } import_function;

        _ms_expr_t *expr;
    };
};
typedef vec_t(_ms_stmt_t *) _vec_ms_stmt_ptr_t;

static _ms_expr_t *_ms_expr_unary_op_new(_ms_mem_t *mem, _ms_token_t token, _ms_unary_op_type_t type, _ms_expr_t *operand);
static _ms_expr_t *_ms_expr_binary_op_new(_ms_mem_t *mem, _ms_token_t token, _ms_binary_op_type_t type, _ms_expr_t *left, _ms_expr_t *right);
static _ms_expr_t *_ms_expr_assignment_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *left, _ms_expr_t *right);
static _ms_expr_t *_ms_expr_array_access_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *left, _ms_expr_t *right);
static _ms_expr_t *_ms_expr_member_access_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *left, char *member_name);
static _ms_expr_t *_ms_expr_call_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *function, _vec_expr_ptr_t args);
static _ms_expr_t *_ms_expr_debug_print_new(_ms_mem_t *mem, _ms_token_t token, _vec_expr_ptr_t args);
static _ms_expr_t *_ms_expr_array_new(_ms_mem_t *mem, _ms_token_t token, _vec_expr_ptr_t args);
static _ms_expr_t *_ms_expr_object_new(_ms_mem_t *mem, _ms_token_t token, struct array_char_ptr names, _vec_expr_ptr_t args);
static _ms_expr_t *_ms_expr_cast_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t type, _ms_expr_t *expr);
static _ms_expr_t *_ms_expr_int_new(_ms_mem_t *mem, _ms_token_t token, int int_val);
static _ms_expr_t *_ms_expr_float_new(_ms_mem_t *mem, _ms_token_t token, float float_val);
static _ms_expr_t *_ms_expr_symbol_new(_ms_mem_t *mem, _ms_token_t token, char *symbol);
static _ms_expr_t *_ms_expr_null_new(_ms_mem_t *mem, _ms_token_t token);

static _ms_stmt_t *_ms_stmt_if_new(_ms_mem_t *mem, _ms_token_t token, _vec_expr_ptr_t conds, _vec_ms_stmt_ptr_t stmts, _ms_stmt_t *else_stmt);
static _ms_stmt_t *_ms_stmt_return_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *expr);
static _ms_stmt_t *_ms_stmt_block_new(_ms_mem_t *mem, _ms_token_t token, _vec_ms_stmt_ptr_t stmts);
static _ms_stmt_t *_ms_stmt_function_declaration_new(_ms_mem_t *mem, _ms_token_t token,
        _ms_parsed_type_t return_type, char *name, _vec_ms_parsed_type_t arg_types, struct array_char_ptr arg_names, _ms_stmt_t *body);
static _ms_stmt_t *_ms_stmt_global_declaration_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t type, char *name, _ms_expr_t *init_expr);
static _ms_stmt_t *_ms_stmt_variable_declaration_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t type, char *name, _ms_expr_t *assignment_expr);
static _ms_stmt_t *_ms_stmt_struct_declaration_new(_ms_mem_t *mem, _ms_token_t token, char *name,
        _vec_ms_parsed_type_t member_types, struct array_char_ptr member_names);
static _ms_stmt_t *_ms_stmt_enum_declaration_new(_ms_mem_t *mem, _ms_token_t token, char *name, struct array_char_ptr value_names);
static _ms_stmt_t *_ms_stmt_for_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *init, _ms_expr_t *cond, _ms_expr_t *inc, _ms_stmt_t *body);
static _ms_stmt_t *_ms_stmt_import_new(_ms_mem_t *mem, _ms_token_t token, char *program_name);
static _ms_stmt_t *_ms_stmt_import_function_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t return_type, char *name,
        _vec_ms_parsed_type_t arg_types, struct array_char_ptr arg_names);
static _ms_stmt_t *_ms_stmt_expr_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *expr);

static _ms_parsed_type_t _ms_parse_type(mscript_program_t *program);  

static _ms_expr_t *_ms_parse_expr(mscript_program_t *program);
static _ms_expr_t *_ms_parse_expr_assignment(mscript_program_t *program);
static _ms_expr_t *_ms_parse_expr_comparison(mscript_program_t *program);
static _ms_expr_t *_ms_parse_expr_term(mscript_program_t *program);
static _ms_expr_t *_ms_parse_expr_factor(mscript_program_t *program);
static _ms_expr_t *_ms_parse_expr_unary(mscript_program_t *program);
static _ms_expr_t *_ms_parse_expr_member_access_or_array_access(mscript_program_t *program);
static _ms_expr_t *_ms_parse_call_expr(mscript_program_t *program);
static _ms_expr_t *_ms_parse_primary_expr(mscript_program_t *program);
static _ms_expr_t *_ms_parse_array_expr(mscript_program_t *program);
static _ms_expr_t *_ms_parse_object_expr(mscript_program_t *program);

static _ms_stmt_t *_ms_parse_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_if_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_block_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_for_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_return_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_variable_declaration_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_function_declaration_stmt(mscript_program_t *program, _ms_parsed_type_t return_type);
static _ms_stmt_t *_ms_parse_global_declaration_stmt(mscript_program_t *program, _ms_parsed_type_t type);
static _ms_stmt_t *_ms_parse_struct_declaration_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_enum_declaration_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_import_stmt(mscript_program_t *program);
static _ms_stmt_t *_ms_parse_import_function_stmt(mscript_program_t *program);

typedef struct _ms_struct_decl_arg {
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    mscript_type_t *type;
    int offset;
} _ms_struct_decl_arg_t;

typedef struct _ms_struct_decl {
    _ms_stmt_t *stmt;
    int recur_state;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    int num_members;
    _ms_struct_decl_arg_t members[MSCRIPT_MAX_STRUCT_MEMBERS];
} _ms_struct_decl_t;

static mscript_type_t *_ms_struct_decl_get_member(_ms_struct_decl_t *decl, const char *member, int *offset);

struct mscript_type {
    char name[MSCRIPT_MAX_SYMBOL_LEN + 3];
    mscript_type_type_t type;
    mscript_type_t *array_member_type;
    _ms_struct_decl_t *struct_decl;
    int size;
};
typedef vec_t(mscript_type_t*) _vec_mscript_type_ptr_t;

static void _mscript_type_init(mscript_type_t *type, const char *name, mscript_type_type_t type_type,
        mscript_type_t *array_member_type, _ms_struct_decl_t *struct_decl, int size);

typedef vec_t(mscript_program_t *) _vec_program_ptr;

struct mscript_program {
    struct file file;
    bool visited;
    mscript_t *mscript;

    _ms_symbol_table_t symbol_table;

    _vec_mscript_type_ptr_t exported_types;
    _vec_ms_function_decl_ptr_t exported_function_decls;
    _vec_ms_global_decl_ptr_t exported_global_decls;
    _vec_ms_const_decl_t exported_const_decls;

    _ms_mem_t compiler_mem;

    map_int_t func_label_map;

    _vec_program_ptr imported_programs;

    _vec_ms_stmt_ptr_t global_stmts;
    _vec_opcode_t opcodes;
    struct array_char strings;

    struct parser parser;
    struct compiler compiler; 
    struct vm vm;

    _ms_function_decl_t *verify_function_decl;

    char *error;
    _ms_token_t error_token;
};
typedef vec_t(mscript_program_t *) _vec_mscript_program_ptr_t;
typedef map_t(mscript_program_t *) _map_mscript_program_ptr_t;

struct mscript {
    mscript_type_t int_type, int_array_type,
                        float_type, float_array_type,
                        bool_type, bool_array_type,
                        void_type, void_star_type, void_star_array_type,
                        char_star_type;

    _map_mscript_program_ptr_t programs_map;
    _vec_mscript_program_ptr_t programs_array;
};

static void _ms_program_init(mscript_program_t *program, mscript_t *mscript, struct file file);
static void _ms_program_error(mscript_program_t *program, _ms_token_t token, char *fmt, ...);
static void _ms_program_error_no_token(mscript_program_t *program, char *fmt, ...);
static void _ms_program_import_program(mscript_program_t *program, mscript_program_t *import);
static void _ms_program_add_global_decl_stub(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_program_patch_global_decl(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_program_add_enum_decl(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_program_add_struct_decl_stub(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_program_patch_struct_decl(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_program_patch_struct_decl_recur(mscript_program_t *program, mscript_type_t *type);
static void _ms_program_add_function_decl_stub(mscript_program_t *program, _ms_stmt_t *stmt);
static void _ms_program_patch_function_decl(mscript_program_t *program, _ms_stmt_t *stmt);

static int program_add_string(mscript_program_t *program, char *string);
static void _ms_program_load_stage_1(mscript_t *mscript, struct file file);
static void _ms_program_load_stage_2(mscript_t *mscript, mscript_program_t *program);
static void _ms_program_load_stage_3(mscript_t *mscript, mscript_program_t *program);
static void _ms_program_load_stage_4(mscript_t *mscript, mscript_program_t *program);
static void _ms_program_load_stage_5(mscript_t *mscript, mscript_program_t *program);
static void program_load_stage_6(mscript_t *mscript, mscript_program_t *program);
static void program_load_stage_7(mscript_t *mscript, mscript_program_t *program);
static void program_load_stage_8(mscript_t *mscript, mscript_program_t *program);

static void debug_log_token(_ms_token_t token);
static void debug_log_tokens(_ms_token_t *tokens);
static void debug_log_stmt(_ms_stmt_t *stmt);
static void debug_log_expr(_ms_expr_t *expr);
static void debug_log_opcodes(struct opcode *opcodes, int num_opcodes);

//
// DEFINITIONS
//

static mscript_type_t *_ms_struct_decl_get_member(_ms_struct_decl_t *decl, const char *member, int *offset) {
    for (int i = 0; i < decl->num_members; i++) {
        if (strcmp(decl->members[i].name, member) == 0) {
            *offset = decl->members[i].offset;
            return decl->members[i].type;
        }
    }
    return NULL;
}

static void _mscript_type_init(mscript_type_t *type, const char *name, mscript_type_type_t type_type, 
        mscript_type_t *array_member_type, _ms_struct_decl_t *struct_decl, int size) {
    type->type = type_type;
    strncpy(type->name, name, MSCRIPT_MAX_SYMBOL_LEN + 2);
    type->name[MSCRIPT_MAX_SYMBOL_LEN + 2] = 0;
    type->array_member_type = array_member_type;
    type->struct_decl = struct_decl;
    type->size = size;
}

_ms_lvalue_t _ms_lvalue_invalid(void) {
    _ms_lvalue_t lvalue;
    lvalue.type = LVALUE_INVALID;
    return lvalue;
}

_ms_lvalue_t _ms_lvalue_local(int offset) {
    _ms_lvalue_t lvalue;
    lvalue.type = LVALUE_LOCAL;
    lvalue.offset = offset;
    return lvalue;
}

_ms_lvalue_t _ms_lvalue_array(void) {
    _ms_lvalue_t lvalue;
    lvalue.type = LVALUE_ARRAY;
    return lvalue;
}

static _ms_expr_t *_ms_expr_unary_op_new(_ms_mem_t *mem, _ms_token_t token, _ms_unary_op_type_t type, _ms_expr_t *operand) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_UNARY_OP;
    expr->token = token;
    expr->unary_op.type = type;
    expr->unary_op.operand = operand;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_binary_op_new(_ms_mem_t *mem, _ms_token_t token, _ms_binary_op_type_t type, _ms_expr_t *left, _ms_expr_t *right) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_BINARY_OP;
    expr->token = token;
    expr->binary_op.type = type;
    expr->binary_op.left = left;
    expr->binary_op.right = right;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_assignment_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *left, _ms_expr_t *right) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_ASSIGNMENT;
    expr->token = token;
    expr->assignment.left = left;
    expr->assignment.right = right;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_array_access_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *left, _ms_expr_t *right) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_ARRAY_ACCESS;
    expr->token = token;
    expr->array_access.left = left;
    expr->array_access.right = right;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_member_access_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *left, char *member_name) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_MEMBER_ACCESS;
    expr->token = token;
    expr->member_access.left = left;
    expr->member_access.member_name = member_name;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_call_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *function, _vec_expr_ptr_t args) {
    int num_args = args.length;

    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_CALL;
    expr->token = token;
    expr->call.function = function;
    expr->call.num_args = num_args;
    expr->call.args = _ms_mem_alloc(mem, num_args * sizeof(_ms_expr_t*));
    memcpy(expr->call.args, args.data, num_args * sizeof(_ms_expr_t*));

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_debug_print_new(_ms_mem_t *mem, _ms_token_t token, _vec_expr_ptr_t args) {
    int num_args = args.length;

    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_DEBUG_PRINT;
    expr->token = token;
    expr->debug_print.num_args = num_args;
    expr->debug_print.args = _ms_mem_alloc(mem, num_args * sizeof(_ms_expr_t*));
    memcpy(expr->debug_print.args, args.data, num_args * sizeof(_ms_expr_t*));

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_array_new(_ms_mem_t *mem, _ms_token_t token, _vec_expr_ptr_t args) {
    int num_args = args.length;

    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_ARRAY;
    expr->token = token;
    expr->array.num_args = num_args;
    expr->array.args = _ms_mem_alloc(mem, num_args * sizeof(_ms_expr_t*));
    memcpy(expr->array.args, args.data, num_args * sizeof(_ms_expr_t*));

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_object_new(_ms_mem_t *mem, _ms_token_t token, struct array_char_ptr names, _vec_expr_ptr_t args) {
    assert(names.length == args.length);
    int num_args = args.length;

    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_OBJECT;
    expr->token = token;
    expr->object.num_args = num_args;
    expr->object.names = _ms_mem_alloc(mem, num_args * sizeof(char *));
    memcpy(expr->object.names, names.data, num_args * sizeof(char *));
    expr->object.args = _ms_mem_alloc(mem, num_args * sizeof(_ms_expr_t *));
    memcpy(expr->object.args, args.data, num_args * sizeof(_ms_expr_t *));

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_cast_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t type, _ms_expr_t *arg) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_CAST;
    expr->token = token;
    expr->cast.type = type;
    expr->cast.arg = arg;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_int_new(_ms_mem_t *mem, _ms_token_t token, int int_val) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_INT;
    expr->token = token;
    expr->int_val = int_val;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_float_new(_ms_mem_t *mem, _ms_token_t token, float float_val) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_FLOAT;
    expr->token = token;
    expr->float_val = float_val;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_symbol_new(_ms_mem_t *mem, _ms_token_t token, char *symbol) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_SYMBOL;
    expr->token = token;
    expr->symbol = symbol;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *_ms_expr_null_new(_ms_mem_t *mem, _ms_token_t token) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_NULL;
    expr->token = token;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_expr_t *new_string_expr(_ms_mem_t *mem, _ms_token_t token, char *string) {
    _ms_expr_t *expr = _ms_mem_alloc(mem, sizeof(_ms_expr_t));
    expr->type = _MS_EXPR_STRING;
    expr->token = token;
    expr->string = string;

    expr->is_const = false;
    expr->result_type = NULL;
    expr->lvalue = _ms_lvalue_invalid();
    return expr;
}

static _ms_stmt_t *_ms_stmt_if_new(_ms_mem_t *mem, _ms_token_t token, _vec_expr_ptr_t conds, _vec_ms_stmt_ptr_t stmts, _ms_stmt_t *else_stmt) {
    assert(conds.length == stmts.length);
    int num_stmts = conds.length;

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_IF;
    stmt->token = token;
    stmt->if_stmt.num_stmts = num_stmts;
    stmt->if_stmt.conds = _ms_mem_alloc(mem, num_stmts * sizeof(_ms_expr_t *));
    memcpy(stmt->if_stmt.conds, conds.data, num_stmts * sizeof(_ms_expr_t *));
    stmt->if_stmt.stmts = _ms_mem_alloc(mem, num_stmts * sizeof(_ms_stmt_t *));
    memcpy(stmt->if_stmt.stmts, stmts.data, num_stmts * sizeof(_ms_stmt_t *));
    stmt->if_stmt.else_stmt = else_stmt;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_return_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *expr) {
    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_RETURN;
    stmt->token = token;
    stmt->return_stmt.expr = expr;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_block_new(_ms_mem_t *mem, _ms_token_t token, _vec_ms_stmt_ptr_t stmts) {
    int num_stmts = stmts.length;

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_BLOCK;
    stmt->token = token;
    stmt->block.num_stmts = num_stmts;
    stmt->block.stmts = _ms_mem_alloc(mem, num_stmts * sizeof(_ms_stmt_t *));
    memcpy(stmt->block.stmts, stmts.data, num_stmts * sizeof(_ms_stmt_t *));
    return stmt;
}

static _ms_stmt_t *_ms_stmt_function_declaration_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t return_type, char *name, 
        _vec_ms_parsed_type_t arg_types, struct array_char_ptr arg_names, _ms_stmt_t *body) {
    assert(arg_types.length == arg_names.length);
    int num_args = arg_types.length;

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_FUNCTION_DECLARATION;
    stmt->token = token;
    stmt->function_declaration.token = token;
    stmt->function_declaration.return_type = return_type;
    stmt->function_declaration.name = name;
    stmt->function_declaration.num_args = num_args;
    stmt->function_declaration.arg_types = _ms_mem_alloc(mem, num_args * sizeof(_ms_parsed_type_t));
    memcpy(stmt->function_declaration.arg_types, arg_types.data, num_args * sizeof(_ms_parsed_type_t));
    stmt->function_declaration.arg_names = _ms_mem_alloc(mem, num_args * sizeof(char *));
    memcpy(stmt->function_declaration.arg_names, arg_names.data, num_args * sizeof(char *));
    stmt->function_declaration.body = body;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_global_declaration_new(_ms_mem_t *mem, _ms_token_t token,
        _ms_parsed_type_t type, char *name, _ms_expr_t *init_expr) {
    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_GLOBAL_DECLARATION;
    stmt->token = token;
    stmt->global_declaration.type = type;
    stmt->global_declaration.name = name;
    stmt->global_declaration.init_expr = init_expr;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_variable_declaration_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t type, char *name, _ms_expr_t *assignment_expr) {
    if (assignment_expr) {
        assert(assignment_expr->type == _MS_EXPR_ASSIGNMENT);
    }

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_VARIABLE_DECLARATION;
    stmt->token = token;
    stmt->variable_declaration.type = type;
    stmt->variable_declaration.name = name;
    stmt->variable_declaration.assignment_expr = assignment_expr;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_struct_declaration_new(_ms_mem_t *mem, _ms_token_t token, char *name, 
        _vec_ms_parsed_type_t member_types, struct array_char_ptr member_names) {
    assert(member_types.length == member_names.length);
    int num_members = member_types.length;

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_STRUCT_DECLARATION;
    stmt->token = token;
    stmt->struct_declaration.name = name;
    stmt->struct_declaration.num_members = num_members;
    stmt->struct_declaration.member_types = _ms_mem_alloc(mem, num_members * sizeof(_ms_parsed_type_t));
    memcpy(stmt->struct_declaration.member_types, member_types.data, num_members * sizeof(_ms_parsed_type_t));
    stmt->struct_declaration.member_names = _ms_mem_alloc(mem, num_members * sizeof(char *));
    memcpy(stmt->struct_declaration.member_names, member_names.data, num_members * sizeof(char *));
    return stmt;
}

static _ms_stmt_t *_ms_stmt_enum_declaration_new(_ms_mem_t *mem, _ms_token_t token, char *name, struct array_char_ptr value_names) {
    int num_values = value_names.length;

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_ENUM_DECLARATION;
    stmt->token = token;
    stmt->enum_declaration.name = name;
    stmt->enum_declaration.num_values = num_values;
    stmt->enum_declaration.value_names = _ms_mem_alloc(mem, num_values * sizeof(char *));
    memcpy(stmt->enum_declaration.value_names, value_names.data, num_values * sizeof(char *));
    return stmt;
}

static _ms_stmt_t *_ms_stmt_for_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *init, _ms_expr_t *cond, _ms_expr_t *inc, _ms_stmt_t *body) {
    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_FOR;
    stmt->token = token;
    stmt->for_stmt.init = init;
    stmt->for_stmt.cond = cond;
    stmt->for_stmt.inc = inc;
    stmt->for_stmt.body = body;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_import_new(_ms_mem_t *mem, _ms_token_t token, char *program_name) {
    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_IMPORT;
    stmt->token = token;
    stmt->import.program_name = program_name;
    return stmt;
}

static _ms_stmt_t *_ms_stmt_import_function_new(_ms_mem_t *mem, _ms_token_t token, _ms_parsed_type_t return_type, char *name, _vec_ms_parsed_type_t arg_types, struct array_char_ptr arg_names) {
    int num_args = arg_types.length;
    assert(arg_types.length == arg_names.length);

    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_IMPORT_FUNCTION;
    stmt->token = token;
    stmt->import_function.return_type = return_type;
    stmt->import_function.name = name;
    stmt->import_function.num_args = num_args;
    stmt->import_function.arg_types = _ms_mem_alloc(mem, num_args * sizeof(_ms_parsed_type_t));
    memcpy(stmt->import_function.arg_types, arg_types.data, num_args * sizeof(_ms_parsed_type_t));
    stmt->import_function.arg_names = _ms_mem_alloc(mem, num_args * sizeof(char *));
    memcpy(stmt->import_function.arg_names, arg_names.data, num_args * sizeof(char *));
    return stmt;
}

static _ms_stmt_t *_ms_stmt_expr_new(_ms_mem_t *mem, _ms_token_t token, _ms_expr_t *expr) {
    _ms_stmt_t *stmt = _ms_mem_alloc(mem, sizeof(_ms_stmt_t));
    stmt->type = _MS_STMT_EXPR;
    stmt->token = token;
    stmt->expr = expr;
    return stmt;
}

static _ms_parsed_type_t _ms_parse_type(mscript_program_t *program) {
    const char *name = NULL;
    bool is_array = false;

    if (match_symbol(program, "void")) {
        if (match_char(program, '*')) {
            name = "void*";
        }
        else {
            name = "void";
        }
    }
    else if (match_symbol(program, "int")) {
        name = "int";
    }
    else if (match_symbol(program, "float")) {
        name = "float";
    }
    else if (match_symbol(program, "bool")) {
        name = "bool";
    }
    else {
        _ms_token_t tok = peek(program);
        if (tok.type != TOKEN_SYMBOL) {
            _ms_program_error(program, tok, "Expected symbol");
            return _ms_parsed_type("", false);
        }
        eat(program);

        name = tok.symbol;
    }

    if (match_char_n(program, 2, '[', ']')) {
        is_array = true;
    }

    return _ms_parsed_type(name, is_array);
}

static _ms_expr_t *_ms_parse_object_expr(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    struct array_char_ptr names;
    _vec_expr_ptr_t args;
    array_init(&names);
    vec_init(&args);

    _ms_expr_t *expr = NULL;

    if (!match_char(program, '}')) {
        while (true) {
            _ms_token_t tok = peek(program);
            if (tok.type != TOKEN_SYMBOL) {
                _ms_program_error(program, tok, "Expected symbol"); 
                goto cleanup;
            }
            array_push(&names, tok.symbol);
            eat(program);

            if (!match_char(program, '=')) {
                _ms_program_error(program, peek(program), "Expected '='");
                goto cleanup;
            }

            _ms_expr_t *arg = _ms_parse_expr(program);
            if (program->error) goto cleanup;
            vec_push(&args, arg);

            if (!match_char(program, ',')) {
                if (!match_char(program, '}')) {
                    _ms_program_error(program, peek(program), "Expected '}'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = _ms_expr_object_new(&program->parser.mem, token, names, args);

cleanup:
    array_deinit(&names);
    array_deinit(&args);
    return expr;
}

static _ms_expr_t *_ms_parse_array_expr(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _vec_expr_ptr_t args;
    vec_init(&args);

    _ms_expr_t *expr = NULL;
    if (!match_char(program, ']')) {
        while (true) {
            _ms_expr_t *arg = _ms_parse_expr(program);
            if (program->error) goto cleanup;
            vec_push(&args, arg);

            if (!match_char(program, ',')) {
                if (!match_char(program, ']')) {
                    _ms_program_error(program, peek(program), "Expected ']'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = _ms_expr_array_new(&program->parser.mem, token, args);

cleanup:
    array_deinit(&args);
    return expr;
}

static _ms_expr_t *_ms_parse_primary_expr(mscript_program_t *program) {
    _ms_token_t tok = peek(program);
    _ms_expr_t *expr = NULL;

    if (tok.type == TOKEN_INT) {
        expr = _ms_expr_int_new(&program->parser.mem, tok, tok.int_val);
        eat(program);
    }
    else if (tok.type == TOKEN_FLOAT) {
        expr = _ms_expr_float_new(&program->parser.mem, tok, tok.float_val);
        eat(program);
    }
    else if (match_symbol(program, "NULL")) {
        expr = _ms_expr_null_new(&program->parser.mem, tok);
    }
    else if (tok.type == TOKEN_SYMBOL) {
        expr = _ms_expr_symbol_new(&program->parser.mem, tok, tok.symbol);
        eat(program);
    }
    else if (tok.type == TOKEN_STRING) {
        expr = new_string_expr(&program->parser.mem, tok, tok.string);
        eat(program);
    }
    else if (match_char(program, '[')) {
        expr = _ms_parse_array_expr(program);
    }
    else if (match_char(program, '{')) {
        expr = _ms_parse_object_expr(program);
    }
    else if (match_char(program, '(')) {
        expr = _ms_parse_expr(program);
        if (!match_char(program, ')')) {
            _ms_program_error(program, peek(program), "Expected ')'."); 
            goto cleanup;
        }
    }
    else {
        _ms_program_error(program, tok, "Unknown token.");
        goto cleanup;
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_call_expr(mscript_program_t *program) {
    _vec_expr_ptr_t args;
    vec_init(&args);

    _ms_token_t token = peek(program);
    _ms_expr_t *expr = _ms_parse_primary_expr(program);
    if (program->error) goto cleanup;

    if (match_char(program, '(')) {
        if (!match_char(program, ')')) {
            while (true) {
                _ms_expr_t *arg = _ms_parse_expr(program);
                if (program->error) goto cleanup;
                vec_push(&args, arg);

                if (!match_char(program, ',')) {
                    if (!match_char(program, ')')) {
                        _ms_program_error(program, peek(program), "Expected ')'");
                        goto cleanup;
                    }
                    break;
                }
            }
        }

        if (expr->type == _MS_EXPR_SYMBOL && (strcmp(expr->symbol, "print") == 0)) {
            expr = _ms_expr_debug_print_new(&program->parser.mem, token, args);
        }
        else {
            expr = _ms_expr_call_new(&program->parser.mem, token, expr, args);
        }
    }

cleanup:
    array_deinit(&args);
    return expr;
}

static _ms_expr_t *_ms_parse_expr_member_access_or_array_access(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_expr_t *expr = _ms_parse_call_expr(program);
    if (program->error) goto cleanup;

    while (true) {
        if (match_char(program, '.')) {
            _ms_token_t tok = peek(program);
            if (tok.type != TOKEN_SYMBOL) {
                _ms_program_error(program, tok, "Expected symbol token");
                goto cleanup;
            }
            eat(program);

            expr = _ms_expr_member_access_new(&program->parser.mem, token, expr, tok.symbol);
        }
        else if (match_char(program, '[')) {
            _ms_expr_t *right = _ms_parse_expr(program);
            if (program->error) goto cleanup;
            expr = _ms_expr_array_access_new(&program->parser.mem, token, expr, right);

            if (!match_char(program, ']')) {
                _ms_program_error(program, peek(program), "Expected ']'");
                goto cleanup;
            }
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_expr_unary(mscript_program_t *program) {
    _ms_token_t token = peek(program);

    _ms_expr_t *expr = NULL;

    if (match_char(program, '!')) {
        expr = _ms_parse_expr_member_access_or_array_access(program); 
        if (program->error) goto cleanup;

        expr = _ms_expr_unary_op_new(&program->parser.mem, token, _MS_UNARY_OP_LOGICAL_NOT, expr);
    }
    else {
        expr = _ms_parse_expr_member_access_or_array_access(program);
        if (program->error) goto cleanup;

        if (match_char_n(program, 2, '+', '+')) {
            expr = _ms_expr_unary_op_new(&program->parser.mem, token, _MS_UNARY_OP_POST_INC, expr);
        }
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_expr_factor(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_expr_t *expr = _ms_parse_expr_unary(program);
    if (program->error) goto cleanup;

    while (true) {
        _ms_binary_op_type_t binary_op_type;

        _ms_token_t tok0 = peek_n(program, 0);
        _ms_token_t tok1 = peek_n(program, 1);
        if (is_char_token(tok0, '*') && !is_char_token(tok1, '=')) {
            binary_op_type = _MS_BINARY_OP_MUL;
            eat(program);
        }
        else if (is_char_token(tok0, '/') && !is_char_token(tok1, '=')) {
            binary_op_type = _MS_BINARY_OP_DIV;
            eat(program);
        }
        else {
            break;
        }

        _ms_expr_t *right = _ms_parse_expr_unary(program);
        if (program->error) goto cleanup;
        expr = _ms_expr_binary_op_new(&program->parser.mem, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_expr_term(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_expr_t *expr = _ms_parse_expr_factor(program);
    if (program->error) goto cleanup;

    while (true) {
        _ms_binary_op_type_t binary_op_type;

        _ms_token_t tok0 = peek_n(program, 0);
        _ms_token_t tok1 = peek_n(program, 1);
        if (is_char_token(tok0, '+') && !is_char_token(tok1, '=')) {
            binary_op_type = _MS_BINARY_OP_ADD;
            eat(program);
        }
        else if (is_char_token(tok0, '-') && !is_char_token(tok1, '=')) {
            binary_op_type = _MS_BINARY_OP_SUB;
            eat(program);
        }
        else {
            break;
        }

        _ms_expr_t *right = _ms_parse_expr_factor(program);
        if (program->error) goto cleanup;
        expr = _ms_expr_binary_op_new(&program->parser.mem, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_expr_comparison(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_expr_t *expr = _ms_parse_expr_term(program);
    if (program->error) goto cleanup;

    while (true) {
        _ms_binary_op_type_t binary_op_type;

        if (match_char_n(program, 2, '<', '=')) {
            binary_op_type = _MS_BINARY_OP_LTE;
        }
        else if (match_char_n(program, 1, '<')) {
            binary_op_type = _MS_BINARY_OP_LT;
        }
        else if (match_char_n(program, 2, '>', '=')) {
            binary_op_type = _MS_BINARY_OP_GTE;
        }
        else if (match_char_n(program, 1, '>')) {
            binary_op_type = _MS_BINARY_OP_GT;
        }
        else if (match_char_n(program, 2, '=', '=')) {
            binary_op_type = _MS_BINARY_OP_EQ;
        }
        else if (match_char_n(program, 2, '!', '=')) {
            binary_op_type = _MS_BINARY_OP_NEQ;
        }
        else {
            break;
        }

        _ms_expr_t *right = _ms_parse_expr_term(program);
        if (program->error) goto cleanup;
        expr = _ms_expr_binary_op_new(&program->parser.mem, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_expr_assignment(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_expr_t *expr = _ms_parse_expr_comparison(program);
    if (program->error) goto cleanup;

    while (true) {
        if (match_char_n(program, 2, '+', '=')) {
            _ms_expr_t *right = _ms_parse_expr_assignment(program);
            if (program->error) goto cleanup;

            _ms_expr_t *lvalue = expr;
            expr = _ms_expr_binary_op_new(&program->parser.mem, token, _MS_BINARY_OP_ADD, lvalue, right);
            expr = _ms_expr_assignment_new(&program->parser.mem, token, lvalue, expr);
        }
        else if (match_char_n(program, 2, '-', '=')) {
            _ms_expr_t *right = _ms_parse_expr_assignment(program);
            if (program->error) goto cleanup;

            _ms_expr_t *lvalue = expr;
            expr = _ms_expr_binary_op_new(&program->parser.mem, token, _MS_BINARY_OP_SUB, lvalue, right);
            expr = _ms_expr_assignment_new(&program->parser.mem, token, lvalue, expr);
        }
        else if (match_char_n(program, 2, '*', '=')) {
            _ms_expr_t *right = _ms_parse_expr_assignment(program);
            if (program->error) goto cleanup;

            _ms_expr_t *lvalue = expr;
            expr = _ms_expr_binary_op_new(&program->parser.mem, token, _MS_BINARY_OP_MUL, lvalue, right);
            expr = _ms_expr_assignment_new(&program->parser.mem, token, lvalue, expr);
        }
        else if (match_char_n(program, 2, '/', '=')) {
            _ms_expr_t *right = _ms_parse_expr_assignment(program);
            if (program->error) goto cleanup;

            _ms_expr_t *lvalue = expr;
            expr = _ms_expr_binary_op_new(&program->parser.mem, token, _MS_BINARY_OP_DIV, lvalue, right);
            expr = _ms_expr_assignment_new(&program->parser.mem, token, lvalue, expr);
        }
        else if (match_char(program, '=')) {
            _ms_expr_t *right = _ms_parse_expr_assignment(program);
            if (program->error) goto cleanup;

            expr = _ms_expr_assignment_new(&program->parser.mem, token, expr, right);
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static _ms_expr_t *_ms_parse_expr(mscript_program_t *program) {
    _ms_expr_t *expr = _ms_parse_expr_assignment(program);
    return expr;
}

static _ms_stmt_t *_ms_parse_stmt(mscript_program_t *program) {
    if (match_symbol(program, "if")) {
        return _ms_parse_if_stmt(program);
    }
    else if (match_symbol(program, "for")) {
        return _ms_parse_for_stmt(program);
    }
    else if (match_symbol(program, "return")) {
        return _ms_parse_return_stmt(program);
    }
    else if (check_type(program)) {
        return _ms_parse_variable_declaration_stmt(program);
    }
    else if (match_char(program, '{')) {
        return _ms_parse_block_stmt(program);
    }
    else {
        _ms_token_t token = peek(program);
        _ms_expr_t *expr = _ms_parse_expr(program);
        if (program->error) return NULL;

        if (!match_char(program, ';')) {
            _ms_program_error(program, peek(program), "Expected ';'");
            return NULL;
        }
        return _ms_stmt_expr_new(&program->parser.mem, token, expr);
    }
}

static _ms_stmt_t *_ms_parse_if_stmt(mscript_program_t *program) {
    _vec_expr_ptr_t conds;
    _vec_ms_stmt_ptr_t stmts;
    _ms_stmt_t *else_stmt = NULL;
    vec_init(&conds);
    vec_init(&stmts);

    _ms_stmt_t *stmt = NULL;
    _ms_token_t token = peek(program);

    if (!match_char(program, '(')) {
        _ms_program_error(program, peek(program), "Expected '('");
        goto cleanup;
    }

    {
        _ms_expr_t *cond = _ms_parse_expr(program);
        if (program->error) goto cleanup;

        if (!match_char(program, ')')) {
            _ms_program_error(program, peek(program), "Expected ')'");
            goto cleanup;
        }

        _ms_stmt_t *stmt = _ms_parse_stmt(program);
        if (program->error) goto cleanup;

        vec_push(&conds, cond);
        vec_push(&stmts, stmt);
    }

    while (true) {
        if (match_symbol_n(program, 2, "else", "if")) {
            if (!match_char(program, '(')) {
                _ms_program_error(program, peek(program), "Expected '('");
                goto cleanup;
            }

            _ms_expr_t *cond = _ms_parse_expr(program);
            if (program->error) goto cleanup;

            if (!match_char(program, ')')) {
                _ms_program_error(program, peek(program), "Expected ')'");
                goto cleanup;
            }

            _ms_stmt_t *stmt = _ms_parse_stmt(program);
            if (program->error) goto cleanup;

            vec_push(&conds, cond);
            vec_push(&stmts, stmt);
        }
        else if (match_symbol(program, "else")) {
            else_stmt = _ms_parse_stmt(program);
            if (program->error) goto cleanup;
            break;
        }
        else {
            break;
        }
    }

    stmt = _ms_stmt_if_new(&program->parser.mem, token, conds, stmts, else_stmt);

cleanup:
    vec_deinit(&conds);
    vec_deinit(&stmts);
    return stmt;
}

static _ms_stmt_t *_ms_parse_block_stmt(mscript_program_t *program) {
    _vec_ms_stmt_ptr_t stmts;
    vec_init(&stmts);

    _ms_stmt_t *stmt = NULL;
    _ms_token_t token = peek(program);
    
    while (true) {
        if (match_char(program, '}')) {
            break;
        }

        _ms_stmt_t *stmt = _ms_parse_stmt(program);
        if (program->error) goto cleanup;
        vec_push(&stmts, stmt);
    }

    stmt = _ms_stmt_block_new(&program->parser.mem, token, stmts);

cleanup:
    vec_deinit(&stmts);
    return stmt;
}

static _ms_stmt_t *_ms_parse_for_stmt(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_stmt_t *stmt = NULL;

    if (!match_char(program, '(')) {
        _ms_program_error(program, peek(program), "Expected '('");
        goto cleanup;
    }

    _ms_expr_t *init = _ms_parse_expr(program);
    if (program->error) goto cleanup;
    if (!match_char(program, ';')) {
        _ms_program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    _ms_expr_t *cond = _ms_parse_expr(program);
    if (program->error) goto cleanup;
    if (!match_char(program, ';')) {
        _ms_program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    _ms_expr_t *inc = _ms_parse_expr(program);
    if (program->error) goto cleanup;
    if (!match_char(program, ')')) {
        _ms_program_error(program, peek(program), "Expected ')'");
        goto cleanup;
    }

    _ms_stmt_t *body = _ms_parse_stmt(program);
    if (program->error) goto cleanup;

    stmt = _ms_stmt_for_new(&program->parser.mem, token, init, cond, inc, body);

cleanup:
    return stmt;
}

static _ms_stmt_t *_ms_parse_return_stmt(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_stmt_t *stmt = NULL;
    _ms_expr_t *expr = NULL;

    if (!match_char(program, ';')) {
        expr = _ms_parse_expr(program);
        if (program->error) goto cleanup;
        if (!match_char(program, ';')) {
            _ms_program_error(program, peek(program), "Expected ';'");
            goto cleanup;
        }
    }

    stmt = _ms_stmt_return_new(&program->parser.mem, token, expr);

cleanup:
    return stmt;
}

static _ms_stmt_t *_ms_parse_variable_declaration_stmt(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_stmt_t *stmt = NULL;

    _ms_parsed_type_t type = _ms_parse_type(program);
    if (program->error) goto cleanup;

    _ms_token_t name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        _ms_program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    _ms_expr_t *assignment_expr = NULL;
    if (match_char(program, '=')) {
        _ms_expr_t *left = _ms_expr_symbol_new(&program->parser.mem, token, name.symbol);
        _ms_expr_t *right = _ms_parse_expr(program);
        if (program->error) goto cleanup;

        assignment_expr = _ms_expr_assignment_new(&program->parser.mem, token, left, right);
    }

    if (!match_char(program, ';')) {
        _ms_program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    stmt = _ms_stmt_variable_declaration_new(&program->parser.mem, token, type, name.symbol, assignment_expr);

cleanup:
    return stmt;
}

static _ms_stmt_t *_ms_parse_function_declaration_stmt(mscript_program_t *program, _ms_parsed_type_t return_type) {
    _vec_ms_parsed_type_t arg_types;
    struct array_char_ptr arg_names;
    vec_init(&arg_types);
    array_init(&arg_names);

    _ms_stmt_t *stmt = NULL;

    _ms_token_t name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        _ms_program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '(')) {
        _ms_program_error(program, peek(program), "Expected '('");
        goto cleanup;
    }

    if (!match_char(program, ')')) {
        while (true) {
            _ms_parsed_type_t arg_type = _ms_parse_type(program);
            if (program->error) goto cleanup;

            _ms_token_t arg_name = peek(program);
            if (arg_name.type != TOKEN_SYMBOL) {
                _ms_program_error(program, arg_name, "Expected symbol");
                goto cleanup;
            }
            eat(program);

            vec_push(&arg_types, arg_type);
            array_push(&arg_names, arg_name.symbol);

            if (!match_char(program, ',')) {
                if (!match_char(program, ')')) {
                    _ms_program_error(program, peek(program), "Expected ')'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    if (!match_char(program, '{')) {
        _ms_program_error(program, peek(program), "Expected '{'");
        goto cleanup;
    }

    _ms_stmt_t *body_stmt = _ms_parse_block_stmt(program);
    if (program->error) goto cleanup;

    stmt = _ms_stmt_function_declaration_new(&program->parser.mem, name, return_type, name.symbol, arg_types, arg_names, body_stmt);

cleanup:
    array_deinit(&arg_types);
    array_deinit(&arg_names);
    return stmt;
}

static _ms_stmt_t *_ms_parse_global_declaration_stmt(mscript_program_t *program, _ms_parsed_type_t type) {
    _ms_stmt_t *stmt = NULL;

    _ms_token_t name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        _ms_program_error(program, name, "Expected symbol.");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '=')) {
        _ms_program_error(program, peek(program), "Expected '='.");
        goto cleanup;
    }

    _ms_expr_t *init_expr = _ms_parse_expr(program);

    if (!match_char(program, ';')) {
        _ms_program_error(program, peek(program), "Expected ';'.");
        goto cleanup;
    }

    stmt = _ms_stmt_global_declaration_new(&program->parser.mem, name, type, name.symbol, init_expr);

cleanup:
    return stmt;
}

static _ms_stmt_t *_ms_parse_struct_declaration_stmt(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _vec_ms_parsed_type_t member_types;
    struct array_char_ptr member_names;
    vec_init(&member_types);
    array_init(&member_names);

    _ms_stmt_t *stmt = NULL;

    _ms_token_t name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        _ms_program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '{')) {
        _ms_program_error(program, peek(program), "Expected '{'");
        goto cleanup;
    }

    while (true) {
        if (match_char(program, '}')) {
            break;
        }

        _ms_parsed_type_t member_type = _ms_parse_type(program);
        if (program->error) goto cleanup;

        while (true) {
            _ms_token_t member_name = peek(program);
            if (member_name.type != TOKEN_SYMBOL) {
                _ms_program_error(program, member_name, "Expected symbol");
                goto cleanup;
            }
            eat(program);

            vec_push(&member_types, member_type);
            array_push(&member_names, member_name.symbol);

            if (!match_char(program, ',')) {
                break;
            }
        }

        if (!match_char(program, ';')) {
            _ms_program_error(program, peek(program), "Expected ';'");
            goto cleanup;
        }
    }

    stmt = _ms_stmt_struct_declaration_new(&program->parser.mem, token, name.symbol, member_types, member_names);

cleanup:
    array_deinit(&member_types);
    array_deinit(&member_names);
    return stmt;
}

static _ms_stmt_t *_ms_parse_enum_declaration_stmt(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    struct array_char_ptr value_names;
    array_init(&value_names);

    _ms_stmt_t *stmt = NULL;

    _ms_token_t name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        _ms_program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '{')) {
        _ms_program_error(program, peek(program), "Expected '{'");
        goto cleanup;
    }

    if (!match_char(program, '}')) {
        while (true) {
            _ms_token_t value_name = peek(program);
            if (value_name.type != TOKEN_SYMBOL) {
                _ms_program_error(program, value_name, "Expected symbol.");
                goto cleanup;
            }
            eat(program);
            array_push(&value_names, value_name.symbol);

            if (!match_char(program, ',')) {
                if (!match_char(program, '}')) {
                    _ms_program_error(program, peek(program), "Expected '}'.");
                    goto cleanup;
                }
                break;
            }
        }
    }

    stmt = _ms_stmt_enum_declaration_new(&program->parser.mem, token, name.symbol, value_names);

cleanup:
    array_deinit(&value_names);
    return stmt;
}

static _ms_stmt_t *_ms_parse_import_stmt(mscript_program_t *program) {
    _ms_token_t token = peek(program);
    _ms_stmt_t *stmt = NULL;

    _ms_token_t program_name = peek(program);
    if (program_name.type != TOKEN_STRING) {
        _ms_program_error(program, program_name, "Expected string");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, ';')) {
        _ms_program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    stmt = _ms_stmt_import_new(&program->parser.mem, token, program_name.symbol);

cleanup:
    return stmt;
}

static _ms_stmt_t *_ms_parse_import_function_stmt(mscript_program_t *program) {
    _ms_stmt_t *stmt = NULL;

    _vec_ms_parsed_type_t arg_types;
    struct array_char_ptr arg_names;
    vec_init(&arg_types);
    array_init(&arg_names);

    _ms_parsed_type_t return_type = _ms_parse_type(program);
    if (program->error) goto cleanup;

    _ms_token_t name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        _ms_program_error(program, name, "Expected a symbol.");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '(')) {
        _ms_program_error(program, name, "Expected '('.");
        goto cleanup;
    }

    while (true) {
        _ms_parsed_type_t arg_type = _ms_parse_type(program);
        if (program->error) goto cleanup;

        _ms_token_t arg_name = peek(program);
        if (arg_name.type != TOKEN_SYMBOL) {
            _ms_program_error(program, name, "Expected a symbol.");
            goto cleanup;
        }
        eat(program);

        vec_push(&arg_types, arg_type);
        array_push(&arg_names, arg_name.symbol);

        if (!match_char(program, ',')) {
            if (!match_char(program, ')')) {
                _ms_program_error(program, arg_name, "Expected ')'.");
                goto cleanup;
            }
            break;
        }
    }

    if (!match_char(program, ';')) {
        _ms_program_error(program, name, "Expected ';'.");
        goto cleanup;
    }

    stmt = _ms_stmt_import_function_new(&program->parser.mem, name, return_type, name.symbol, arg_types, arg_names); 

cleanup:
    return stmt;
}

static _ms_const_val_t _ms_const_val_binary_op(_ms_binary_op_type_t type, _ms_const_val_t left, _ms_const_val_t right) {
    switch (type) {
        case _MS_BINARY_OP_ADD:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_int(left.int_val + right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_float(left.float_val + right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_SUB:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_int(left.int_val - right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_float(left.float_val - right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_MUL:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_int(left.int_val * right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_float(left.float_val * right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_DIV:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_int(left.int_val / right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_float(left.float_val / right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_LTE:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_bool(left.int_val <= right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_bool(left.float_val <= right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_LT:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_bool(left.int_val < right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_bool(left.float_val < right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_GTE:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_bool(left.int_val >= right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_bool(left.float_val >= right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_GT:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_bool(left.int_val > right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_bool(left.float_val > right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_EQ:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_bool(left.int_val == right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_bool(left.float_val == right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
        case _MS_BINARY_OP_NEQ:
            {
                if (left.type == _MS_CONST_VAL_INT && right.type == _MS_CONST_VAL_INT) {
                    return _ms_const_val_bool(left.int_val != right.int_val);
                }
                else if (left.type == _MS_CONST_VAL_FLOAT && right.type == _MS_CONST_VAL_FLOAT) {
                    return _ms_const_val_bool(left.float_val != right.float_val);
                }
                else {
                    assert(false);
                }
            }
            break;
    }

    assert(false);
    return _ms_const_val_int(0);
}

static _ms_const_val_t _ms_const_val_float(float float_val) {
    _ms_const_val_t val;
    val.type = _MS_CONST_VAL_FLOAT;
    val.float_val = float_val;
    return val;
}

static _ms_const_val_t _ms_const_val_int(int int_val) {
    _ms_const_val_t val;
    val.type = _MS_CONST_VAL_INT;
    val.int_val = int_val;
    return val;
}

static _ms_const_val_t _ms_const_val_bool(bool bool_val) {
    _ms_const_val_t val;
    val.type = _MS_CONST_VAL_BOOL;
    val.bool_val = bool_val;
    return val;
}

static _ms_const_val_t _ms_const_val_object(int num_args, _ms_const_val_t *args) {
    _ms_const_val_t val;
    val.type = _MS_CONST_VAL_OBJECT;
    val.object.num_args = num_args;
    val.object.args = args;
    return val;
}

static bool is_char_digit(char c) {
    return (c >= '0' && c <= '9');
}

static bool is_char_start_of_symbol(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c == '_');
}

static bool is_char_part_of_symbol(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '_');
}

static bool is_char(char c) {
    return (c == '(') ||
        (c == ')') ||
        (c == '{') ||
        (c == '}') ||
        (c == '<') ||
        (c == '>') ||
        (c == '=') ||
        (c == '+') ||
        (c == '-') ||
        (c == '*') ||
        (c == '/') ||
        (c == ',') ||
        (c == '!') ||
        (c == '[') ||
        (c == ']') ||
        (c == '.') ||
        (c == ';');
}

static _ms_token_t _ms_token_number(const char *text, int *len, int line, int col) {
    int int_part = 0;
    float float_part = 0.0f;

    *len = 0;
    bool is_negative = false;
    if (text[0] == '-') {
        is_negative = true;
        *len = 1;
    }

    bool found_decimal = false;
    float decimal_position = 10.0f;
    while (true) {
        if (is_char_digit(text[*len])) {
            if (found_decimal) {
                float_part += (text[*len] - '0') / decimal_position;
                decimal_position *= 10.0f;
            }
            else {
                int_part = 10 * int_part + (text[*len] - '0');
            }
        }
        else if (text[*len] == '.') {
            found_decimal = true;
        }
        else {
            break;
        }
        (*len)++;
    }

    if (found_decimal) {
        _ms_token_t token;
        token.type = TOKEN_FLOAT;
        token.float_val = (float)int_part + float_part;
        if (is_negative) token.float_val = -token.float_val;
        token.line = line;
        token.col = col;
        return token;
    }
    else {
        _ms_token_t token;
        token.type = TOKEN_INT;
        token.int_val = int_part;
        if (is_negative) token.int_val = -token.int_val;
        token.line = line;
        token.col = col;
        return token;
    }
}

static _ms_token_t _ms_token_char(char c, int line, int col) {
    _ms_token_t token;
    token.type = TOKEN_CHAR;
    token.char_val = c;
    token.line = line;
    token.col = col;
    return token;
}

static _ms_token_t _ms_token_string(mscript_program_t *program, const char *text, int *len, int line, int col) {
    *len = 0;
    while (text[*len] != '"') {
        (*len)++;
    }

    int actual_len = *len;
    int actual_i = 0;
    char *string = malloc((*len) + 1);
    for (int i = 0; i < *len; i++) {
        if ((i + 1 < *len) && (text[i] == '\\')) {
            if (text[i + 1] == 'n') {
                string[actual_i] = '\n';
            }
            else if (text[i + 1] == 't') {
                string[actual_i] = '\t';
            }
            else {
                _ms_token_t token;
                token.type = TOKEN_CHAR;
                token.char_val = text[i + 1];
                token.line = line;
                token.col = col;
                _ms_program_error(program, token, "Invalid escape character %c", token.char_val);
                free(string);
                return token;
            }

            i++;
            actual_len--;
        }
        else {
            string[actual_i] = text[i];
        }
        actual_i++;
    }
    string[actual_len] = 0;

    _ms_token_t token;
    token.type = TOKEN_STRING;
    token.string = string;
    token.line = line;
    token.col = col;
    return token;
}

static _ms_token_t _ms_token_symbol(const char *text, int *len, int line, int col) {
    *len = 0;
    while (is_char_part_of_symbol(text[*len])) {
        (*len)++;
    }

    char *symbol = malloc((*len) + 1);
    for (int i = 0; i < *len; i++) {
        symbol[i] = text[i];
    }
    symbol[*len] = 0;

    _ms_token_t token;
    token.type = TOKEN_SYMBOL;
    token.symbol = symbol;
    token.line = line;
    token.col = col;
    return token;
}

static _ms_token_t _ms_token_eof(int line, int col) {
    _ms_token_t token;
    token.type = TOKEN_EOF;
    token.line = line;
    token.col = col;
    return token;
}

static void _ms_tokenize(mscript_program_t *program) {
    struct parser *parser = &program->parser;
    const char *prog = parser->prog_text;
    int line = 1;
    int col = 1;
    int i = 0;

    while (true) {
        if (prog[i] == ' ' || prog[i] == '\t' || prog[i] == '\r') {
            col++;
            i++;
        }
        else if (prog[i] == '\n') {
            col = 1;
            line++;
            i++;
        }
        else if (prog[i] == 0) {
            vec_push(&parser->tokens, _ms_token_eof(line, col));
            break;
        }
        else if ((prog[i] == '/') && (prog[i + 1] == '/')) {
            while (prog[i] && (prog[i] != '\n')) {
                i++;
            }
        }
        else if (prog[i] == '"') {
            i++;
            col++;
            int len = 0;
            vec_push(&parser->tokens, _ms_token_string(program, prog + i, &len, line, col));
            if (program->error) return;
            i += (len + 1);
            col += (len + 1);
        }
        else if (is_char_start_of_symbol(prog[i])) {
            int len = 0;
            vec_push(&parser->tokens, _ms_token_symbol(prog + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (is_char_digit(prog[i])) {
            int len = 0;
            vec_push(&parser->tokens, _ms_token_number(prog + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (is_char(prog[i])) {
            vec_push(&parser->tokens, _ms_token_char(prog[i], line, col));
            i++;
            col++;
        }
        else {
            _ms_token_t tok = _ms_token_char(prog[i], line, col);
            _ms_program_error(program, tok, "Unknown character: %c", prog[i]);
            return;
        }
    }
}

static _ms_parsed_type_t _ms_parsed_type(const char *name, bool is_array) {
    _ms_parsed_type_t type;
    strncpy(type.string, name, MSCRIPT_MAX_SYMBOL_LEN);
    type.string[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    if (is_array) {
        strcat(type.string, "[]");
    }
    return type;
}

static void _ms_mem_init(_ms_mem_t *mem) {
    mem->bytes_allocated = 0;
    vec_init(&mem->ptrs);
}

static void _ms_mem_deinit(_ms_mem_t *mem) {
    vec_deinit(&mem->ptrs);
}

static void *_ms_mem_alloc(_ms_mem_t *mem, size_t size) {
    mem->bytes_allocated += size;
    void *ptr = malloc(size);
    vec_push(&mem->ptrs, ptr);
    return ptr;
}

static void parser_init(struct parser *parser, const char *prog_text) {
    parser->prog_text = prog_text;
    _ms_mem_init(&parser->mem);
    parser->token_idx = 0;
    vec_init(&parser->tokens);
    parser->error = NULL;
}

static void parser_deinit(struct parser *parser) {
}

static _ms_token_t peek(mscript_program_t *program) {
    struct parser *parser = &program->parser;

    if (parser->token_idx >= parser->tokens.length) {
        // Return EOF
        return parser->tokens.data[parser->tokens.length - 1];
    }
    else {
        return parser->tokens.data[parser->token_idx];
    }
}

static _ms_token_t peek_n(mscript_program_t *program, int n) {
    struct parser *parser = &program->parser;

    if (parser->token_idx + n >= parser->tokens.length) {
        // Return EOF
        return parser->tokens.data[parser->tokens.length - 1];
    }
    else {
        return parser->tokens.data[parser->token_idx + n];
    }
}

static void eat(mscript_program_t *program) {
    struct parser *parser = &program->parser;
    parser->token_idx++;
}

static bool is_char_token(_ms_token_t tok, char c) {
    if (tok.type == TOKEN_CHAR && tok.char_val == c) {
        return true;
    }
    else {
        return false;
    }
}

static bool match_char(mscript_program_t *program, char c) {
    _ms_token_t tok = peek(program);
    if (tok.type == TOKEN_CHAR && tok.char_val == c) {
        eat(program);
        return true;
    }
    else {
        return false;
    }
}

static bool match_char_n(mscript_program_t *program, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        char c = va_arg(ap, int);
        _ms_token_t tok = peek_n(program, i);
        if (tok.type != TOKEN_CHAR || tok.char_val != c) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        for (int i = 0; i < n; i++) {
            eat(program);
        }
    }

    return match;
}

static bool match_symbol(mscript_program_t *program, const char *symbol) {
    _ms_token_t tok = peek(program);
    if (tok.type == TOKEN_SYMBOL && (strcmp(symbol, tok.symbol) == 0)) {
        eat(program);
        return true;
    }
    else {
        return false;
    }
}

static bool match_symbol_n(mscript_program_t *program, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *symbol = va_arg(ap, const char *);
        _ms_token_t tok = peek_n(program, i);
        if (tok.type != TOKEN_SYMBOL || (strcmp(symbol, tok.symbol) != 0)) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        for (int i = 0; i < n; i++) {
            eat(program);
        }
    }

    return match;
}

static bool match_eof(mscript_program_t *program) {
    _ms_token_t tok = peek(program);
    return tok.type == TOKEN_EOF;
}

static bool check_type(mscript_program_t *program) {
    // Type's begin with 2 symbols or 1 symbol followed by [] for an array.
    // Or void*
    _ms_token_t tok0 = peek_n(program, 0);
    _ms_token_t tok1 = peek_n(program, 1);
    _ms_token_t tok2 = peek_n(program, 2);
    return ((tok0.type == TOKEN_SYMBOL) && (tok1.type == TOKEN_SYMBOL)) ||
            ((tok0.type == TOKEN_SYMBOL) &&
             (tok1.type == TOKEN_CHAR) &&
             (tok1.char_val == '[') &&
             (tok2.type == TOKEN_CHAR) &&
             (tok2.char_val == ']')) ||
            ((tok0.type == TOKEN_SYMBOL) && (strcmp(tok0.symbol, "void") == 0) &&
             (tok1.type == TOKEN_CHAR) && (tok1.char_val == '*'));
}

static _ms_const_decl_t _ms_const_decl(const char *name, mscript_type_t *type, _ms_const_val_t value) {
    _ms_const_decl_t decl;
    strncpy(decl.name, name, MSCRIPT_MAX_SYMBOL_LEN);
    decl.name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    decl.type = type;
    decl.value = value;
    return decl;
}

static void _ms_symbol_table_init(_ms_symbol_table_t *table) {
    table->globals_size = 0;
    vec_init(&table->blocks);
    map_init(&table->global_symbol_map);
}

static void _ms_symbol_table_push_block(_ms_symbol_table_t *table) {
    int l = table->blocks.length;

    _ms_symbol_block_t block;
    map_init(&block.symbol_map);
    if (l == 0) {
        block.size = 12;
    }
    else if (l == 1) {
        block.size = 0;
    }
    else {
        _ms_symbol_block_t *prev_block = table->blocks.data + l - 1;
        block.size = prev_block->size;
    }
    block.total_size = 0;
    vec_push(&table->blocks, block);
}

static void _ms_symbol_table_pop_block(_ms_symbol_table_t *table) {
    int l = table->blocks.length;
    assert(l > 0);

    if (l > 1) {
        _ms_symbol_block_t *block = table->blocks.data + l - 1;
        _ms_symbol_block_t *prev_block = table->blocks.data + l - 2;

        if (block->total_size < block->size) {
            block->total_size = block->size;
        }
        if (prev_block->total_size < block->total_size) {
            prev_block->total_size = block->total_size;
        }
    }

    vec_pop(&table->blocks);
}

static void _ms_symbol_table_add_local_var(_ms_symbol_table_t *table, const char *name, mscript_type_t *type) {
    int l = table->blocks.length;
    assert(l > 0);
    _ms_symbol_block_t *block = table->blocks.data + l - 1;

    int symbol_offset = 0;
    if (l == 1) {
        symbol_offset = -block->size - type->size;
    }
    else {
        symbol_offset = block->size;
    }

    _ms_symbol_t symbol;
    symbol.type = _MS_SYMBOL_LOCAL_VAR;
    symbol.local_var.type = type;
    symbol.local_var.offset = symbol_offset;
    map_set(&block->symbol_map, name, symbol);

    block->size += type->size;
}

static void _ms_symbol_table_add_global_decl(_ms_symbol_table_t *table, _ms_global_decl_t *decl) {
    _ms_symbol_t symbol;
    symbol.type = _MS_SYMBOL_GLOBAL_VAR;
    symbol.global_var.decl = decl;
    map_set(&table->global_symbol_map, decl->name, symbol);
}

static void _ms_symbol_table_add_const_decl(_ms_symbol_table_t *table, _ms_const_decl_t decl) {
    _ms_symbol_t symbol;
    symbol.type = _MS_SYMBOL_CONST;
    symbol.const_decl = decl;
    map_set(&table->global_symbol_map, decl.name, symbol);
}

static void _ms_symbol_table_add_function_decl(_ms_symbol_table_t *table, _ms_function_decl_t *decl) {
    _ms_symbol_t symbol;
    symbol.type = _MS_SYMBOL_FUNCTION;
    symbol.function_decl = decl;
    map_set(&table->global_symbol_map, decl->name, symbol);
}

static void _ms_symbol_table_add_type(_ms_symbol_table_t *table, mscript_type_t *type) {
    _ms_symbol_t symbol;
    symbol.type = _MS_SYMBOL_TYPE;
    symbol.type_val = type;
    map_set(&table->global_symbol_map, type->name, symbol);
}

static mscript_type_t *_ms_symbol_table_get_type(_ms_symbol_table_t *table, const char *name) {
    _ms_symbol_t *symbol = map_get(&table->global_symbol_map, name);
    if (symbol && symbol->type == _MS_SYMBOL_TYPE) {
        return symbol->type_val;
    }

    return NULL;
}

static _ms_function_decl_t *_ms_symbol_table_get_function_decl(_ms_symbol_table_t *table, const char *name) {
    _ms_symbol_t *symbol = map_get(&table->global_symbol_map, name);
    if (symbol && symbol->type == _MS_SYMBOL_FUNCTION) {
        return symbol->function_decl;
    }

    return NULL;
}

static _ms_symbol_t *_ms_symbol_table_get(_ms_symbol_table_t *table, const char *name) {
    int i = table->blocks.length;   
    while (i > 0) {
        _ms_symbol_block_t *block = table->blocks.data + i - 1;
        _ms_symbol_t *symbol = map_get(&block->symbol_map, name);
        if (symbol) {
            return symbol;
        }
        i--;
    }

    _ms_symbol_t *symbol = map_get(&table->global_symbol_map, name);
    if (symbol) {
        return symbol;
    }

    return NULL;
}

/*
static struct pre_compiler_env_var *pre_compiler_top_env_get_var(mscript_program_t *program, const char *symbol) {
    assert(program->pre_compiler.env_blocks.length > 0);
    int i = program->pre_compiler.env_blocks.length - 1;
    struct pre_compiler_env_block *block = &(program->pre_compiler.env_blocks.data[i]);
    struct pre_compiler_env_var *var = map_get(&block->map, symbol);
    if (var) {
        return var;
    }
    return NULL;
}
*/

static void _ms_verify_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    *all_paths_return = false;
    switch (stmt->type) {
        case _MS_STMT_IF:
            _ms_verify_if_stmt(program, stmt, all_paths_return);
            break;
        case _MS_STMT_FOR:
            _ms_verify_for_stmt(program, stmt, all_paths_return);
            break;
        case _MS_STMT_RETURN:
            _ms_verify_return_stmt(program, stmt, all_paths_return);
            break;
        case _MS_STMT_BLOCK:
            _ms_verify_block_stmt(program, stmt, all_paths_return);
            break;
        case _MS_STMT_EXPR:
            _ms_verify_expr_stmt(program, stmt, all_paths_return);
            break;
        case _MS_STMT_VARIABLE_DECLARATION:
            _ms_verify_variable_declaration_stmt(program, stmt, all_paths_return);
            break;
        case _MS_STMT_GLOBAL_DECLARATION:
        case _MS_STMT_FUNCTION_DECLARATION:
        case _MS_STMT_STRUCT_DECLARATION:
        case _MS_STMT_ENUM_DECLARATION:
        case _MS_STMT_IMPORT:
        case _MS_STMT_IMPORT_FUNCTION:
            // shouldn't do analysis on global statements
            assert(false);
            break;
    }
}

static void _ms_verify_if_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    assert(stmt->type == _MS_STMT_IF);

    *all_paths_return = true;
    for (int i = 0; i < stmt->if_stmt.num_stmts; i++) {
        _ms_verify_expr_with_cast(program, &(stmt->if_stmt.conds[i]), _ms_symbol_table_get_type(&program->symbol_table, "bool"));
        if (program->error) return;

        bool stmt_all_paths_return;
        _ms_verify_stmt(program, stmt->if_stmt.stmts[i], &stmt_all_paths_return);
        if (program->error) return;

        if (!stmt_all_paths_return) {
            *all_paths_return = false;
        }
    }

    if (stmt->if_stmt.else_stmt) {
        bool stmt_all_paths_return;
        _ms_verify_stmt(program, stmt->if_stmt.else_stmt, &stmt_all_paths_return);
        if (program->error) return;

        if (!stmt_all_paths_return) {
            *all_paths_return = false;
        }
    }
    else {
        *all_paths_return = false;
    }
}

static void _ms_verify_for_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    assert(stmt->type == _MS_STMT_FOR);

    _ms_verify_expr(program, stmt->for_stmt.init, NULL);
    if (program->error) return;
    _ms_verify_expr_with_cast(program, &(stmt->for_stmt.cond), _ms_symbol_table_get_type(&program->symbol_table, "bool"));
    if (program->error) return;
    _ms_verify_expr(program, stmt->for_stmt.inc, NULL);
    if (program->error) return;
    _ms_verify_stmt(program, stmt->for_stmt.body, all_paths_return);
    if (program->error) return;
}

static void _ms_verify_return_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    assert(stmt->type == _MS_STMT_RETURN);
    *all_paths_return = true;

    mscript_type_t *return_type = program->verify_function_decl->return_type;
    assert(return_type);

    if (return_type->type == MSCRIPT_TYPE_VOID) {
        if (stmt->return_stmt.expr) {
            _ms_program_error(program, stmt->token, "Cannot return expression for void function.");
            return;
        }
     }
    else {
        if (!stmt->return_stmt.expr) {
            _ms_program_error(program, stmt->token, "Must return expression for non-void function.");
            return;
        }
        else {
            _ms_verify_expr_with_cast(program, &(stmt->return_stmt.expr), return_type);
        }
    }
}

static void _ms_verify_block_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    assert(stmt->type == _MS_STMT_BLOCK);

    _ms_symbol_table_push_block(&program->symbol_table);
    for (int i = 0; i < stmt->block.num_stmts; i++) {
        bool stmt_all_paths_return;
        _ms_verify_stmt(program, stmt->block.stmts[i], &stmt_all_paths_return);
        if (program->error) goto cleanup;
        if (stmt_all_paths_return) {
            *all_paths_return = true;
        }

        if (stmt_all_paths_return && (i < stmt->block.num_stmts - 1)) {
            _ms_program_error(program, stmt->block.stmts[i + 1]->token, "Unreachable statement.");
            goto cleanup;

        }
    }

cleanup:
    _ms_symbol_table_pop_block(&program->symbol_table);
}

static void _ms_verify_expr_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    assert(stmt->type == _MS_STMT_EXPR);
    _ms_verify_expr(program, stmt->expr, NULL);
}

static void _ms_verify_variable_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt, bool *all_paths_return) {
    assert(stmt->type == _MS_STMT_VARIABLE_DECLARATION);

    char *name = stmt->variable_declaration.name;
    mscript_type_t *type = _ms_symbol_table_get_type(&program->symbol_table, stmt->variable_declaration.type.string);
    if (!type) {
        _ms_program_error(program, stmt->token, "Undefined type %s.", stmt->variable_declaration.type.string);
        return;
    }

    if (_ms_symbol_table_get(&program->symbol_table, name)) {
        _ms_program_error(program, stmt->token, "Symbol already declared.");
        return;
    }

    _ms_symbol_table_add_local_var(&program->symbol_table, name, type);
    if (stmt->variable_declaration.assignment_expr) {
        _ms_verify_expr(program, stmt->variable_declaration.assignment_expr, NULL);
        if (program->error) return;
    }
}

static void _ms_verify_import_function_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_IMPORT_FUNCTION);

    _ms_function_decl_t *decl = _ms_symbol_table_get_function_decl(&program->symbol_table, stmt->import_function.name);
    assert(decl);

    decl->return_type = _ms_symbol_table_get_type(&program->symbol_table, stmt->import_function.return_type.string);
    if (!decl->return_type) {
        _ms_program_error(program, stmt->token, "Undefined type %s.", stmt->import_function.return_type.string);
        goto cleanup;
    }

    strncpy(decl->name, stmt->import_function.name, MSCRIPT_MAX_SYMBOL_LEN);
    decl->name[MSCRIPT_MAX_SYMBOL_LEN] = 0;

    decl->num_args = stmt->import_function.num_args;
    for (int i = 0; i < decl->num_args; i++) {
        decl->args[i].type = _ms_symbol_table_get_type(&program->symbol_table, stmt->import_function.arg_types[i].string);
        if (!decl->args[i].type) {
            _ms_program_error(program, stmt->token, "Undefined type %s.", stmt->import_function.arg_types[i].string);
            goto cleanup;
        }

        strncpy(decl->args[i].name, stmt->import_function.arg_names[i], MSCRIPT_MAX_SYMBOL_LEN);
        decl->args[i].name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    }

cleanup:
    return;
}

static void _ms_verify_function_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_FUNCTION_DECLARATION);

    _ms_symbol_table_push_block(&program->symbol_table);
    _ms_function_decl_t *decl = _ms_symbol_table_get_function_decl(&program->symbol_table, stmt->function_declaration.name);
    assert(decl);

    for (int i = 0; i < decl->num_args; i++) {
        mscript_type_t *arg_type = decl->args[i].type;
        assert(arg_type);

        const char *arg_name = decl->args[i].name;
        _ms_symbol_table_add_local_var(&program->symbol_table, arg_name, arg_type);
    }

    program->verify_function_decl = decl;
    bool all_paths_return = false;
    _ms_verify_stmt(program, stmt->function_declaration.body, &all_paths_return);
    if (program->error) goto cleanup;

    if (!all_paths_return && (decl->return_type->type != MSCRIPT_TYPE_VOID)) {
        _ms_program_error(program, stmt->function_declaration.token, "Not all paths return from function.");
        goto cleanup;
    }

    _ms_symbol_block_t *first_block = program->symbol_table.blocks.data;
    decl->block_size = first_block->total_size;

cleanup:
    _ms_symbol_table_pop_block(&program->symbol_table);
}

static void _ms_verify_expr_with_cast(mscript_program_t *program, _ms_expr_t **expr, mscript_type_t *type) {
    _ms_verify_expr(program, *expr, type);
    if (program->error) return;

    if ((*expr)->result_type == type) {
        return;
    }

    mscript_type_type_t result_type_type = (*expr)->result_type->type;
    if (type->type == MSCRIPT_TYPE_INT) {
        if (result_type_type == MSCRIPT_TYPE_FLOAT) {
            _ms_parsed_type_t parsed_type = _ms_parsed_type("int", false);
            *expr = _ms_expr_cast_new(&program->parser.mem, (*expr)->token, parsed_type, *expr);
            (*expr)->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);

            bool is_const = (*expr)->cast.arg->is_const;
            (*expr)->is_const = is_const;
            if (is_const) {
                (*expr)->const_val = _ms_const_val_int((int) (*expr)->cast.arg->const_val.float_val);
            }
        }
        else {
            _ms_program_error(program, (*expr)->token, "Unable cast from %s to %s.", (*expr)->result_type->name, type->name);
            return;
        }
    }
    else if (type->type == MSCRIPT_TYPE_FLOAT) {
        if (result_type_type == MSCRIPT_TYPE_INT) {
            _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
            *expr = _ms_expr_cast_new(&program->parser.mem, (*expr)->token, parsed_type, *expr);
            (*expr)->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);

            bool is_const = (*expr)->cast.arg->is_const;
            (*expr)->is_const = is_const;
            if (is_const) {
                (*expr)->const_val = _ms_const_val_float((float) (*expr)->cast.arg->const_val.int_val);
            }
        }
        else {
            _ms_program_error(program, (*expr)->token, "Unable cast from %s to %s.", (*expr)->result_type->name, type->name);
            return;
        }
    }
    else if (type->type == MSCRIPT_TYPE_BOOL) {
        if (result_type_type == MSCRIPT_TYPE_ARRAY) {
            _ms_parsed_type_t parsed_type = _ms_parsed_type("bool", false);
            *expr = _ms_expr_cast_new(&program->parser.mem, (*expr)->token, parsed_type, *expr);
            (*expr)->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
            (*expr)->is_const = false;
        }
        else {
            _ms_program_error(program, (*expr)->token, "Unable to cast from %s to %s.", (*expr)->result_type->name, type->name);
        }
    }
    else {
        _ms_program_error(program, (*expr)->token, "Unable cast from %s to %s.", (*expr)->result_type->name, type->name);
        return;
    }
}

static void _ms_verify_expr_lvalue(mscript_program_t *program, _ms_expr_t *expr) {
    switch (expr->type) {
        case _MS_EXPR_UNARY_OP:
        case _MS_EXPR_BINARY_OP:
        case _MS_EXPR_CALL:
        case _MS_EXPR_DEBUG_PRINT:
        case _MS_EXPR_ASSIGNMENT:
        case _MS_EXPR_INT:
        case _MS_EXPR_FLOAT:
        case _MS_EXPR_ARRAY:
        case _MS_EXPR_OBJECT:
        case _MS_EXPR_CAST:
        case _MS_EXPR_NULL:
        case _MS_EXPR_STRING:
            {
                _ms_program_error(program, expr->token, "Invalid lvalue.");
            }
            break;
        case _MS_EXPR_ARRAY_ACCESS:
            {
                _ms_verify_expr(program, expr->array_access.left, NULL);
                if (program->error) return;

                mscript_type_t *left_type = expr->array_access.left->result_type;
                if (left_type->type != MSCRIPT_TYPE_ARRAY) {
                    _ms_program_error(program, expr->array_access.left->token, "Cannot perform array access on type %s.", left_type->name);
                    return;
                }

                _ms_verify_expr_with_cast(program, &(expr->array_access.right), _ms_symbol_table_get_type(&program->symbol_table, "int"));

                expr->result_type = left_type->array_member_type;
                expr->lvalue = _ms_lvalue_array();
                expr->is_const = false;
            }
            break;
        case _MS_EXPR_MEMBER_ACCESS:
            {
                _ms_verify_expr(program, expr->member_access.left, NULL);
                if (program->error) return;

                mscript_type_t *left_type = expr->member_access.left->result_type;
                if (left_type->type != MSCRIPT_TYPE_STRUCT) {
                    _ms_program_error(program, expr->member_access.left->token, "Cannot perform member access on type %s.", left_type->name);
                    return;
                }

                int member_offset;
                mscript_type_t *member_type = _ms_struct_decl_get_member(left_type->struct_decl, expr->member_access.member_name, &member_offset);
                if (!member_type) {
                    _ms_program_error(program, expr->token, "Invalid member %s on type %s.", expr->member_access.member_name, left_type->name);
                    return;
                }

                _ms_lvalue_t lvalue;
                _ms_lvalue_t left_lvalue = expr->member_access.left->lvalue;
                switch (left_lvalue.type) {
                    case LVALUE_INVALID:
                        assert(false);
                        break;
                    case LVALUE_LOCAL:
                        lvalue = _ms_lvalue_local(left_lvalue.offset + member_offset);
                        break;
                    case LVALUE_ARRAY:
                        lvalue = _ms_lvalue_array();
                        break;
                }

                expr->result_type = member_type;
                expr->lvalue = lvalue;
                expr->is_const = false;
            }
            break;
        case _MS_EXPR_SYMBOL:
            {
                _ms_verify_expr(program, expr, NULL);
            }
            break;
    }
}

static void _ms_verify_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    switch (expr->type) {
        case _MS_EXPR_UNARY_OP:
            _ms_verify_unary_op_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_BINARY_OP:
            _ms_verify_binary_op_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_CALL:
            _ms_verify_call_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_DEBUG_PRINT:
            _ms_verify_debug_print_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_MEMBER_ACCESS:
            _ms_verify_member_access_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_ASSIGNMENT:
            _ms_verify_assignment_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_INT:
            _ms_verify_int_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_FLOAT:
            _ms_verify_float_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_SYMBOL:
            _ms_verify_symbol_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_NULL:
            _ms_verify_null_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_STRING:
            _ms_verify_string_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_ARRAY:
            _ms_verify_array_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_ARRAY_ACCESS:
            _ms_verify_array_access_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_OBJECT:
            _ms_verify_object_expr(program, expr, expected_type);
            break;
        case _MS_EXPR_CAST:
            _ms_verify_cast_expr(program, expr, expected_type);
            break;
    }
}

static void _ms_verify_unary_op_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_UNARY_OP);

    switch (expr->unary_op.type) {
        case _MS_UNARY_OP_POST_INC:
            {
                _ms_verify_expr_lvalue(program, expr->unary_op.operand);
                if (program->error) return;

                expr->is_const = false;

                mscript_type_t *operand_type = expr->unary_op.operand->result_type;
                if (operand_type->type == MSCRIPT_TYPE_INT) {
                    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "int");
                }
                else if (operand_type->type == MSCRIPT_TYPE_FLOAT) {
                    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "float");
                }
                else {
                    _ms_program_error(program, expr->token, "Unable to do increment on type %s.", operand_type->name);
                    return;
                }
            }
            break;
        case _MS_UNARY_OP_LOGICAL_NOT:
            {
                _ms_verify_expr_with_cast(program, &(expr->unary_op.operand), _ms_symbol_table_get_type(&program->symbol_table, "bool"));
                if (program->error) return;

                expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "bool");
                expr->is_const = expr->unary_op.operand->is_const;
                if (expr->is_const) {
                    assert(expr->unary_op.operand->const_val.type == _MS_CONST_VAL_BOOL);
                    expr->const_val = _ms_const_val_bool(!expr->unary_op.operand->const_val.bool_val);
                }
            }
            break;
    }

    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_binary_op_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_BINARY_OP);

    _ms_verify_expr(program, expr->binary_op.left, expected_type);
    if (program->error) return;
    _ms_verify_expr(program, expr->binary_op.right, expected_type);
    if (program->error) return;

    mscript_type_t *left_result_type = expr->binary_op.left->result_type;
    mscript_type_t *right_result_type = expr->binary_op.right->result_type;

    switch (expr->binary_op.type) {
        case _MS_BINARY_OP_ADD:
        case _MS_BINARY_OP_SUB:
        case _MS_BINARY_OP_MUL:
        case _MS_BINARY_OP_DIV:
            {
                if (left_result_type->type == MSCRIPT_TYPE_INT && right_result_type->type == MSCRIPT_TYPE_INT) {
                    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "int");
                }
                else if (left_result_type->type == MSCRIPT_TYPE_FLOAT && right_result_type->type == MSCRIPT_TYPE_FLOAT) {
                    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "float");
                }
                else if (left_result_type->type == MSCRIPT_TYPE_FLOAT && right_result_type->type == MSCRIPT_TYPE_INT) {
                    _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
                    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "float");
                    expr->binary_op.right = _ms_expr_cast_new(&program->parser.mem, expr->token, parsed_type, expr->binary_op.right);
                    expr->binary_op.right->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
                }
                else if (left_result_type->type == MSCRIPT_TYPE_INT && right_result_type->type == MSCRIPT_TYPE_FLOAT) {
                    _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
                    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "float");
                    expr->binary_op.left = _ms_expr_cast_new(&program->parser.mem, expr->token, parsed_type, expr->binary_op.left);
                    expr->binary_op.left->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
                }
                else {
                    _ms_program_error(program, expr->token, "Unable to do this binary operation on types %s and %s.", 
                            left_result_type->name, right_result_type->name);
                    return;
                }
            }
            break;
        case _MS_BINARY_OP_LTE:
        case _MS_BINARY_OP_LT:
        case _MS_BINARY_OP_GTE:
        case _MS_BINARY_OP_GT:
            {
                expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "bool");

                if (left_result_type->type == MSCRIPT_TYPE_INT && right_result_type->type == MSCRIPT_TYPE_INT) {
                    // no casts needed
                }
                else if (left_result_type->type == MSCRIPT_TYPE_FLOAT && right_result_type->type == MSCRIPT_TYPE_FLOAT) {
                    // no casts needed
                }
                else if (left_result_type->type == MSCRIPT_TYPE_FLOAT && right_result_type->type == MSCRIPT_TYPE_INT) {
                    _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
                    expr->binary_op.right = _ms_expr_cast_new(&program->parser.mem, expr->token, parsed_type, expr->binary_op.right);
                    expr->binary_op.right->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
                }
                else if (left_result_type->type == MSCRIPT_TYPE_INT && right_result_type->type == MSCRIPT_TYPE_FLOAT) {
                    _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
                    expr->binary_op.left = _ms_expr_cast_new(&program->parser.mem, expr->token, parsed_type, expr->binary_op.left);
                    expr->binary_op.left->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
                }
                else if (left_result_type->type == MSCRIPT_TYPE_ENUM && right_result_type->type == MSCRIPT_TYPE_ENUM && 
                        left_result_type == right_result_type) {
                    // no casts needed
                }
                else {
                    _ms_program_error(program, expr->token, "Unable to do this binary operation on types %s and %s.",
                            left_result_type->name, right_result_type->name);
                    return;
                }
            }
            break;
        case _MS_BINARY_OP_EQ:
        case _MS_BINARY_OP_NEQ:
            {
                expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "bool");

                if (left_result_type->type == MSCRIPT_TYPE_INT && right_result_type->type == MSCRIPT_TYPE_INT) {
                    // no casts needed
                }
                else if (left_result_type->type == MSCRIPT_TYPE_FLOAT && right_result_type->type == MSCRIPT_TYPE_FLOAT) {
                    // no casts needed
                }
                else if (left_result_type->type == MSCRIPT_TYPE_FLOAT && right_result_type->type == MSCRIPT_TYPE_INT) {
                    _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
                    expr->binary_op.right = _ms_expr_cast_new(&program->parser.mem, expr->token, parsed_type, expr->binary_op.right);
                    expr->binary_op.right->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
                }
                else if (left_result_type->type == MSCRIPT_TYPE_INT && right_result_type->type == MSCRIPT_TYPE_FLOAT) {
                    _ms_parsed_type_t parsed_type = _ms_parsed_type("float", false);
                    expr->binary_op.left = _ms_expr_cast_new(&program->parser.mem, expr->token, parsed_type, expr->binary_op.left);
                    expr->binary_op.left->result_type = _ms_symbol_table_get_type(&program->symbol_table, parsed_type.string);
                }
                else if (left_result_type->type == MSCRIPT_TYPE_ENUM && right_result_type->type == MSCRIPT_TYPE_ENUM && 
                        left_result_type == right_result_type) {
                    // no casts needed
                }
                else if (left_result_type->type == MSCRIPT_TYPE_BOOL && right_result_type->type == MSCRIPT_TYPE_BOOL) {
                    // no casts needed
                }
                else {
                    _ms_program_error(program, expr->token, "Unable to do this binary operation on types %s and %s.",
                            left_result_type->name, right_result_type->name);
                    return;
                }
            }
            break;
    }

    expr->is_const = expr->binary_op.left->is_const && expr->binary_op.right->is_const;
    if (expr->is_const) {
        expr->const_val = _ms_const_val_binary_op(expr->binary_op.type, expr->binary_op.left->const_val, expr->binary_op.right->const_val);
    }
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_call_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_CALL);

    _ms_expr_t *fn = expr->call.function;
    if (fn->type != _MS_EXPR_SYMBOL) {
        _ms_program_error(program, fn->token, "Expect symbol in function position.");
        return;
    }

    _ms_function_decl_t *decl = _ms_symbol_table_get_function_decl(&program->symbol_table, fn->symbol);
    if (!decl) {
        _ms_program_error(program, fn->token, "Undefined function %s.", fn->symbol);
        return;
    }

    if (decl->num_args != expr->call.num_args) {
        _ms_program_error(program, expr->token, "Invalid number of arguments to function %s. Expected %d but got %d.", fn->symbol, decl->num_args, expr->call.num_args);
        return;
    }

    for (int i = 0; i < expr->call.num_args; i++) {
        _ms_verify_expr_with_cast(program, &(expr->call.args[i]), decl->args[i].type);
        if (program->error) return;
    }

    expr->is_const = false;
    expr->result_type = decl->return_type;
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_debug_print_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_DEBUG_PRINT);

    for (int i = 0; i < expr->debug_print.num_args; i++) {
        _ms_verify_expr(program, expr->debug_print.args[i], NULL);
        if (program->error) return;
    }

    expr->is_const = false;
    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "void");
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_member_access_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_MEMBER_ACCESS);

    _ms_verify_expr(program, expr->member_access.left, NULL);
    if (program->error) return;

    mscript_type_t *left_type = expr->member_access.left->result_type;
    if (left_type->type == MSCRIPT_TYPE_STRUCT) {
        _ms_struct_decl_t *decl = left_type->struct_decl;

        int member_offset;
        mscript_type_t *member_type = _ms_struct_decl_get_member(decl, expr->member_access.member_name, &member_offset);
        if (!member_type) {
            _ms_program_error(program, expr->token, "Invalid member %s on struct.", expr->member_access.member_name);
            return;
        }

        _ms_lvalue_t lvalue;
        _ms_lvalue_t left_lvalue = expr->member_access.left->lvalue;
        switch (left_lvalue.type) {
            case LVALUE_INVALID:
                assert(false);
                break;
            case LVALUE_LOCAL:
                lvalue = _ms_lvalue_local(left_lvalue.offset + member_offset);
                break;
            case LVALUE_ARRAY:
                lvalue = _ms_lvalue_array();
                break;
        }

        expr->result_type = member_type;
        expr->lvalue = lvalue;
    }
    else if (left_type->type == MSCRIPT_TYPE_ARRAY) {
        if (strcmp(expr->member_access.member_name, "length") == 0) {
            expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "int");
        }
        else {
            _ms_program_error(program, expr->token, "Invalid member %s on array.", expr->member_access.member_name);
            return;
        }

        expr->lvalue = _ms_lvalue_array();
    }
    else {
        _ms_program_error(program, expr->token, "Invalid type for member access.");
        return;
    }

    expr->is_const = false;
}

static void _ms_verify_assignment_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_ASSIGNMENT);

    _ms_verify_expr_lvalue(program, expr->assignment.left);
    if (program->error) return;

    mscript_type_t *left_type = expr->assignment.left->result_type;

    _ms_verify_expr_with_cast(program, &(expr->assignment.right), left_type);
    if (program->error) return;

    expr->is_const = false;
    expr->result_type = left_type;
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_int_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_INT);
    expr->is_const = true;
    expr->const_val = _ms_const_val_int(expr->int_val);
    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "int");
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_float_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_FLOAT);
    expr->is_const = true;
    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "float");
    expr->const_val = _ms_const_val_float(expr->float_val);
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_symbol_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_SYMBOL);

    _ms_symbol_t *symbol = _ms_symbol_table_get(&program->symbol_table, expr->symbol);
    if (!symbol) {
        _ms_program_error(program, expr->token, "Unable to find declaration of symbol %s.\n", expr->symbol);
        goto cleanup;
    }

    switch (symbol->type) {
        case _MS_SYMBOL_LOCAL_VAR:
            {
                expr->is_const = false;
                expr->result_type = symbol->local_var.type;
                expr->lvalue = _ms_lvalue_local(symbol->local_var.offset);
            }
            break;
        case _MS_SYMBOL_GLOBAL_VAR:
            {
                assert(false);
            }
            break;
        case _MS_SYMBOL_CONST:
            {
                expr->is_const = true;
                expr->const_val = symbol->const_decl.value;
                expr->result_type = symbol->const_decl.type;
                expr->lvalue = _ms_lvalue_invalid();
            }
            break;
        case _MS_SYMBOL_FUNCTION:
            {
                assert(false);
            }
            break;
        case _MS_SYMBOL_TYPE:
            {
                assert(false);
            }
            break;
    }

cleanup:
    return;
}

static void _ms_verify_null_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_NULL);

    if (!expected_type) {
        _ms_program_error(program, expr->token, "Cannot determine type of NULL.");
        return;
    }

    if (expected_type->type != MSCRIPT_TYPE_ARRAY) {
        _ms_program_error(program, expr->token, "Can only use NULL as an array.");
        return;
    }

    expr->is_const = true;
    expr->const_val = _ms_const_val_int(0);
    expr->result_type = expected_type; 
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_string_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_STRING);
    expr->is_const = false;
    expr->result_type = _ms_symbol_table_get_type(&program->symbol_table, "char*");
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_array_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_ARRAY);

    if (!expected_type) {
        _ms_program_error(program, expr->token, "Cannot determine type of array.");
        return;
    }

    if (expected_type->type != MSCRIPT_TYPE_ARRAY) {
        _ms_program_error(program, expr->token, "Not expecting array.");
        return;
    }

    mscript_type_t *arg_type = expected_type->array_member_type;
    for (int i = 0; i < expr->array.num_args; i++) {
        _ms_verify_expr_with_cast(program, &(expr->array.args[i]), arg_type);
        if (program->error) return;
    }

    expr->is_const = false;
    expr->result_type = expected_type;
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_array_access_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_ARRAY_ACCESS);

    _ms_verify_expr(program, expr->array_access.left, expected_type);
    if (program->error) return;

    mscript_type_t *left_type = expr->array_access.left->result_type;
    if (left_type->type != MSCRIPT_TYPE_ARRAY) {
        _ms_program_error(program, expr->array_access.left->token, "Expected array.");
        return;
    }

    _ms_verify_expr_with_cast(program, &(expr->array_access.right), _ms_symbol_table_get_type(&program->symbol_table, "int"));
    if (program->error) return;

    expr->is_const = false;
    expr->result_type = left_type->array_member_type;
    expr->lvalue = _ms_lvalue_array();
}

static void _ms_verify_object_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(expr->type == _MS_EXPR_OBJECT);
    
    if (!expected_type) {
        _ms_program_error(program, expr->token, "Cannot determine type of struct.");
        return;
    }

    if (expected_type->type != MSCRIPT_TYPE_STRUCT) {
        _ms_program_error(program, expr->token, "Not expecting struct.");
        return;
    }

    _ms_struct_decl_t *decl = expected_type->struct_decl;
    assert(decl);

    if (expr->object.num_args != decl->num_members) {
        _ms_program_error(program, expr->token, "Invalid number of members in object. Expected %d but got %d.", decl->num_members, expr->object.num_args);
        return;
    }

    bool is_const = true;
    for (int i = 0; i < expr->object.num_args; i++) {
        if (strcmp(expr->object.names[i], decl->members[i].name) != 0) {
            _ms_program_error(program, expr->token, "Incorrect member position for type. Expected %s but got %s.", decl->members[i].name, expr->object.names[i]);
            return;
        }

        mscript_type_t *member_type = decl->members[i].type;
        _ms_verify_expr_with_cast(program, &(expr->object.args[i]), member_type);
        if (program->error) return;

        if (!expr->object.args[i]->is_const) {
            is_const = false;
        }
    }

    expr->is_const = is_const;
    if (is_const) {
        int num_args = expr->object.num_args;
        _ms_const_val_t *args = malloc(sizeof(_ms_const_val_t) * num_args);  
        for (int i = 0; i < num_args; i++) {
            args[i] = expr->object.args[i]->const_val;
        }
        expr->const_val = _ms_const_val_object(num_args, args);
    }
    expr->result_type = expected_type;
    expr->lvalue = _ms_lvalue_invalid();
}

static void _ms_verify_cast_expr(mscript_program_t *program, _ms_expr_t *expr, mscript_type_t *expected_type) {
    assert(false);
}

static void opcode_iadd(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IADD;
    compiler_push_opcode(program, op);
}

static void opcode_fadd(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FADD;
    compiler_push_opcode(program, op);
}

static void opcode_isub(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_ISUB;
    compiler_push_opcode(program, op);
}

static void opcode_fsub(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FSUB;
    compiler_push_opcode(program, op);
}

static void opcode_imul(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IMUL;
    compiler_push_opcode(program, op);
}

static void opcode_fmul(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FMUL;
    compiler_push_opcode(program, op);
}

static void opcode_idiv(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IDIV;
    compiler_push_opcode(program, op);
}

static void opcode_fdiv(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FDIV;
    compiler_push_opcode(program, op);
}

static void opcode_ilte(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_ILTE;
    compiler_push_opcode(program, op);
}

static void opcode_flte(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FLTE;
    compiler_push_opcode(program, op);
}

static void opcode_ilt(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_ILT;
    compiler_push_opcode(program, op);
}

static void opcode_flt(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FLT;
    compiler_push_opcode(program, op);
}

static void opcode_igte(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IGTE;
    compiler_push_opcode(program, op);
}

static void opcode_fgte(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FGTE;
    compiler_push_opcode(program, op);
}

static void opcode_igt(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IGT;
    compiler_push_opcode(program, op);
}

static void opcode_fgt(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FGT;
    compiler_push_opcode(program, op);
}

static void opcode_ieq(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IEQ;
    compiler_push_opcode(program, op);
}

static void opcode_feq(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FEQ;
    compiler_push_opcode(program, op);
}

static void opcode_ineq(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_INEQ;
    compiler_push_opcode(program, op);
}

static void opcode_fneq(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FNEQ;
    compiler_push_opcode(program, op);
}

static void opcode_iinc(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_IINC;
    compiler_push_opcode(program, op);
}

static void opcode_finc(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_FINC;
    compiler_push_opcode(program, op);
}

static void opcode_not(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_NOT;
    compiler_push_opcode(program, op);
}

static void opcode_f2i(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_F2I;
    compiler_push_opcode(program, op);
}

static void opcode_i2f(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_I2F;
    compiler_push_opcode(program, op);
}

static void opcode_copy(mscript_program_t *program, int offset, int size) {
    assert(offset >= 0 && size >= 0);

    struct opcode op;
    op.type = OPCODE_COPY;
    op.load_store.offset = offset;
    op.load_store.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_int(mscript_program_t *program, int val) {
    struct opcode op;
    op.type = OPCODE_INT;
    op.int_val = val;
    compiler_push_opcode(program, op);
}

static void opcode_float(mscript_program_t *program, float val) {
    struct opcode op;
    op.type = OPCODE_FLOAT;
    op.float_val = val;
    compiler_push_opcode(program, op);
}

static void opcode_local_store(mscript_program_t *program, int offset, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_LOCAL_STORE;
    op.load_store.offset = offset;
    op.load_store.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_local_load(mscript_program_t *program, int offset, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_LOCAL_LOAD;
    op.load_store.offset = offset;
    op.load_store.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_jf(mscript_program_t *program, int label) {
    struct opcode op;
    op.type = OPCODE_JF;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_jmp(mscript_program_t *program, int label) {
    struct opcode op;
    op.type = OPCODE_JMP;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_call(mscript_program_t *program, char *str) {
    assert(false);
    struct opcode op;
    op.type = OPCODE_CALL;
    strncpy(op.string, str, MSCRIPT_MAX_SYMBOL_LEN);
    op.string[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    compiler_push_opcode(program, op);
}

static void opcode_return(mscript_program_t *program, int size) {
    struct opcode op;
    op.type = OPCODE_RETURN;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_push(mscript_program_t *program, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_PUSH;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_pop(mscript_program_t *program, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_POP;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_create(mscript_program_t *program, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_ARRAY_CREATE;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_store(mscript_program_t *program, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_ARRAY_STORE;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_load(mscript_program_t *program, int size) {
    assert(size >= 0);

    struct opcode op;
    op.type = OPCODE_ARRAY_LOAD;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_length(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_ARRAY_LENGTH;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_int(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_INT;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_float(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_FLOAT;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_bool(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_BOOL;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_string(mscript_program_t *program) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_STRING;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_string_const(mscript_program_t *program, char *string) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_STRING_CONST;
    strncpy(op.string, string, MSCRIPT_MAX_SYMBOL_LEN);
    op.string[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    compiler_push_opcode(program, op);
}

static void opcode_intermediate_label(mscript_program_t *program, int label) {
    struct opcode op;
    op.type = OPCODE_INTERMEDIATE_LABEL;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_intermediate_func(mscript_program_t *program, char *str) {
    struct opcode op;
    op.type = OPCODE_INTERMEDIATE_FUNC;
    strncpy(op.string, str, MSCRIPT_MAX_SYMBOL_LEN);
    op.string[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    compiler_push_opcode(program, op);
}

static void opcode_intermediate_call(mscript_program_t *program, char *str) {
    struct opcode op;
    op.type = OPCODE_INTERMEDIATE_CALL;
    strncpy(op.string, str, MSCRIPT_MAX_SYMBOL_LEN);
    op.string[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    compiler_push_opcode(program, op);
}

static void opcode_intermediate_jmp(mscript_program_t *program, int label) {
    struct opcode op;
    op.type = OPCODE_INTERMEDIATE_JMP;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_intermediate_jf(mscript_program_t *program, int label) {
    struct opcode op;
    op.type = OPCODE_INTERMEDIATE_JF;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_intermediate_string(mscript_program_t *program, char *string) {
    struct opcode op;
    op.type = OPCODE_INTERMEDIATE_STRING;
    op.intermediate_string = string;
    compiler_push_opcode(program, op);
}

static void compiler_init(mscript_program_t *program) {
    struct compiler *compiler = &program->compiler;
    compiler->cur_label = 0;
    compiler->cur_function_decl = NULL;
}

static void compiler_deinit(mscript_program_t *program) {
}

static void compiler_push_opcode(mscript_program_t *program, struct opcode op) {
    //struct array_opcode *opcodes = &(program->compiler.cur_function_decl->opcodes);
    //array_push(opcodes, op);
}

static int compiler_new_label(mscript_program_t *program) {
    struct compiler *compiler = &program->compiler;
    return compiler->cur_label++;
}

static void compile_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    switch (stmt->type) {
        case _MS_STMT_IF:
            compile_if_stmt(program, stmt);
            break;
        case _MS_STMT_RETURN:
            compile_return_stmt(program, stmt);
            break;
        case _MS_STMT_BLOCK:
            compile_block_stmt(program, stmt);
            break;
        case _MS_STMT_GLOBAL_DECLARATION:
            assert(false);
            break;
        case _MS_STMT_FUNCTION_DECLARATION:
            compile_function_declaration_stmt(program, stmt);
            break;
        case _MS_STMT_VARIABLE_DECLARATION:
            compile_variable_declaration_stmt(program, stmt);
            break;
        case _MS_STMT_EXPR:
            compile_expr_stmt(program, stmt);
            break;
        case _MS_STMT_FOR:
            compile_for_stmt(program, stmt);
            break;
        case _MS_STMT_STRUCT_DECLARATION:
        case _MS_STMT_ENUM_DECLARATION:
        case _MS_STMT_IMPORT:
        case _MS_STMT_IMPORT_FUNCTION:
            break;

    }
}

static void compile_if_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_IF);

    int num_stmts = stmt->if_stmt.num_stmts;
    _ms_expr_t **conds = stmt->if_stmt.conds;
    _ms_stmt_t **stmts = stmt->if_stmt.stmts;
    _ms_stmt_t *else_stmt = stmt->if_stmt.else_stmt;

    int else_if_label = -1;
    int final_label = compiler_new_label(program);

    for (int i = 0; i < num_stmts; i++) {
        compile_expr(program, conds[i]);
        else_if_label = compiler_new_label(program);
        opcode_intermediate_jf(program, else_if_label);
        compile_stmt(program, stmts[i]);
        opcode_intermediate_jmp(program, final_label);
        opcode_intermediate_label(program, else_if_label);
    }

    if (else_stmt) {
        compile_stmt(program, else_stmt);
    }

    opcode_intermediate_label(program, final_label);
}

static void compile_for_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_FOR);

    _ms_expr_t *init = stmt->for_stmt.init;
    _ms_expr_t *cond = stmt->for_stmt.cond;
    _ms_expr_t *inc = stmt->for_stmt.inc;
    _ms_stmt_t *body = stmt->for_stmt.body;

    int cond_label = compiler_new_label(program);
    int end_label = compiler_new_label(program);

    compile_expr(program, init);
    opcode_pop(program, init->result_type->size);
    opcode_intermediate_label(program, cond_label);
    compile_expr(program, cond);
    opcode_intermediate_jf(program, end_label);
    compile_stmt(program, body);
    compile_expr(program, inc);
    opcode_pop(program, inc->result_type->size);
    opcode_intermediate_jmp(program, cond_label);
    opcode_intermediate_label(program, end_label);
}

static void compile_return_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_RETURN);

    //_ms_function_decl_t *decl = program->compiler.cur_function_decl;
    _ms_function_decl_t *decl = NULL;
    assert(decl);

    _ms_expr_t *expr = stmt->return_stmt.expr;
    if (expr) {
        compile_expr(program, stmt->return_stmt.expr);
        opcode_return(program, decl->return_type->size);
    }
    else {
        //opcode_pop(program, program->compiler.cur_function_decl->block_size);
        opcode_return(program, 0);
    }
}

static void compile_block_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_BLOCK);

    int num_stmts = stmt->block.num_stmts;
    _ms_stmt_t **stmts = stmt->block.stmts;
    for (int i = 0; i < num_stmts; i++) {
        compile_stmt(program, stmts[i]);
    }
}

static void compile_expr_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_EXPR);

    _ms_expr_t *expr = stmt->expr;
    compile_expr(program, expr);
    opcode_pop(program, expr->result_type->size);
}

static void compile_function_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_FUNCTION_DECLARATION);

    _ms_function_decl_t *decl = _ms_symbol_table_get_function_decl(&program->symbol_table, stmt->function_declaration.name);
    assert(decl);

    program->compiler.cur_function_decl = decl;
    program->compiler.cur_label = 0;

    opcode_intermediate_func(program, decl->name);

    opcode_push(program, decl->block_size);
    compile_stmt(program, stmt->function_declaration.body); 
    if (decl->return_type->type == MSCRIPT_TYPE_VOID) {
        opcode_return(program, 0);
    }

    for (int i = 0; i < decl->opcodes.length; i++) {
        struct opcode op = decl->opcodes.data[i];
        if (op.type == OPCODE_INTERMEDIATE_LABEL) {
            array_reserve(&(decl->labels), op.label + 1);
            decl->labels.data[op.label] = i;
            if (decl->labels.length <= op.label) {
                decl->labels.length = op.label + 1;
            }
        }
    }
}

static void compile_variable_declaration_stmt(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_VARIABLE_DECLARATION);

    _ms_expr_t *assignment_expr = stmt->variable_declaration.assignment_expr;
    if (assignment_expr) {
        compile_expr(program, assignment_expr);
        opcode_pop(program, assignment_expr->result_type->size);
    }
}

static void compile_expr(mscript_program_t *program, _ms_expr_t *expr) {
    switch (expr->type) {
        case _MS_EXPR_UNARY_OP:
            compile_unary_op_expr(program, expr);
            break;
        case _MS_EXPR_BINARY_OP:
            compile_binary_op_expr(program, expr);
            break;
        case _MS_EXPR_CALL:
            compile_call_expr(program, expr);
            break;
        case _MS_EXPR_DEBUG_PRINT:
            compile_debug_print_expr(program, expr);
            break;
        case _MS_EXPR_ARRAY_ACCESS:
            compile_array_access_expr(program, expr);
            break;
        case _MS_EXPR_MEMBER_ACCESS:
            compile_member_access_expr(program, expr);
            break;
        case _MS_EXPR_ASSIGNMENT:
            compile_assignment_expr(program, expr);
            break;
        case _MS_EXPR_INT:
            compile_int_expr(program, expr);
            break;
        case _MS_EXPR_FLOAT:
            compile_float_expr(program, expr);
            break;
        case _MS_EXPR_SYMBOL:
            compile_symbol_expr(program, expr);
            break;
        case _MS_EXPR_NULL:
            compile_null_expr(program, expr);
            break;
        case _MS_EXPR_STRING:
            compile_string_expr(program, expr);
            break;
        case _MS_EXPR_ARRAY:
            compile_array_expr(program, expr);
            break;
        case _MS_EXPR_OBJECT:
            compile_object_expr(program, expr);
            break;
        case _MS_EXPR_CAST:
            compile_cast_expr(program, expr);
            break;
    }
}

static void compile_lvalue_expr(mscript_program_t *program, _ms_expr_t *expr) {
    switch (expr->type) {
        case _MS_EXPR_UNARY_OP:
        case _MS_EXPR_BINARY_OP:
        case _MS_EXPR_CALL:
        case _MS_EXPR_DEBUG_PRINT:
        case _MS_EXPR_ASSIGNMENT:
        case _MS_EXPR_INT:
        case _MS_EXPR_FLOAT:
        case _MS_EXPR_STRING:
        case _MS_EXPR_ARRAY:
        case _MS_EXPR_OBJECT:
        case _MS_EXPR_CAST:
        case _MS_EXPR_NULL:
            assert(false);
            break;
        case _MS_EXPR_ARRAY_ACCESS:
            {
                compile_expr(program, expr->array_access.left);
                compile_expr(program, expr->array_access.right);
                opcode_int(program, expr->result_type->size);
                opcode_imul(program);
            }
            break;
        case _MS_EXPR_MEMBER_ACCESS:
            {
                mscript_type_t *struct_type = expr->member_access.left->result_type;
                assert(struct_type->type == MSCRIPT_TYPE_STRUCT);

                _ms_struct_decl_t *decl = struct_type->struct_decl;
                assert(decl);

                bool found_member = false;
                int offset = 0;
                for (int i = 0; i < decl->num_members; i++) {
                    if (strcmp(expr->member_access.member_name, decl->members[i].name) == 0) {
                        found_member = true;
                        break;
                    }
                    offset += decl->members[i].type->size;
                }
                assert(found_member);

                compile_lvalue_expr(program, expr->member_access.left);

                _ms_lvalue_t left_lvalue = expr->member_access.left->lvalue; 
                switch (left_lvalue.type) {
                    case LVALUE_LOCAL:
                        break;
                    case LVALUE_ARRAY:
                        opcode_int(program, offset);
                        opcode_iadd(program);
                        break;
                    case LVALUE_INVALID:
                        assert(false);
                        break;
                }
            }
            break;
        case _MS_EXPR_SYMBOL:
            break;
    }
}

static void compile_unary_op_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_UNARY_OP);

    _ms_expr_t *operand = expr->unary_op.operand;
    compile_expr(program, operand);

    switch (expr->unary_op.type) {
        case _MS_UNARY_OP_POST_INC:
            {
                if (operand->result_type->type == MSCRIPT_TYPE_INT) {
                    opcode_iinc(program);
                }
                else if (operand->result_type->type == MSCRIPT_TYPE_FLOAT) {
                    opcode_finc(program);
                }
                else {
                    assert(false);
                }

                compile_lvalue_expr(program, operand);
                switch (operand->lvalue.type) {
                    case LVALUE_LOCAL:
                        opcode_local_store(program, operand->lvalue.offset, expr->result_type->size);
                        break;
                    case LVALUE_ARRAY:
                        opcode_array_store(program, expr->result_type->size);
                        break;
                    case LVALUE_INVALID:
                        assert(false);
                        break;
                }
            }
            break;
        case _MS_UNARY_OP_LOGICAL_NOT:
            {
                assert(operand->result_type->type == MSCRIPT_TYPE_BOOL);
                opcode_not(program);
            }
            break;
    }
}

static void compile_binary_op_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_BINARY_OP);

    _ms_expr_t *left = expr->binary_op.left;
    _ms_expr_t *right = expr->binary_op.right;
    assert(left->result_type->type == right->result_type->type);

    compile_expr(program, left);
    compile_expr(program, right);

    if (left->result_type->type == MSCRIPT_TYPE_INT) {
        switch (expr->binary_op.type) {
            case _MS_BINARY_OP_ADD:
                opcode_iadd(program);
                break;
            case _MS_BINARY_OP_SUB:
                opcode_isub(program);
                break;
            case _MS_BINARY_OP_MUL:
                opcode_imul(program);
                break;
            case _MS_BINARY_OP_DIV:
                opcode_idiv(program);
                break;
            case _MS_BINARY_OP_LTE:
                opcode_ilte(program);
                break;
            case _MS_BINARY_OP_LT:
                opcode_ilt(program);
                break;
            case _MS_BINARY_OP_GTE:
                opcode_igte(program);
                break;
            case _MS_BINARY_OP_GT:
                opcode_igt(program);
                break;
            case _MS_BINARY_OP_EQ:
                opcode_ieq(program);
                break;
            case _MS_BINARY_OP_NEQ:
                opcode_ineq(program);
                break;
        }
    }
    else if (left->result_type->type == MSCRIPT_TYPE_FLOAT) {
        switch (expr->binary_op.type) {
            case _MS_BINARY_OP_ADD:
                opcode_fadd(program);
                break;
            case _MS_BINARY_OP_SUB:
                opcode_fsub(program);
                break;
            case _MS_BINARY_OP_MUL:
                opcode_fmul(program);
                break;
            case _MS_BINARY_OP_DIV:
                opcode_fdiv(program);
                break;
            case _MS_BINARY_OP_LTE:
                opcode_flte(program);
                break;
            case _MS_BINARY_OP_LT:
                opcode_flt(program);
                break;
            case _MS_BINARY_OP_GTE:
                opcode_fgte(program);
                break;
            case _MS_BINARY_OP_GT:
                opcode_fgt(program);
                break;
            case _MS_BINARY_OP_EQ:
                opcode_feq(program);
                break;
            case _MS_BINARY_OP_NEQ:
                opcode_fneq(program);
                break;
        }
    }
    else if (left->result_type->type == MSCRIPT_TYPE_ENUM) {
        switch (expr->binary_op.type) {
            case _MS_BINARY_OP_ADD:
            case _MS_BINARY_OP_SUB:
            case _MS_BINARY_OP_MUL:
            case _MS_BINARY_OP_DIV:
                assert(false);
                break;
            case _MS_BINARY_OP_LTE:
                opcode_ilte(program);
                break;
            case _MS_BINARY_OP_LT:
                opcode_ilt(program);
                break;
            case _MS_BINARY_OP_GTE:
                opcode_igte(program);
                break;
            case _MS_BINARY_OP_GT:
                opcode_igt(program);
                break;
            case _MS_BINARY_OP_EQ:
                opcode_ieq(program);
                break;
            case _MS_BINARY_OP_NEQ:
                opcode_ineq(program);
                break;
        }
    }
    else if (left->result_type->type == MSCRIPT_TYPE_BOOL) {
        switch (expr->binary_op.type) {
            case _MS_BINARY_OP_ADD:
            case _MS_BINARY_OP_SUB:
            case _MS_BINARY_OP_MUL:
            case _MS_BINARY_OP_DIV:
            case _MS_BINARY_OP_LTE:
            case _MS_BINARY_OP_LT:
            case _MS_BINARY_OP_GTE:
            case _MS_BINARY_OP_GT:
                assert(false);
                break;
            case _MS_BINARY_OP_EQ:
                opcode_ieq(program);
                break;
            case _MS_BINARY_OP_NEQ:
                opcode_ineq(program);
                break;
        }
    }
    else {
        assert(false);
    }
}

static void compile_call_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_CALL);

    for (int i = expr->call.num_args - 1; i >= 0; i--) {
        compile_expr(program, expr->call.args[i]);
    }
    assert(expr->call.function->type == _MS_EXPR_SYMBOL);
    opcode_intermediate_call(program, expr->call.function->symbol);
}

static void compile_debug_print_type(mscript_program_t *program, mscript_type_t *type) {
    switch (type->type) {
        case MSCRIPT_TYPE_VOID:
            {
                opcode_debug_print_string_const(program, "<void>");
                opcode_pop(program, type->size);
            }
            break;
        case MSCRIPT_TYPE_VOID_STAR:
            {
                opcode_debug_print_string_const(program, "<void*>");
                opcode_pop(program, type->size);
            }
            break;
        case MSCRIPT_TYPE_INT:
            {
                opcode_debug_print_int(program);
            }
            break;
        case MSCRIPT_TYPE_FLOAT:
            {
                opcode_debug_print_float(program);
            }
            break;
        case MSCRIPT_TYPE_BOOL:
            {
                opcode_debug_print_bool(program);
            }
            break;
        case MSCRIPT_TYPE_STRUCT:
            {
                _ms_struct_decl_t *decl = type->struct_decl;
                assert(decl);

                opcode_debug_print_string_const(program, "{");
                int member_offset = 0;
                for (int i = 0; i < decl->num_members; i++) {
                    mscript_type_t *member_type = decl->members[i].type;
                    int member_type_size = member_type->size;
                    char *member_name = decl->members[i].name;

                    opcode_debug_print_string_const(program, member_name);
                    opcode_debug_print_string_const(program, ": ");
                    opcode_copy(program, type->size - member_offset, member_type_size); 
                    compile_debug_print_type(program, member_type);
                    member_offset += member_type_size;
                    
                    if (i != decl->num_members - 1) {
                        opcode_debug_print_string_const(program, ", ");
                    }
                }
                opcode_debug_print_string_const(program, "}");

                opcode_pop(program, type->size);
            }
            break;
        case MSCRIPT_TYPE_ARRAY:
            {
                opcode_debug_print_string_const(program, "<array>");
                opcode_pop(program, type->size);
            }
            break;
        case MSCRIPT_TYPE_CHAR_STAR:
            {
                opcode_debug_print_string(program);
            }
            break;
        case MSCRIPT_TYPE_ENUM:
            {
                opcode_debug_print_int(program);
            }
            break;
    }
}

static void compile_debug_print_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_DEBUG_PRINT);

    int num_args = expr->debug_print.num_args;
    _ms_expr_t **args = expr->debug_print.args;
    for (int i = 0; i < num_args; i++) {
        _ms_expr_t *arg = args[i];
        mscript_type_t *arg_type = arg->result_type;

        compile_expr(program, arg);
        compile_debug_print_type(program, arg_type);
    }
}

static void compile_array_access_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_ARRAY_ACCESS);

    _ms_expr_t *left = expr->array_access.left;
    _ms_expr_t *right = expr->array_access.right;

    assert(left->result_type->type == MSCRIPT_TYPE_ARRAY);
    assert(right->result_type->type == MSCRIPT_TYPE_INT);

    compile_lvalue_expr(program, left);

    _ms_lvalue_t lvalue = left->lvalue;
    switch (lvalue.type) {
        case LVALUE_LOCAL:
            opcode_local_load(program, lvalue.offset, left->result_type->size);
            break;
        case LVALUE_ARRAY:
            opcode_array_load(program, left->result_type->size);
            break;
        case LVALUE_INVALID:
            assert(false);
            break;
    }

    compile_expr(program, right);
    opcode_int(program, expr->result_type->size);
    opcode_imul(program);
    opcode_array_load(program, expr->result_type->size);
}

static void compile_member_access_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_MEMBER_ACCESS);

    _ms_expr_t *left = expr->member_access.left;

    if (left->result_type->type == MSCRIPT_TYPE_STRUCT) {
        compile_lvalue_expr(program, expr);
        _ms_lvalue_t lvalue = expr->lvalue;
        switch (lvalue.type) {
            case LVALUE_LOCAL:
                opcode_local_load(program, lvalue.offset, expr->result_type->size);
                break;
            case LVALUE_ARRAY:
                opcode_array_load(program, expr->result_type->size);
                break;
            case LVALUE_INVALID:
                assert(false);
                break;
        }
    }
    else if (left->result_type->type == MSCRIPT_TYPE_ARRAY) {
        compile_lvalue_expr(program, left);
        _ms_lvalue_t lvalue = left->lvalue;
        switch (lvalue.type) {
            case LVALUE_LOCAL:
                opcode_local_load(program, lvalue.offset, left->result_type->size);
                break;
            case LVALUE_ARRAY:
                opcode_array_load(program, left->result_type->size);
                break;
            case LVALUE_INVALID:
                assert(false);
                break;
        }
        opcode_array_length(program);
    }
    else {
        assert(false);
    }
}

static void compile_assignment_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_ASSIGNMENT);
    assert(expr->result_type == expr->assignment.right->result_type);

    _ms_expr_t *left = expr->assignment.left;
    _ms_expr_t *right = expr->assignment.right;
    _ms_lvalue_t lvalue = left->lvalue;

    compile_expr(program, right);
    compile_lvalue_expr(program, left);

    switch (lvalue.type) {
        case LVALUE_LOCAL:
            opcode_local_store(program, lvalue.offset, expr->result_type->size);
            break;
        case LVALUE_ARRAY:
            opcode_array_store(program, expr->result_type->size);
            break;
        case LVALUE_INVALID:
            assert(false);
            break;
    }
}

static void compile_int_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_INT);
    opcode_int(program, expr->int_val);
}

static void compile_float_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_FLOAT);
    opcode_float(program, expr->float_val);
}

static void compile_symbol_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_SYMBOL);

    /*
    struct enum_value *enum_value = map_get(&program->enum_map, expr->symbol);
    if (enum_value) {
        opcode_int(program, enum_value->val);
    }
    else {
        _ms_lvalue_t lvalue = expr->lvalue;
        switch (lvalue.type) {
            case LVALUE_INVALID:
            case LVALUE_ARRAY:
                assert(false);
                break;
            case LVALUE_LOCAL:
                opcode_local_load(program, lvalue.offset, expr->result_type->size);
                break;
        }
    }
    */
}

static void compile_null_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_NULL);

    assert(expr->result_type->type == MSCRIPT_TYPE_ARRAY);
    opcode_int(program, 0);
}

static void compile_string_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_STRING);

    opcode_intermediate_string(program, expr->string);
}

static void compile_array_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_ARRAY);

    assert(expr->result_type->type == MSCRIPT_TYPE_ARRAY);
    mscript_type_t *arg_type = expr->result_type->array_member_type;

    int size = arg_type->size;
    opcode_array_create(program, size);

    int num_args = expr->array.num_args;
    if (num_args > 0) {
        for (int i = 0; i < expr->array.num_args; i++) {
            _ms_expr_t *arg = expr->array.args[i];
            compile_expr(program, arg);
        }

        int result_type_size = expr->result_type->size;
        opcode_copy(program, num_args * size + result_type_size, result_type_size);
        opcode_int(program, 0);
        opcode_array_store(program, num_args * size);
        opcode_pop(program, num_args * size);
    }
}

static void compile_object_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_OBJECT);

    int num_args = expr->object.num_args;
    for (int i = 0; i < num_args; i++) {
        _ms_expr_t *arg = expr->object.args[i];
        compile_expr(program, arg);
    }
}

static void compile_cast_expr(mscript_program_t *program, _ms_expr_t *expr) {
    assert(expr->type == _MS_EXPR_CAST);

    mscript_type_t *cast_type = expr->result_type;
    mscript_type_t *arg_type = expr->cast.arg->result_type;
    _ms_expr_t *arg = expr->cast.arg;

    compile_expr(program, arg);

    if (cast_type->type == MSCRIPT_TYPE_INT && arg_type->type == MSCRIPT_TYPE_INT) {
    }
    else if (cast_type->type == MSCRIPT_TYPE_FLOAT && arg_type->type == MSCRIPT_TYPE_FLOAT) {
    }
    else if (cast_type->type == MSCRIPT_TYPE_BOOL && arg_type->type == MSCRIPT_TYPE_BOOL) {
    }
    else if (cast_type->type == MSCRIPT_TYPE_BOOL && arg_type->type == MSCRIPT_TYPE_ARRAY) {
    }
    else if (cast_type->type == MSCRIPT_TYPE_FLOAT && arg_type->type == MSCRIPT_TYPE_INT) {
        opcode_i2f(program);
    }
    else if (cast_type->type == MSCRIPT_TYPE_INT && arg_type->type == MSCRIPT_TYPE_FLOAT) {
        opcode_f2i(program);
    }
    else {
        assert(false);
    }
}

static void vm_init(mscript_program_t *program) {
    struct vm *vm = &program->vm;
    array_init(&vm->stack);
    array_init(&vm->arrays);
    array_init(&vm->strings);
}

#define VM_BINARY_OP(op, arg_type, result_type)\
    assert(sp >= 8);\
    arg_type v1 = *((arg_type*) (stack + sp - 4));\
    arg_type v0 = *((arg_type*) (stack + sp - 8));\
    result_type v = (result_type) (v0 op v1);\
    memcpy(stack + sp - 8, &v, 4);\
    sp -= 4;

static void vm_run(mscript_program_t *program) {
    struct compiler *compiler = &program->compiler;
    struct vm *vm = &program->vm;
    struct opcode *opcodes = program->opcodes.data;

    char *stack = malloc(sizeof(char)*8096);
    int fp = 0;
    int sp = 0;
    int ip = 0;

    *((int*) (stack + sp + 0)) = (int) (50);
    *((int*) (stack + sp + 4)) = (int) (0);
    *((int*) (stack + sp + 8)) = (int) (-2);
    *((int*) (stack + sp + 12)) = (int) (0);

    sp += 16;
    fp = sp;
    ip = *(map_get(&program->func_label_map, "run"));

    uint64_t time0, time1;
    double time_sec;

    time0 = stm_now();

    while (true) {
        if (ip == -1) {
            break;
        }

        struct opcode op = opcodes[ip];
        switch (op.type) {
            case OPCODE_IADD:
                {
                    VM_BINARY_OP(+, int, int);
                }
                break;
            case OPCODE_FADD:
                {
                    VM_BINARY_OP(+, float, float);
                }
                break;
            case OPCODE_ISUB:
                {
                    VM_BINARY_OP(-, int, int);
                }
                break;
            case OPCODE_FSUB:
                {
                    VM_BINARY_OP(-, float, float);
                }
                break;
            case OPCODE_IMUL:
                {
                    VM_BINARY_OP(*, int, int);
                }
                break;
            case OPCODE_FMUL:
                {
                    VM_BINARY_OP(*, float, float);
                }
                break;
            case OPCODE_IDIV:
                {
                    VM_BINARY_OP(/, int, int);
                }
                break;
            case OPCODE_FDIV:
                {
                    VM_BINARY_OP(/, float, float);
                }
                break;
            case OPCODE_ILTE:
                {
                    VM_BINARY_OP(<=, int, int);
                }
                break;
            case OPCODE_FLTE:
                {
                    VM_BINARY_OP(<=, float, int);
                }
                break;
            case OPCODE_ILT:
                {
                    VM_BINARY_OP(<, int, int);
                }
                break;
            case OPCODE_FLT:
                {
                    VM_BINARY_OP(<, float, int);
                }
                break;
            case OPCODE_IGTE:
                {
                    VM_BINARY_OP(>=, int, int);
                }
                break;
            case OPCODE_FGTE:
                {
                    VM_BINARY_OP(>=, float, int);
                }
                break;
            case OPCODE_IGT:
                {
                    VM_BINARY_OP(>, int, int);
                }
                break;
            case OPCODE_FGT:
                {
                    VM_BINARY_OP(>, float, int);
                }
                break;
            case OPCODE_IEQ:
                {
                    VM_BINARY_OP(==, int, int);
                }
                break;
            case OPCODE_FEQ:
                {
                    VM_BINARY_OP(==, float, int);
                }
                break;
            case OPCODE_INEQ:
                {
                    VM_BINARY_OP(!=, int, int);
                }
                break;
            case OPCODE_FNEQ:
                {
                    VM_BINARY_OP(!=, float, int);
                }
                break;
            case OPCODE_IINC:
                {
                    int v = *((int*) (stack + sp - 4));
                    v = v + 1;
                    memcpy(stack + sp - 4, &v, 4);
                }
                break;
            case OPCODE_FINC:
                {
                    float v = *((float*) (stack + sp - 4));
                    v = v + 1.0f;
                    memcpy(stack + sp - 4, &v, 4);
                }
                break;
            case OPCODE_NOT:
                {
                    int v = *((int*) (stack + sp - 4));
                    v = !v;
                    memcpy(stack + sp - 4, &v, 4);
                }
                break;
            case OPCODE_F2I:
                {
                    float vf = *((float*) (stack + sp - 4));
                    int vi = (int) vf;
                    memcpy(stack + sp - 4, &vi, 4);
                }
                break;
            case OPCODE_I2F:
                {
                    int vi = *((int*) (stack + sp - 4));
                    float vf = (float) vi;
                    memcpy(stack + sp - 4, &vf, 4);
                }
                break;
            case OPCODE_COPY:
                {
                    int offset = op.load_store.offset;
                    int size = op.load_store.size;
                    int start = sp - offset; 
                    assert((start >= 0) && (start + size <= sp));

                    char *dest = stack + sp;
                    char *src = stack + start;
                    memmove(dest, src, size);
                    sp += size;
                }
                break;
            case OPCODE_INT:
                {
                    *((int*) (stack + sp)) = op.int_val;
                    sp += 4;
                }
                break;
            case OPCODE_FLOAT:
                {
                    *((float*) (stack + sp)) = op.float_val;
                    sp += 4;
                }
                break;
            case OPCODE_LOCAL_STORE:
                {
                    int offset = op.load_store.offset;
                    int size = op.load_store.size;
                    assert(fp + offset + size <= sp);

                    char *dest = stack + fp + offset;
                    char *src = stack + sp - size;
                    memmove(dest, src, size);
                }
                break;
            case OPCODE_LOCAL_LOAD:
                {
                    int offset = op.load_store.offset;
                    int size = op.load_store.size;
                    assert(fp + offset + size <= sp);

                    char *dest = stack + sp;
                    char *src = stack + fp + offset;
                    memmove(dest, src, size);
                    sp += size;
                }
                break;
            case OPCODE_JF:
                {
                    assert(sp >= 4);

                    int v = *((int*) (stack + sp - 4));
                    sp -= 4;
                    if (!v) {
                        ip = op.label - 1;
                        assert(ip >= 0);
                    }
                }
                break;
            case OPCODE_JMP:
                {
                    ip = op.label - 1;
                    assert(ip >= 0);
                }
                break;
            case OPCODE_CALL:
                {
                    int ret_sp = sp - op.call.args_size;

                    memcpy(stack + sp + 0, &fp, 4);
                    memcpy(stack + sp + 4, &ip, 4);
                    memcpy(stack + sp + 8, &ret_sp, 4);

                    sp += 12;
                    ip = op.call.label - 1;
                    fp = sp;
                }
                break;
            case OPCODE_RETURN:
                {
                    assert(sp >= op.size);

                    char *return_data = stack + sp - op.size;
                    sp = *((int*) (stack + fp - 4));
                    ip = *((int*) (stack + fp - 8));
                    fp = *((int*) (stack + fp - 12));

                    memmove(stack + sp, return_data, op.size);
                    sp += op.size;
                }
                break;
            case OPCODE_PUSH:
                {
                    memset(stack + sp, 0, op.size);
                    sp += op.size;
                }
                break;
            case OPCODE_POP:
                {
                    sp -= op.size;
                }
                break;
            case OPCODE_ARRAY_CREATE:
                {
                    int size = op.size;
                    int array_idx = vm->arrays.length + 1;

                    struct vm_array new_array;
                    new_array.member_size = size;
                    array_init(&new_array.array);
                    array_push(&vm->arrays, new_array);

                    memcpy(stack + sp, &array_idx, 4);
                    sp += 4;
                }
                break;
            case OPCODE_ARRAY_STORE:
                {
                    int size = op.size;

                    int offset = *((int*) (stack + sp - 4));
                    int array_idx = (*((int*) (stack + sp - 8))) - 1;
                    sp -= 8;

                    char *data = stack + sp - size;
                    struct vm_array *array = vm->arrays.data + array_idx;

                    int reserve_size = offset + size;
                    if ((reserve_size % array->member_size) != 0) {
                        reserve_size = array->member_size * ((int) (reserve_size / array->member_size) + 1);
                    }

                    array_reserve(&array->array, reserve_size);
                    if (array->array.length < reserve_size) {
                        array->array.length = reserve_size;
                    }
                    
                    memmove(array->array.data + offset, data, size);
                }
                break;
            case OPCODE_ARRAY_LOAD:
                {
                    int size = op.size;

                    int offset = *((int*) (stack + sp - 4));
                    int array_idx = (*((int*) (stack + sp - 8))) - 1;
                    sp -= 8;

                    struct vm_array *array = vm->arrays.data + array_idx;

                    assert(offset + size <= array->array.length);
                    memmove(stack + sp, array->array.data + offset, size);
                    sp += size;
                }
                break;
            case OPCODE_ARRAY_LENGTH:
                {
                    int array_idx = (*((int*) (stack + sp - 4))) - 1;
                    struct vm_array *array = vm->arrays.data + array_idx;

                    int len = array->array.length / array->member_size;
                    memcpy(stack + sp - 4, &len, 4);
                }
                break;
            case OPCODE_DEBUG_PRINT_INT:
                {
                    int v = *((int*) (stack + sp - 4));
                    sp -= 4;
                    m_logf("%d", v);
                }
                break;
            case OPCODE_DEBUG_PRINT_FLOAT:
                {
                    float v = *((float*) (stack + sp - 4));
                    sp -= 4;
                    m_logf("%f", v);
                }
                break;
            case OPCODE_DEBUG_PRINT_BOOL:
                {
                    int v = *((int*) (stack + sp - 4));
                    sp -= 4;

                    if (v) {
                        m_logf("true");
                    }
                    else {
                        m_logf("false");
                    }
                }
                break;
            case OPCODE_DEBUG_PRINT_STRING:
                {
                    int str_pos = *((int*) (stack + sp - 4));
                    sp -= 4;
                    char *str = program->strings.data + str_pos;
                    m_logf("%s", str);
                }
                break;
            case OPCODE_DEBUG_PRINT_STRING_CONST:
                {
                    m_logf(op.string);
                }
                break;
            case OPCODE_INTERMEDIATE_LABEL:
            case OPCODE_INTERMEDIATE_FUNC:
            case OPCODE_INTERMEDIATE_CALL:
            case OPCODE_INTERMEDIATE_JF:
            case OPCODE_INTERMEDIATE_JMP:
            case OPCODE_INTERMEDIATE_STRING:
                assert(false);
                break;
        }

        ip++;
    }

    time1 = stm_now();
    time_sec = stm_ms(stm_diff(time1, time0));

    m_logf("TIME: %f\n", (float) time_sec);
    m_logf("%d\n", *((int*) stack));
}

static void _ms_program_init(mscript_program_t *prog, mscript_t *mscript, struct file file) {
    prog->file = file;
    prog->error = NULL;
    prog->mscript = mscript;

    _ms_symbol_table_init(&prog->symbol_table);
    vec_init(&prog->exported_types);
    vec_init(&prog->exported_function_decls);
    vec_init(&prog->exported_global_decls);
    vec_init(&prog->exported_const_decls);

    vec_init(&prog->imported_programs);
    array_init(&prog->global_stmts);
    vec_init(&prog->opcodes);
    array_init(&prog->strings);

    parser_init(&prog->parser, file.data);
    compiler_init(prog);
    vm_init(prog);

    map_init(&prog->func_label_map);
}

static void _ms_program_error(mscript_program_t *program, _ms_token_t token, char *fmt, ...) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[255] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 255, fmt, args);
    va_end(args);

    program->error = buffer;
    program->error_token = token;
}

static void _ms_program_error_no_token(mscript_program_t *program, char *fmt, ...) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[255] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 255, fmt, args);
    va_end(args);

    program->error = buffer;
    program->error_token.line = -1;
    program->error_token.col = -1;
}

static void _ms_program_import_program(mscript_program_t *program, mscript_program_t *import) {
    if (import->visited) {
        return;
    }
    import->visited = true;

    for (int i = 0; i < import->exported_types.length; i++) {
        mscript_type_t *type = import->exported_types.data[i];

        if (_ms_symbol_table_get(&program->symbol_table, type->name)) {
            _ms_program_error_no_token(program, "Redeclaration of symbol %s.", type->name);
            goto cleanup;
        }

        _ms_symbol_table_add_type(&program->symbol_table, type);
    }

    for (int i = 0; i < import->exported_function_decls.length; i++) {
        _ms_function_decl_t *decl = import->exported_function_decls.data[i];

        if (_ms_symbol_table_get(&program->symbol_table, decl->name)) {
            _ms_program_error_no_token(program, "Redeclaration of symbol %s.", decl->name);
            goto cleanup;
        }

        _ms_symbol_table_add_function_decl(&program->symbol_table, decl);
    }

    for (int i = 0; i < import->exported_global_decls.length; i++) {
        _ms_global_decl_t *decl = import->exported_global_decls.data[i];

        if (_ms_symbol_table_get(&program->symbol_table, decl->name)) {
            _ms_program_error_no_token(program, "Redeclaration of symbol %s.", decl->name);
            goto cleanup;
        }

        _ms_symbol_table_add_global_decl(&program->symbol_table, decl);
    }

    for (int i = 0; i < import->exported_const_decls.length; i++) {
        _ms_const_decl_t decl = import->exported_const_decls.data[i];

        if (_ms_symbol_table_get(&program->symbol_table, decl.name)) {
            _ms_program_error_no_token(program, "Redeclaration of symbol %s.", decl.name);
            goto cleanup;
        }

        _ms_symbol_table_add_const_decl(&program->symbol_table, decl);
    }

    for (int i = 0; i < import->imported_programs.length; i++) {
        _ms_program_import_program(program, import->imported_programs.data[i]);
    }

cleanup:
    return;
}

static void _ms_program_add_enum_decl(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_ENUM_DECLARATION);

    mscript_type_t *type = malloc(sizeof(mscript_type_t));
    _mscript_type_init(type, stmt->enum_declaration.name, MSCRIPT_TYPE_ENUM, NULL, NULL, 4);
    vec_push(&program->exported_types, type);

    int num_values = stmt->enum_declaration.num_values;
    for (int i = 0; i < num_values; i++) {
        _ms_const_val_t const_val = _ms_const_val_int(i);
        _ms_const_decl_t const_decl = _ms_const_decl(stmt->enum_declaration.value_names[i], type, const_val);
        _ms_symbol_table_add_const_decl(&program->symbol_table, const_decl);
        vec_push(&program->exported_const_decls, const_decl);
    }

    char array_type_name[MSCRIPT_MAX_SYMBOL_LEN + 3];
    strncpy(array_type_name, type->name, MSCRIPT_MAX_SYMBOL_LEN);
    array_type_name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    strcat(array_type_name, "[]");

    mscript_type_t *array_type = malloc(sizeof(mscript_type_t));
    _mscript_type_init(array_type, array_type_name, MSCRIPT_TYPE_ARRAY, type, NULL, 4);
    vec_push(&program->exported_types, array_type);

    _ms_symbol_table_add_type(&program->symbol_table, type);
    _ms_symbol_table_add_type(&program->symbol_table, array_type);
}

static void _ms_program_add_struct_decl_stub(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_STRUCT_DECLARATION);

    _ms_struct_decl_t *decl = malloc(sizeof(_ms_struct_decl_t));
    decl->stmt = stmt;
    decl->recur_state = 0;
    strncpy(decl->name, stmt->struct_declaration.name, MSCRIPT_MAX_SYMBOL_LEN);
    decl->name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    decl->num_members = 0;

    mscript_type_t *type = malloc(sizeof(mscript_type_t));
    _mscript_type_init(type, decl->name, MSCRIPT_TYPE_STRUCT, NULL, decl, -1);
    vec_push(&program->exported_types, type);

    char array_type_name[MSCRIPT_MAX_SYMBOL_LEN + 3];
    strncpy(array_type_name, type->name, MSCRIPT_MAX_SYMBOL_LEN);
    array_type_name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    strcat(array_type_name, "[]");

    mscript_type_t *array_type = malloc(sizeof(mscript_type_t));
    _mscript_type_init(array_type, array_type_name, MSCRIPT_TYPE_ARRAY, type, NULL, 4);
    vec_push(&program->exported_types, array_type);

    _ms_symbol_table_add_type(&program->symbol_table, type);
    _ms_symbol_table_add_type(&program->symbol_table, array_type);
}

static void _ms_program_patch_struct_decl(mscript_program_t *program, _ms_stmt_t *stmt) {
    mscript_type_t *type = _ms_symbol_table_get_type(&program->symbol_table, stmt->struct_declaration.name);
    assert(type);
    _ms_program_patch_struct_decl_recur(program, type);
}

static void _ms_program_patch_struct_decl_recur(mscript_program_t *program, mscript_type_t *type) {
    assert(type->type == MSCRIPT_TYPE_STRUCT);
    if (type->struct_decl->recur_state != 0) {
        return;
    }

    _ms_stmt_t *stmt = type->struct_decl->stmt;
    int num_members = stmt->struct_declaration.num_members;
    type->struct_decl->num_members = num_members;
    for (int i = 0; i < num_members; i++) {
        const char *member_type_name = stmt->struct_declaration.member_types[i].string;
        mscript_type_t *member_type = _ms_symbol_table_get_type(&program->symbol_table, member_type_name);
        if (!member_type) {
            _ms_program_error(program, stmt->token, "Undefined type %s.", member_type_name);
            goto cleanup;
        }

        const char *member_name = stmt->struct_declaration.member_names[i];
        strncpy(type->struct_decl->members[i].name, member_name, MSCRIPT_MAX_SYMBOL_LEN);
        type->struct_decl->members[i].name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
        type->struct_decl->members[i].type = member_type;
    }

    type->size = 0;
    type->struct_decl->recur_state = 1;

    for (int i = 0; i < type->struct_decl->num_members; i++) {
        type->struct_decl->members[i].offset = type->size;

        mscript_type_t *member_type = type->struct_decl->members[i].type;
        if (member_type->type == MSCRIPT_TYPE_STRUCT) {
            if (member_type->struct_decl->recur_state == 0) {
                _ms_program_patch_struct_decl_recur(program, member_type);
            }
            else if (member_type->struct_decl->recur_state == 1) {
                _ms_program_error(program, stmt->token, "Recursive type found.");
                goto cleanup;
            }
        }

        type->size += member_type->size;
    }

    type->struct_decl->recur_state = 2;

cleanup:
    return;
}

static void _ms_program_add_global_decl_stub(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_GLOBAL_DECLARATION);

    _ms_global_decl_t *decl = malloc(sizeof(_ms_global_decl_t));
    strncpy(decl->name, stmt->global_declaration.name, MSCRIPT_MAX_SYMBOL_LEN);
    decl->name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    decl->type = NULL;
    decl->has_initial_value = false;
    vec_push(&program->exported_global_decls, decl);

    _ms_symbol_table_add_global_decl(&program->symbol_table, decl);
}

static void _ms_program_patch_global_decl(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_GLOBAL_DECLARATION);

    _ms_symbol_t *symbol = _ms_symbol_table_get(&program->symbol_table, stmt->global_declaration.name);
    assert(symbol && symbol->type == _MS_SYMBOL_GLOBAL_VAR);

    _ms_global_decl_t *decl = symbol->global_var.decl; 
    assert(decl);

    decl->type = _ms_symbol_table_get_type(&program->symbol_table, stmt->global_declaration.type.string);
    if (!decl->type) {
        _ms_program_error(program, stmt->token, "Undefined type %s.", stmt->global_declaration.type.string);
        goto cleanup;
    }

    if (stmt->global_declaration.init_expr) {
        decl->has_initial_value = true;
        _ms_verify_expr_with_cast(program, &(stmt->global_declaration.init_expr), decl->type);
        if (program->error) goto cleanup;
        if (!stmt->global_declaration.init_expr->is_const) {
            _ms_program_error(program, stmt->token, "Global variables initial value must be const.");
            goto cleanup;
        }
        decl->inital_value = stmt->global_declaration.init_expr->const_val;
    }

    symbol->global_var.offset = program->symbol_table.globals_size;
    program->symbol_table.globals_size += decl->type->size;

cleanup:
    return;
}

static void _ms_program_add_function_decl_stub(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_FUNCTION_DECLARATION);

    _ms_function_decl_t *decl = malloc(sizeof(_ms_function_decl_t)); 
    strncpy(decl->name, stmt->function_declaration.name, MSCRIPT_MAX_SYMBOL_LEN);
    decl->name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    decl->return_type = NULL;
    decl->num_args = 0;
    vec_init(&decl->opcodes);
    array_init(&decl->labels);
    vec_push(&program->exported_function_decls, decl);

    _ms_symbol_table_add_function_decl(&program->symbol_table, decl);
}

static void _ms_program_patch_function_decl(mscript_program_t *program, _ms_stmt_t *stmt) {
    assert(stmt->type == _MS_STMT_FUNCTION_DECLARATION);

    _ms_symbol_t *symbol = _ms_symbol_table_get(&program->symbol_table, stmt->function_declaration.name);
    assert(symbol && symbol->type == _MS_SYMBOL_FUNCTION);

    _ms_function_decl_t *decl = symbol->function_decl;
    assert(decl);

    decl->return_type = _ms_symbol_table_get_type(&program->symbol_table, stmt->function_declaration.return_type.string);
    if (!decl->return_type) {
        _ms_program_error(program, stmt->token, "Undefined function return type %s.", stmt->function_declaration.return_type.string);
        goto cleanup;
    }

    int num_args = stmt->function_declaration.num_args;
    decl->num_args = num_args;
    decl->args_size = 0;
    for (int i = 0; i < num_args; i++) {
        decl->args[i].type = _ms_symbol_table_get_type(&program->symbol_table, stmt->function_declaration.arg_types[i].string);
        if (!decl->args[i].type) {
            _ms_program_error(program, stmt->token, "Undefined function argument type %s.", stmt->function_declaration.arg_types[i].string);
            goto cleanup;
        }
        decl->args_size += decl->args[i].type->size;

        strncpy(decl->args[i].name, stmt->function_declaration.arg_names[i], MSCRIPT_MAX_SYMBOL_LEN);
        decl->args[i].name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    }

cleanup:
    return;
}

static int program_add_string(mscript_program_t *program, char *string) {
    int len = (int) strlen(string);
    array_pusharr(&program->strings, string, len + 1);
    return program->strings.length - len - 1;
}

static void debug_log_token(_ms_token_t token) {
    switch (token.type) {
        case TOKEN_INT:
            m_logf("[INT: %d]\n", token.int_val);
            break;
        case TOKEN_FLOAT:
            m_logf("[FLOAT: %d]\n", token.float_val);
            break;
        case TOKEN_STRING:
            m_logf("[STRING: %s]\n", token.string);
            break;
        case TOKEN_SYMBOL:
            m_logf("[SYMBOL: %s]\n", token.symbol);
            break;
        case TOKEN_CHAR:
            m_logf("[CHAR: %c]\n", token.char_val);
            break;
        case TOKEN_EOF:
            m_logf("[EOF]\n");
            return;
            break;
    }
}

static void debug_log_tokens(_ms_token_t *tokens) {
    while (tokens->type != TOKEN_EOF) {
        debug_log_token(*tokens);
        tokens++;
    }
}

static void debug_log_stmt(_ms_stmt_t *stmt) {
    switch (stmt->type) {
        case _MS_STMT_IF:
            m_logf("if (");
            debug_log_expr(stmt->if_stmt.conds[0]);
            m_logf(") ");
            debug_log_stmt(stmt->if_stmt.stmts[0]);
            for (int i = 1; i < stmt->if_stmt.num_stmts; i++) {
                m_logf("else if (");
                debug_log_expr(stmt->if_stmt.conds[i]);
                m_logf(") ");
                debug_log_stmt(stmt->if_stmt.stmts[i]);
            }
            if (stmt->if_stmt.else_stmt) {
                m_logf("else ");
                debug_log_stmt(stmt->if_stmt.else_stmt);
            }
            break;
        case _MS_STMT_FOR:
            m_logf("for (");
            debug_log_expr(stmt->for_stmt.init);
            m_logf(";");
            debug_log_expr(stmt->for_stmt.cond);
            m_logf(";");
            debug_log_expr(stmt->for_stmt.inc);
            m_logf(") ");
            debug_log_stmt(stmt->for_stmt.body);
            break;
        case _MS_STMT_RETURN:
            m_logf("return ");
            if (stmt->return_stmt.expr) {
                debug_log_expr(stmt->return_stmt.expr);
            }
            m_logf(";\n");
            break;
        case _MS_STMT_BLOCK:
            m_logf("{\n");
            for (int i = 0; i < stmt->block.num_stmts; i++) {
                debug_log_stmt(stmt->block.stmts[i]);
            }
            m_logf("}\n");
            break;
        case _MS_STMT_GLOBAL_DECLARATION:
            m_logf("%s %s = ", stmt->global_declaration.type.string, stmt->global_declaration.name);
            debug_log_expr(stmt->global_declaration.init_expr);
            m_logf("\n");
            break;
        case _MS_STMT_FUNCTION_DECLARATION:
            m_logf("%s", stmt->function_declaration.return_type.string);
            m_logf(" %s(", stmt->function_declaration.name);
            for (int i = 0; i < stmt->function_declaration.num_args; i++) {
                m_logf("%s", stmt->function_declaration.arg_types[i].string);
                m_logf(" %s", stmt->function_declaration.arg_names[i]);
                if (i != stmt->function_declaration.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf(")");
            debug_log_stmt(stmt->function_declaration.body);
            break;
        case _MS_STMT_VARIABLE_DECLARATION:
            m_logf("%s", stmt->variable_declaration.type.string);
            m_logf(" %s;\n", stmt->variable_declaration.name);

            if (stmt->variable_declaration.assignment_expr) {
                debug_log_expr(stmt->variable_declaration.assignment_expr);
                m_logf(";\n");
            }
            break;
        case _MS_STMT_EXPR:
            debug_log_expr(stmt->expr);
            m_logf(";\n");
            break;
        case _MS_STMT_STRUCT_DECLARATION:
            m_logf("struct %s {\n", stmt->struct_declaration.name);
            for (int i = 0; i < stmt->struct_declaration.num_members; i++) {
                m_logf("%s", stmt->struct_declaration.member_types[i].string);
                m_logf(" %s;\n", stmt->struct_declaration.member_names[i]);
            }
            m_logf("}\n");
            break;
        case _MS_STMT_ENUM_DECLARATION:
            m_logf("enum %s {\n", stmt->enum_declaration.name);
            for (int i = 0; i < stmt->enum_declaration.num_values; i++) {
                m_logf("%s", stmt->enum_declaration.value_names[i]);
            }
            m_logf("}\n");
            break;
        case _MS_STMT_IMPORT:
            m_logf("import \"%s\"\n", stmt->import.program_name);
            break;
        case _MS_STMT_IMPORT_FUNCTION:
            m_logf("import_function %s();\n", stmt->import_function.name);
            break;
    }
}

static void debug_log_expr(_ms_expr_t *expr) {
    switch (expr->type) {
        case _MS_EXPR_CAST:
            m_log("((");
            m_logf("%s", expr->cast.type.string);
            m_log(")");
            debug_log_expr(expr->cast.arg);
            m_log(")");
            break;
        case _MS_EXPR_ARRAY_ACCESS:
            m_log("(");
            debug_log_expr(expr->array_access.left);
            m_log("[");
            debug_log_expr(expr->array_access.right);
            m_log("]");
            m_log(")");
            break;
        case _MS_EXPR_MEMBER_ACCESS:
            m_log("(");
            debug_log_expr(expr->member_access.left);
            m_logf(".%s)", expr->member_access.member_name);
            m_log(")");
            break;
        case _MS_EXPR_ASSIGNMENT:
            m_log("(");
            debug_log_expr(expr->assignment.left);
            m_log("=");
            debug_log_expr(expr->assignment.right);
            m_log(")");
            break;
        case _MS_EXPR_UNARY_OP:
            switch (expr->unary_op.type) {
                case _MS_UNARY_OP_POST_INC:
                    m_log("(");
                    debug_log_expr(expr->unary_op.operand);
                    m_log(")++");
                    break;
                case _MS_UNARY_OP_LOGICAL_NOT:
                    m_log("!(");
                    debug_log_expr(expr->unary_op.operand);
                    m_log(")");
                    break;
            }
            break;
        case _MS_EXPR_BINARY_OP:
            m_logf("(");
            debug_log_expr(expr->binary_op.left);
            switch (expr->binary_op.type) {
                case _MS_BINARY_OP_ADD:
                    m_log("+");
                    break;
                case _MS_BINARY_OP_SUB:
                    m_log("-");
                    break;
                case _MS_BINARY_OP_MUL:
                    m_log("*");
                    break;
                case _MS_BINARY_OP_DIV:
                    m_log("/");
                    break;
                case _MS_BINARY_OP_LTE:
                    m_log("<=");
                    break;
                case _MS_BINARY_OP_LT:
                    m_log("<");
                    break;
                case _MS_BINARY_OP_GTE:
                    m_log(">=");
                    break;
                case _MS_BINARY_OP_GT:
                    m_log(">");
                    break;
                case _MS_BINARY_OP_EQ:
                    m_log("==");
                    break;
                case _MS_BINARY_OP_NEQ:
                    m_log("!=");
                    break;
            }
            debug_log_expr(expr->binary_op.right);
            m_logf(")");
            break;
        case _MS_EXPR_INT:
            m_logf("%d", expr->int_val);
            break;
            break;
        case _MS_EXPR_FLOAT:
            m_logf("%f", expr->float_val);
            break;
        case _MS_EXPR_SYMBOL:
            m_logf("%s", expr->symbol);
            break;
        case _MS_EXPR_NULL:
            m_logf("NULL");
            break;
        case _MS_EXPR_STRING:
            m_logf("\"%s\"", expr->string);
            break;
        case _MS_EXPR_ARRAY:
            m_logf("[");
            for (int i = 0; i < expr->array.num_args; i++) {
                debug_log_expr(expr->array.args[i]);
                if (i != expr->array.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf("]");
            break;
        case _MS_EXPR_OBJECT:
            m_logf("{");
            for (int i = 0; i < expr->object.num_args; i++) {
                m_logf("%s = ", expr->object.names[i]);
                debug_log_expr(expr->object.args[i]);
                if (i != expr->object.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf("}");
            break;
        case _MS_EXPR_CALL:
            debug_log_expr(expr->call.function);
            m_logf("(");
            for (int i = 0; i < expr->call.num_args; i++) {
                debug_log_expr(expr->call.args[i]);
                if (i != expr->call.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf(")");
            break;
        case _MS_EXPR_DEBUG_PRINT:
            m_logf("debug_print(");
            for (int i = 0; i < expr->debug_print.num_args; i++) {
                debug_log_expr(expr->debug_print.args[i]);
                if (i != expr->debug_print.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf(")");
            break;
    }
}

static void debug_log_opcodes(struct opcode *opcodes, int num_opcodes) {
    for (int i = 0; i < num_opcodes; i++) {
        m_logf("%d: ", i);
        struct opcode op = opcodes[i];
        switch (op.type) {
            case OPCODE_IADD:
                m_logf("IADD");
                break;
            case OPCODE_FADD:
                m_logf("FADD");
                break;
            case OPCODE_ISUB:
                m_logf("ISUB");
                break;
            case OPCODE_FSUB:
                m_logf("FSUB");
                break;
            case OPCODE_IMUL:
                m_logf("IMUL");
                break;
            case OPCODE_FMUL:
                m_logf("FMUL");
                break;
            case OPCODE_IDIV:
                m_logf("IDIV");
                break;
            case OPCODE_FDIV:
                m_logf("FDIV");
                break;
            case OPCODE_ILTE:
                m_logf("ILTE");
                break;
            case OPCODE_FLTE:
                m_logf("FLTE");
                break;
            case OPCODE_ILT:
                m_logf("ILT");
                break;
            case OPCODE_FLT:
                m_logf("FLT");
                break;
            case OPCODE_IGTE:
                m_logf("IGTE");
                break;
            case OPCODE_FGTE:
                m_logf("FGTE");
                break;
            case OPCODE_IGT:
                m_logf("IGT");
                break;
            case OPCODE_FGT:
                m_logf("FGT");
                break;
            case OPCODE_IEQ:
                m_logf("IEQ");
                break;
            case OPCODE_FEQ:
                m_logf("FEQ");
                break;
            case OPCODE_INEQ:
                m_logf("INEQ");
                break;
            case OPCODE_FNEQ:
                m_logf("FNEQ");
                break;
            case OPCODE_IINC:
                m_logf("IINC");
                break;
            case OPCODE_FINC:
                m_logf("FINC");
                break;
            case OPCODE_NOT:
                m_logf("NOT");
                break;
            case OPCODE_F2I:
                m_logf("F2I");
                break;
            case OPCODE_I2F:
                m_logf("I2F");
                break;
            case OPCODE_COPY:
                m_logf("COPY %d %d", op.load_store.offset, op.load_store.size);
                break;
            case OPCODE_INT:
                m_logf("INT %d", op.int_val);
                break;
            case OPCODE_FLOAT:
                m_logf("CONST_FLOAT %f", op.float_val);
                break;
            case OPCODE_LOCAL_STORE:
                m_logf("LOCAL_STORE %d %d", op.load_store.offset, op.load_store.size);
                break;
            case OPCODE_LOCAL_LOAD:
                m_logf("LOCAL_LOAD %d %d", op.load_store.offset, op.load_store.size);
                break;
            case OPCODE_JF:
                m_logf("JF %d", op.label);
                break;
            case OPCODE_JMP:
                m_logf("JMP %d", op.label);
                break;
            case OPCODE_CALL:
                m_logf("CALL %d %d", op.call.label, op.call.args_size);
                break;
            case OPCODE_RETURN:
                m_logf("RETURN");
                break;
            case OPCODE_PUSH:
                m_logf("PUSH %d", op.size);
                break;
            case OPCODE_POP:
                m_logf("POP %d", op.size);
                break;
            case OPCODE_ARRAY_CREATE:
                m_logf("ARRAY_CREATE %d", op.size);
                break;
            case OPCODE_ARRAY_STORE:
                m_logf("ARRAY_STORE %d", op.size);
                break;
            case OPCODE_ARRAY_LOAD:
                m_logf("ARRAY_LOAD %d", op.size);
                break;
            case OPCODE_ARRAY_LENGTH:
                m_logf("ARRAY_LENGTH");
                break;
            case OPCODE_DEBUG_PRINT_INT:
                m_logf("DEBUG_PRINT_INT");
                break;
            case OPCODE_DEBUG_PRINT_FLOAT:
                m_logf("DEBUG_PRINT_FLOAT");
                break;
            case OPCODE_DEBUG_PRINT_BOOL:
                m_logf("DEBUG_PRINT_BOOL");
                break;
            case OPCODE_DEBUG_PRINT_STRING:
                m_logf("DEBUG_PRINT_STRING");
                break;
            case OPCODE_DEBUG_PRINT_STRING_CONST:
                m_logf("DEBUG_PRINT_SHORT_STRING: %s", op.string);
                break;
            case OPCODE_INTERMEDIATE_LABEL:
                m_logf("INTERMEDIATE_LABEL %d", op.label);
                break;
            case OPCODE_INTERMEDIATE_FUNC:
                m_logf("INTERMEDIATE_FUNC %s", op.string);
                break;
            case OPCODE_INTERMEDIATE_CALL:
                m_logf("INTERMEDIATE_CALL %s", op.string);
                break;
            case OPCODE_INTERMEDIATE_JF:
                m_logf("INTERMEDIATE_JF %d", op.label);
                break;
            case OPCODE_INTERMEDIATE_JMP:
                m_logf("INTERMEDIATE_JMP %d", op.label);
                break;
            case OPCODE_INTERMEDIATE_STRING:
                m_logf("INTERMEDIATE_STRING %s", op.intermediate_string);
                break;
        }
        m_logf("\n");
    }
}

static void _ms_program_load_stage_1(mscript_t *mscript, struct file file) {
    if (!file_load_data(&file)) {
        return;
    }

    char *script_name = file.path + strlen("scripts/");
    mscript_program_t *program = malloc(sizeof(mscript_program_t));
    _ms_program_init(program, mscript, file);
    map_set(&mscript->programs_map, script_name, program);
    vec_push(&mscript->programs_array, program);

    _ms_symbol_table_add_type(&program->symbol_table, &mscript->void_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->void_star_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->void_star_array_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->int_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->int_array_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->float_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->float_array_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->bool_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->bool_array_type);
    _ms_symbol_table_add_type(&program->symbol_table, &mscript->char_star_type);

    {
        _ms_const_val_t const_val = _ms_const_val_bool(false);
        _ms_const_decl_t const_decl = _ms_const_decl("false", &mscript->bool_type, const_val);
        _ms_symbol_table_add_const_decl(&program->symbol_table, const_decl);
    }

    {
        _ms_const_val_t const_val = _ms_const_val_bool(true);
        _ms_const_decl_t const_decl = _ms_const_decl("true", &mscript->bool_type, const_val);
        _ms_symbol_table_add_const_decl(&program->symbol_table, const_decl);
    }

    {
        _ms_const_val_t const_val = _ms_const_val_float(3.1415f);
        _ms_const_decl_t const_decl = _ms_const_decl("PI", &mscript->float_type, const_val);
        _ms_symbol_table_add_const_decl(&program->symbol_table, const_decl);
    }

    {
        _ms_tokenize(program);
        if (program->error) goto cleanup;

        while (true) {
            if (match_eof(program)) {
                break;
            }

            _ms_stmt_t *stmt;
            if (match_symbol(program, "import")) {
                stmt = _ms_parse_import_stmt(program);
                if (program->error) goto cleanup;
            }
            else if (match_symbol(program, "import_function")) {
                stmt = _ms_parse_import_function_stmt(program);
                if (program->error) goto cleanup;
            }
            else if (match_symbol(program, "struct")) {
                stmt = _ms_parse_struct_declaration_stmt(program);
                if (program->error) goto cleanup;
            }
            else if (match_symbol(program, "enum")) {
                stmt = _ms_parse_enum_declaration_stmt(program);
                if (program->error) goto cleanup;
            }
            else if (check_type(program)) {
                _ms_parsed_type_t type = _ms_parse_type(program);
                if (program->error) goto cleanup;

                _ms_token_t peek1 = peek_n(program, 1);
                if (is_char_token(peek1, '(')) {
                    stmt = _ms_parse_function_declaration_stmt(program, type);
                    if (program->error) goto cleanup;
                }
                else {
                    stmt = _ms_parse_global_declaration_stmt(program, type);
                    if (program->error) goto cleanup;
                }
            }
            else {
                _ms_program_error(program, peek(program), "Unknown token.");
                goto cleanup;
            }

            vec_push(&program->global_stmts, stmt);
        }
    }

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
                break;
            case _MS_STMT_ENUM_DECLARATION:
                {
                    _ms_program_add_enum_decl(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_GLOBAL_DECLARATION:
                {
                    _ms_program_add_global_decl_stub(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_STRUCT_DECLARATION:
                {
                    _ms_program_add_struct_decl_stub(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_FUNCTION_DECLARATION:
                {
                    _ms_program_add_function_decl_stub(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_IMPORT_FUNCTION:
                {
                    assert(false);
                }
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }

    file_delete_data(&file);
}

static void _ms_program_load_stage_2(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
                {
                    mscript_program_t **import = map_get(&mscript->programs_map, stmt->import.program_name);
                    if (!import) {
                        _ms_program_error(program, stmt->token, "Cannot find import %s.", stmt->import.program_name);
                        goto cleanup;
                    }
                    if ((*import)->error) {
                        _ms_program_error(program, stmt->token, "Failed to import %s.", stmt->import.program_name);
                        goto cleanup;
                    }

                    vec_push(&program->imported_programs, *import);
                }
                break;
            case _MS_STMT_STRUCT_DECLARATION:
            case _MS_STMT_ENUM_DECLARATION:
            case _MS_STMT_FUNCTION_DECLARATION:
            case _MS_STMT_GLOBAL_DECLARATION:
            case _MS_STMT_IMPORT_FUNCTION:
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

static void _ms_program_load_stage_3(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    {
        const char *key;
        map_iter_t iter = map_iter(&mscript->map);

        while ((key = map_next(&mscript->programs_map, &iter))) {
            mscript_program_t **p = map_get(&mscript->programs_map, key);
            (*p)->visited = false;
        }

        program->visited = true;
        for (int i = 0; i < program->imported_programs.length; i++) {
            _ms_program_import_program(program, program->imported_programs.data[i]);
            if (program->error) goto cleanup;
        }
    }

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
            case _MS_STMT_ENUM_DECLARATION:
            case _MS_STMT_IMPORT_FUNCTION:
            case _MS_STMT_FUNCTION_DECLARATION:
            case _MS_STMT_GLOBAL_DECLARATION:
                break;
            case _MS_STMT_STRUCT_DECLARATION:
                {
                    _ms_program_patch_struct_decl(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

static void _ms_program_load_stage_4(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
            case _MS_STMT_ENUM_DECLARATION:
            case _MS_STMT_IMPORT_FUNCTION:
            case _MS_STMT_STRUCT_DECLARATION:
                break;
            case _MS_STMT_GLOBAL_DECLARATION:
                {
                    _ms_program_patch_global_decl(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_FUNCTION_DECLARATION:
                {
                    _ms_program_patch_function_decl(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

static void _ms_program_load_stage_5(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
            case _MS_STMT_STRUCT_DECLARATION:
            case _MS_STMT_ENUM_DECLARATION:
            case _MS_STMT_GLOBAL_DECLARATION:
            case _MS_STMT_IMPORT_FUNCTION:
                break;
            case _MS_STMT_FUNCTION_DECLARATION:
                {
                    _ms_verify_function_declaration_stmt(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

static void program_load_stage_6(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
            case _MS_STMT_STRUCT_DECLARATION:
            case _MS_STMT_ENUM_DECLARATION:
            case _MS_STMT_IMPORT_FUNCTION:
            case _MS_STMT_GLOBAL_DECLARATION:
                break;
            case _MS_STMT_FUNCTION_DECLARATION:
                {
                }
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

static void program_load_stage_7(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    for (int i = 0; i < program->global_stmts.length; i++) {
        _ms_stmt_t *stmt = program->global_stmts.data[i];
        switch (stmt->type) {
            case _MS_STMT_IMPORT:
            case _MS_STMT_STRUCT_DECLARATION:
            case _MS_STMT_ENUM_DECLARATION:
            case _MS_STMT_IMPORT_FUNCTION:
            case _MS_STMT_GLOBAL_DECLARATION:
                break;
            case _MS_STMT_FUNCTION_DECLARATION:
                {
                    compile_stmt(program, stmt);
                    if (program->error) goto cleanup;
                }
                break;
            case _MS_STMT_IF:
            case _MS_STMT_RETURN:
            case _MS_STMT_BLOCK:
            case _MS_STMT_EXPR:
            case _MS_STMT_FOR:
            case _MS_STMT_VARIABLE_DECLARATION:
                assert(false);
                break;
        }
    }

cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

static void program_load_stage_8(mscript_t *mscript, mscript_program_t *program) {
    if (program->error) return;

    _vec_opcode_t intermediate_opcodes;
    array_init(&intermediate_opcodes);

    /*
    {
        const char *key;
        map_iter_t iter = map_iter(&program->function_decl_map);
        while ((key = map_next(&program->function_decl_map, &iter))) {
            struct function_decl *decl = _ms_symbol_table_get_function_decl(&program->symbol_table, key);
            assert(decl);

            array_pusharr(&intermediate_opcodes, decl->opcodes.data, decl->opcodes.length);
        }
    }
    */

    struct array_int labels;
    array_init(&labels);

    for (int i = 0; i < intermediate_opcodes.length; i++) {
        struct opcode op = intermediate_opcodes.data[i];
        switch (op.type) {
            case OPCODE_IADD:
            case OPCODE_FADD:
            case OPCODE_ISUB:
            case OPCODE_FSUB:
            case OPCODE_IMUL:
            case OPCODE_FMUL:
            case OPCODE_IDIV:
            case OPCODE_FDIV:
            case OPCODE_ILTE:
            case OPCODE_FLTE:
            case OPCODE_ILT:
            case OPCODE_FLT:
            case OPCODE_IGTE:
            case OPCODE_FGTE:
            case OPCODE_IGT:
            case OPCODE_FGT:
            case OPCODE_IEQ:
            case OPCODE_FEQ:
            case OPCODE_INEQ:
            case OPCODE_FNEQ:
            case OPCODE_IINC:
            case OPCODE_FINC:
            case OPCODE_NOT:
            case OPCODE_F2I:
            case OPCODE_I2F:
            case OPCODE_COPY:
            case OPCODE_INT:
            case OPCODE_FLOAT:
            case OPCODE_LOCAL_STORE:
            case OPCODE_LOCAL_LOAD:
            case OPCODE_RETURN:
            case OPCODE_PUSH:
            case OPCODE_POP:
            case OPCODE_ARRAY_CREATE:
            case OPCODE_ARRAY_STORE:
            case OPCODE_ARRAY_LOAD:
            case OPCODE_ARRAY_LENGTH:
            case OPCODE_DEBUG_PRINT_INT:
            case OPCODE_DEBUG_PRINT_FLOAT:
            case OPCODE_DEBUG_PRINT_BOOL:
            case OPCODE_DEBUG_PRINT_STRING:
            case OPCODE_DEBUG_PRINT_STRING_CONST:
                {
                    vec_push(&program->opcodes, op);
                }
                break;
            case OPCODE_INTERMEDIATE_LABEL:
                {
                }
                break;
            case OPCODE_INTERMEDIATE_FUNC:
                {
                    map_set(&program->func_label_map, op.string, program->opcodes.length);
                    labels.length = 0;

                    int j = i + 1;
                    int line_num = program->opcodes.length;
                    while (j < intermediate_opcodes.length) {
                        struct opcode op2 = intermediate_opcodes.data[j];

                        if (op2.type == OPCODE_INTERMEDIATE_LABEL) {
                            while (op2.label >= labels.length) {
                                array_push(&labels, -1);
                            }
                            labels.data[op2.label] = line_num;
                        }
                        else if (op2.type == OPCODE_INTERMEDIATE_FUNC) {
                            break;
                        }
                        else {
                            line_num++;
                        }

                        j++;
                    }
                }
                break;
            case OPCODE_INTERMEDIATE_CALL:
                {
                    vec_push(&program->opcodes, op);
                }
                break;
            case OPCODE_INTERMEDIATE_JMP:
                {
                    assert(op.label < labels.length);

                    struct opcode op2;
                    op2.type = OPCODE_JMP;
                    op2.label = labels.data[op.label];
                    vec_push(&program->opcodes, op2);
                }
                break;
            case OPCODE_INTERMEDIATE_JF:
                {
                    assert(op.label < labels.length);

                    struct opcode op2;
                    op2.type = OPCODE_JF;
                    op2.label = labels.data[op.label];
                    vec_push(&program->opcodes, op2);
                }
                break;
            case OPCODE_INTERMEDIATE_STRING:
                {
                    int pos = program_add_string(program, op.intermediate_string);

                    struct opcode op2;
                    op2.type = OPCODE_INT;
                    op2.int_val = pos;
                    vec_push(&program->opcodes, op2);
                }
                break;
            case OPCODE_JF:
            case OPCODE_JMP:
            case OPCODE_CALL:
                assert(false);
                break;
        }
    }

    for (int i = 0; i < program->opcodes.length; i++) {
        struct opcode *op = &(program->opcodes.data[i]);
        if (op->type == OPCODE_INTERMEDIATE_CALL) {
            int *label = map_get(&program->func_label_map, op->string);
            assert(label);

            _ms_function_decl_t *decl = _ms_symbol_table_get_function_decl(&program->symbol_table, op->string);
            assert(decl);

            op->type = OPCODE_CALL;
            op->call.label = *label;
            op->call.args_size = decl->args_size;
        }
    }

    //m_logf("%s\n", program->file.path);
    //debug_log_opcodes(program->opcodes.data, program->opcodes.length);

//cleanup:
    if (program->error) {
        _ms_token_t tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", program->file.path, line, col); 
        m_logf("%s\n", program->error);
    }
}

mscript_t *mscript_create(void) {
    mscript_t *mscript = malloc(sizeof(mscript_t));
    map_init(&mscript->programs_map);
    vec_init(&mscript->programs_array);

    _mscript_type_init(&mscript->void_type, "void", MSCRIPT_TYPE_VOID, NULL, NULL, 0);
    _mscript_type_init(&mscript->void_star_type, "void*", MSCRIPT_TYPE_VOID_STAR, NULL, NULL, 4);
    _mscript_type_init(&mscript->void_star_array_type, "void*[]", MSCRIPT_TYPE_ARRAY, &mscript->void_star_type, NULL, 4);
    _mscript_type_init(&mscript->int_type, "int", MSCRIPT_TYPE_INT, NULL, NULL, 4);
    _mscript_type_init(&mscript->int_array_type, "int[]", MSCRIPT_TYPE_ARRAY, &mscript->int_type, NULL, 4);
    _mscript_type_init(&mscript->float_type, "float", MSCRIPT_TYPE_FLOAT, NULL, NULL, 4);
    _mscript_type_init(&mscript->float_array_type, "float[]", MSCRIPT_TYPE_ARRAY, &mscript->float_type, NULL, 4);
    _mscript_type_init(&mscript->bool_type, "bool", MSCRIPT_TYPE_BOOL, NULL, NULL, 4);
    _mscript_type_init(&mscript->bool_array_type, "bool[]", MSCRIPT_TYPE_ARRAY, &mscript->bool_type, NULL, 4);
    _mscript_type_init(&mscript->char_star_type, "char*", MSCRIPT_TYPE_CHAR_STAR, NULL, NULL, 4);

    struct directory dir;
    directory_init(&dir, "scripts");
    for (int i = 0; i < dir.num_files; i++) {
        if (strcmp(dir.files[i].ext, ".mscript") != 0) {
            continue;
        }
        _ms_program_load_stage_1(mscript, dir.files[i]);
    }
    directory_deinit(&dir);

    for (int i = 0; i < mscript->programs_array.length; i++)
        _ms_program_load_stage_2(mscript, mscript->programs_array.data[i]);
    for (int i = 0; i < mscript->programs_array.length; i++)
        _ms_program_load_stage_3(mscript, mscript->programs_array.data[i]);
    for (int i = 0; i < mscript->programs_array.length; i++)
        _ms_program_load_stage_4(mscript, mscript->programs_array.data[i]);
    for (int i = 0; i < mscript->programs_array.length; i++)
        _ms_program_load_stage_5(mscript, mscript->programs_array.data[i]);
    for (int i = 0; i < mscript->programs_array.length; i++)
        program_load_stage_6(mscript, mscript->programs_array.data[i]);
    //for (int i = 0; i < mscript->programs_array.length; i++)
        //program_load_stage_7(mscript, mscript->programs_array.data[i]);
    //for (int i = 0; i < mscript->programs_array.length; i++)
        //program_load_stage_8(mscript, mscript->programs_array.data[i]);

    //mscript_program_t *program = *(map_get(&mscript->programs_map, "testing.mscript"));
    //vm_run(program);

    return mscript;
}
