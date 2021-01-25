#define _CRT_SECURE_NO_WARNINGS
#include "mscript/parser.h"

#include <assert.h>
#include <stdbool.h>

#include "array.h"
#include "file.h"
#include "log.h"
#include "map.h"

struct stmt;
struct expr;
struct parser;
struct pre_compiler;
struct compiler;
struct vm;

array_t(struct mscript_type, array_mscript_type)
typedef map_t(struct mscript_type) map_mscript_type_t;

static struct mscript_type void_type(void);
static struct mscript_type void_star_type(void);
static struct mscript_type int_type(void);
static struct mscript_type float_type(void);
static struct mscript_type struct_type(char *struct_name);
static struct mscript_type array_type(enum mscript_type_type array_type, char *struct_name);
static struct mscript_type string_type(void);
static bool types_equal(struct mscript_type a, struct mscript_type b);
static void type_to_string(struct mscript_type type, char *buffer, int buffer_len);
static int type_size(struct mscript_program *program, struct mscript_type type);

enum lvalue_type {
    LVALUE_INVALID,
    LVALUE_LOCAL,
    LVALUE_ARRAY,
};

struct lvalue {
    enum lvalue_type type;
    int offset;
};

struct lvalue lvalue_invalid(void);
struct lvalue lvalue_local(int offset);
struct lvalue lvalue_array(void);

enum token_type {
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_SYMBOL,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_EOF,
};

struct token {
    enum token_type type;
    int line, col;
    union {
        char *symbol;
        char *string;
        int int_value;
        float float_value;
        char char_value;
    };
};
array_t(struct token, array_token)

struct allocator {
    size_t bytes_allocated;
    struct array_void_ptr ptrs;
};

static void allocator_init(struct allocator *allocator);
static void allocator_deinit(struct allocator *allocator);
static void *allocator_alloc(struct allocator *allocator, size_t size);

static bool is_char_digit(char c);
static bool is_char_start_of_symbol(char c);
static bool is_char_part_of_symbol(char c);
static bool is_char(char c);
static struct token number_token(const char *text, int *len, int line, int col);
static struct token char_token(char c, int line, int col);
static struct token string_token(struct mscript_program *program, const char *text, int *len, int line, int col);
static struct token symbol_token(const char *text, int *len, int line, int col);
static struct token eof_token(int line, int col);
static void tokenize(struct mscript_program *program);
static void parser_run(struct mscript_program *program, struct mscript *mscript); 

struct parser {
    const char *prog_text;

    struct allocator allocator;

    int token_idx;
    struct array_token tokens;

    char *error;
    struct token error_token;
};

static void parser_init(struct parser *parser, const char *prog_text);
static void parser_deinit(struct parser *program);
static struct token peek(struct mscript_program *program);
static struct token peek_n(struct mscript_program *program, int n);
static void eat(struct mscript_program *program); 
static bool match_char(struct mscript_program *program, char c);
static bool match_char_n(struct mscript_program *program, int n, ...);
static bool match_symbol(struct mscript_program *program, const char *symbol);
static bool match_symbol_n(struct mscript_program *program, int n, ...);
static bool match_eof(struct mscript_program *program);
static bool check_type(struct mscript_program *program);

struct pre_compiler_env_var {
    int offset;
    struct mscript_type type;
};
typedef map_t(struct pre_compiler_env_var) map_pre_compiler_env_var_t;

struct pre_compiler_env_block {
    int offset, size, max_size;
    map_pre_compiler_env_var_t map;
};
array_t(struct pre_compiler_env_block, array_pre_compiler_env_block)

struct pre_compiler {
    struct stmt *function_decl;
    struct array_pre_compiler_env_block env_blocks; 
};

static void pre_compiler_init(struct mscript_program *program);
static void pre_compiler_env_push_block(struct mscript_program *program);
static void pre_compiler_env_pop_block(struct mscript_program *program);
static void pre_compiler_env_add_var(struct mscript_program *program, const char *symbol, struct mscript_type type);
static struct pre_compiler_env_var *pre_compiler_env_get_var(struct mscript_program *program, const char *symbol);
static struct pre_compiler_env_var *pre_compiler_top_env_get_var(struct mscript_program *program, const char *symbol);
static void pre_compiler_start(struct mscript_program *program, struct stmt *function_decl);

static void pre_compiler_type(struct mscript_program *program, struct token token, struct mscript_type type);

static void pre_compiler_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static void pre_compiler_if_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static void pre_compiler_for_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static void pre_compiler_return_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static void pre_compiler_block_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static void pre_compiler_expr_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static void pre_compiler_variable_declaration_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return);
static bool pre_compiler_is_struct_declaration_recursive(struct mscript_program *program, struct stmt *stmt, struct mscript_type cur);
static void pre_compiler_struct_declaration(struct mscript_program *program, struct stmt *stmt);
static void pre_compiler_import_function(struct mscript_program *program, struct stmt *stmt);

static void pre_compiler_expr_with_cast(struct mscript_program *program, struct expr **expr, struct mscript_type type);
static void pre_compiler_expr_lvalue(struct mscript_program *program, struct expr *expr);
static void pre_compiler_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_unary_op_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_binary_op_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_call_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_debug_print_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_member_access_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_assignment_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_int_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_float_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_symbol_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_string_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_array_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_array_access_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_object_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);
static void pre_compiler_cast_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type);

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
    OPCODE_LABEL,
    OPCODE_DEBUG_PRINT_INT,
    OPCODE_DEBUG_PRINT_FLOAT,
    OPCODE_DEBUG_PRINT_STRING,
};

#define OPCODE_MAX_STRING_LEN 15

struct opcode {
    enum opcode_type type;

    union {
        int label;
        int function;
        int int_val;
        float float_val;
        int size;
        char string[OPCODE_MAX_STRING_LEN + 1];

        struct {
            int offset, size;
        } load_store;
    };
};
array_t(struct opcode, array_opcode)

static void opcode_iadd(struct mscript_program *program);
static void opcode_fadd(struct mscript_program *program);
static void opcode_isub(struct mscript_program *program);
static void opcode_fsub(struct mscript_program *program);
static void opcode_imul(struct mscript_program *program);
static void opcode_fmul(struct mscript_program *program);
static void opcode_idiv(struct mscript_program *program);
static void opcode_fdiv(struct mscript_program *program);
static void opcode_ilte(struct mscript_program *program);
static void opcode_flte(struct mscript_program *program);
static void opcode_ilt(struct mscript_program *program);
static void opcode_flt(struct mscript_program *program);
static void opcode_igte(struct mscript_program *program);
static void opcode_fgte(struct mscript_program *program);
static void opcode_igt(struct mscript_program *program);
static void opcode_fgt(struct mscript_program *program);
static void opcode_ieq(struct mscript_program *program);
static void opcode_feq(struct mscript_program *program);
static void opcode_ineq(struct mscript_program *program);
static void opcode_fneq(struct mscript_program *program);
static void opcode_iinc(struct mscript_program *program);
static void opcode_finc(struct mscript_program *program);
static void opcode_f2i(struct mscript_program *program);
static void opcode_i2f(struct mscript_program *program);
static void opcode_copy(struct mscript_program *program, int offset, int size);
static void opcode_int(struct mscript_program *program, int val);
static void opcode_float(struct mscript_program *program, float val);
static void opcode_local_store(struct mscript_program *program, int offset, int size);
static void opcode_local_load(struct mscript_program *program, int offset, int size);
static void opcode_jf(struct mscript_program *program, int label);
static void opcode_jmp(struct mscript_program *program, int label);
static void opcode_call(struct mscript_program *program, int function);
static void opcode_return(struct mscript_program *program, int size);
static void opcode_push(struct mscript_program *program, int size);
static void opcode_pop(struct mscript_program *program, int size);
static void opcode_array_create(struct mscript_program *program, int size);
static void opcode_array_store(struct mscript_program *program, int size);
static void opcode_array_load(struct mscript_program *program, int size);
static void opcode_array_length(struct mscript_program *program);
static void opcode_label(struct mscript_program *program, int label);
static void opcode_debug_print_int(struct mscript_program *program);
static void opcode_debug_print_float(struct mscript_program *program);
static void opcode_debug_print_string(struct mscript_program *program, char *str);

struct compiler {
    int cur_label;
    struct array_opcode opcodes; 
};

static void compiler_init(struct mscript_program *program);
static void compiler_deinit(struct mscript_program *program);
static void compiler_push_opcode(struct mscript_program *program, struct opcode op);
static int compiler_new_label(struct mscript_program *program);

static void compile_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_if_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_for_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_return_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_block_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_expr_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_function_declaration_stmt(struct mscript_program *program, struct stmt *stmt);
static void compile_variable_declaration_stmt(struct mscript_program *program, struct stmt *stmt);

static void compile_expr(struct mscript_program *program, struct expr *expr);
static void compile_lvalue_expr(struct mscript_program *program, struct expr *expr);
static void compile_unary_op_expr(struct mscript_program *program, struct expr *expr);
static void compile_binary_op_expr(struct mscript_program *program, struct expr *expr);
static void compile_call_expr(struct mscript_program *program, struct expr *expr);
static void compile_debug_print_expr(struct mscript_program *program, struct expr *expr);
static void compile_array_access_expr(struct mscript_program *program, struct expr *expr);
static void compile_member_access_expr(struct mscript_program *program, struct expr *expr);
static void compile_assignment_expr(struct mscript_program *program, struct expr *expr);
static void compile_int_expr(struct mscript_program *program, struct expr *expr);
static void compile_float_expr(struct mscript_program *program, struct expr *expr);
static void compile_symbol_expr(struct mscript_program *program, struct expr *expr);
static void compile_string_expr(struct mscript_program *program, struct expr *expr);
static void compile_array_expr(struct mscript_program *program, struct expr *expr);
static void compile_object_expr(struct mscript_program *program, struct expr *expr);
static void compile_cast_expr(struct mscript_program *program, struct expr *expr);

struct vm_array {
    int member_size;
    struct array_char array;
};
array_t(struct vm_array, array_vm_array)

struct vm {
    int ip;
    struct array_int ip_stack;

    int fp;
    struct array_int fp_stack;
    struct array_char stack;

    struct array_vm_array arrays;
};

static void vm_init(struct mscript_program *program);
static void vm_run(struct mscript_program *program, struct array_opcode opcodes);
static void vm_push(struct mscript_program *program, char *v, int n);
static void vm_push_val(struct mscript_program *program, char v, int n);
static char *vm_pop(struct mscript_program *program, int n);

enum expr_type {
    EXPR_UNARY_OP,
    EXPR_BINARY_OP,
    EXPR_CALL,
    EXPR_DEBUG_PRINT,
    EXPR_ARRAY_ACCESS,
    EXPR_MEMBER_ACCESS,
    EXPR_ASSIGNMENT,
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_SYMBOL,
    EXPR_STRING,
    EXPR_ARRAY,
    EXPR_OBJECT,
    EXPR_CAST,
};

enum unary_op_type {
    UNARY_OP_POST_INC,
};

enum binary_op_type {
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_MUL,
    BINARY_OP_DIV,
    BINARY_OP_LTE,
    BINARY_OP_LT,
    BINARY_OP_GTE,
    BINARY_OP_GT,
    BINARY_OP_EQ,
    BINARY_OP_NEQ,
};

struct expr {
    enum expr_type type;
    struct token token;

    union {
        struct {
            enum unary_op_type type;
            struct expr *operand;
        } unary_op;

        struct {
            enum binary_op_type type;
            struct expr *left, *right;
        } binary_op;

        struct {
            struct expr *left, *right;
        } assignment;

        struct {
            struct expr *left, *right;
        } array_access;

        struct {
            struct expr *left;
            char *member_name;
        } member_access;

        struct {
            struct expr *function;
            int num_args;
            struct expr **args;
        } call;

        struct {
            int num_args;
            struct expr **args;
        } debug_print;

        struct {
            int num_args;
            struct expr **args;
        } array;

        struct {
            int num_args;
            char **names;
            struct expr **args;
        } object;

        struct {
            struct mscript_type type;
            struct expr *arg;
        } cast;

        int int_value;
        float float_value;
        char *symbol;
        char *string;
    };

    // set by precompiler
    struct mscript_type result_type;
    struct lvalue lvalue;
};
array_t(struct expr *, array_expr_ptr)

enum stmt_type {
    STMT_IF,
    STMT_RETURN,
    STMT_BLOCK,
    STMT_FUNCTION_DECLARATION,
    STMT_VARIABLE_DECLARATION,
    STMT_STRUCT_DECLARATION,
    STMT_IMPORT,
    STMT_IMPORT_FUNCTION,
    STMT_EXPR,
    STMT_FOR,
};

struct stmt {
    enum stmt_type type;
    struct token token;

    union {
        struct {
            int num_stmts;
            struct expr **conds;
            struct stmt **stmts;
            struct stmt *else_stmt;
        } if_stmt;

        struct {
            struct expr *expr;
        } return_stmt;

        struct {
            int num_stmts;
            struct stmt **stmts;
        } block;

        struct {
            struct token token;
            struct mscript_type return_type;
            char *name;
            int num_args;
            struct mscript_type *arg_types;
            char **arg_names;
            struct stmt *body;

            // set by precompiler
            int block_size;
        } function_declaration;

        struct {
            struct mscript_type type;
            char *name;
            struct expr *assignment_expr;
        } variable_declaration;

        struct {
            char *name;
            int num_members;
            struct mscript_type *member_types;
            char **member_names;
        } struct_declaration;

        struct {
            struct expr *init, *cond, *inc;
            struct stmt *body;
        } for_stmt;

        struct {
            char *program_name;
        } import;

        struct {
            struct mscript_type return_type;
            char *name;
            int num_args;
            struct mscript_type *arg_types;
            char **arg_names;
        } import_function;

        struct expr *expr;
    };
};
array_t(struct stmt *, array_stmt_ptr)

static struct expr *new_unary_op_expr(struct allocator *allocator, struct token token, enum unary_op_type type, struct expr *operand);
static struct expr *new_binary_op_expr(struct allocator *allocator, struct token token, enum binary_op_type type, struct expr *left, struct expr *right);
static struct expr *new_assignment_expr(struct allocator *allocator, struct token token, struct expr *left, struct expr *right);
static struct expr *new_array_access_expr(struct allocator *allocator, struct token token, struct expr *left, struct expr *right);
static struct expr *new_member_access_expr(struct allocator *allocator, struct token token, struct expr *left, char *member_name);
static struct expr *new_call_expr(struct allocator *allocator, struct token token, struct expr *function, struct array_expr_ptr args);
static struct expr *new_debug_print_expr(struct allocator *allocator, struct token token, struct array_expr_ptr args);
static struct expr *new_array_expr(struct allocator *allocator, struct token token, struct array_expr_ptr args);
static struct expr *new_object_expr(struct allocator *allocator, struct token token, struct array_char_ptr names, struct array_expr_ptr args);
static struct expr *new_cast_expr(struct allocator *allocator, struct token token, struct mscript_type type, struct expr *expr);
static struct expr *new_int_expr(struct allocator *allocator, struct token token, int int_value);
static struct expr *new_float_expr(struct allocator *allocator, struct token token, float float_value);
static struct expr *new_symbol_expr(struct allocator *allocator, struct token token, char *symbol);

static struct stmt *new_if_stmt(struct allocator *allocator, struct token token, 
        struct array_expr_ptr conds, struct array_stmt_ptr stmts, struct stmt *else_stmt);
static struct stmt *new_return_stmt(struct allocator *allocator, struct token token, struct expr *expr);
static struct stmt *new_block_stmt(struct allocator *allocator, struct token token, struct array_stmt_ptr stmts);
static struct stmt *new_function_declaration_stmt(struct allocator *allocator, struct token token,
        struct mscript_type return_type, char *name, struct array_mscript_type arg_types, struct array_char_ptr arg_names, struct stmt *body);
static struct stmt *new_variable_declaration_stmt(struct allocator *allocator, struct token token,
        struct mscript_type type, char *name, struct expr *assignment_expr);
static struct stmt *new_struct_declaration_stmt(struct allocator *allocator, struct token token,
        char *name, struct array_mscript_type member_types, struct array_char_ptr member_names);
static struct stmt *new_for_stmt(struct allocator *allocator, struct token token,
        struct expr *init, struct expr *cond, struct expr *inc, struct stmt *body);
static struct stmt *new_import_stmt(struct allocator *allocator, struct token token, char *program_name);
static struct stmt *new_import_function_stmt(struct allocator *allocator, struct token token, struct mscript_type return_type, char *name, struct array_mscript_type arg_types, struct array_char_ptr arg_names);
static struct stmt *new_expr_stmt(struct allocator *allocator, struct token token, struct expr *expr);

static void parse_type(struct mscript_program *program, struct mscript_type *type);  

static struct expr *parse_expr(struct mscript_program *program);
static struct expr *parse_assignment_expr(struct mscript_program *program);
static struct expr *parse_comparison_expr(struct mscript_program *program);
static struct expr *parse_term_expr(struct mscript_program *program);
static struct expr *parse_factor_expr(struct mscript_program *program);
static struct expr *parse_unary_expr(struct mscript_program *program);
static struct expr *parse_member_access_or_array_access_expr(struct mscript_program *program);
static struct expr *parse_call_expr(struct mscript_program *program);
static struct expr *parse_primary_expr(struct mscript_program *program);
static struct expr *parse_array_expr(struct mscript_program *program);
static struct expr *parse_object_expr(struct mscript_program *program);

static struct stmt *parse_stmt(struct mscript_program *program);
static struct stmt *parse_if_stmt(struct mscript_program *program);
static struct stmt *parse_block_stmt(struct mscript_program *program);
static struct stmt *parse_for_stmt(struct mscript_program *program);
static struct stmt *parse_return_stmt(struct mscript_program *program);
static struct stmt *parse_variable_declaration_stmt(struct mscript_program *program);
static struct stmt *parse_function_declaration_stmt(struct mscript_program *program);
static struct stmt *parse_struct_declaration_stmt(struct mscript_program *program);
static struct stmt *parse_import_stmt(struct mscript_program *program);
static struct stmt *parse_import_function_stmt(struct mscript_program *program);

static void debug_log_token(struct token token);
static void debug_log_tokens(struct token *tokens);
static void debug_log_type(struct mscript_type type);
static void debug_log_stmt(struct stmt *stmt);
static void debug_log_expr(struct expr *expr);
static void debug_log_opcodes(struct opcode *opcodes, int num_opcodes);

struct function_decl_arg {
    struct mscript_type type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
};

struct function_decl {
    struct mscript_type return_type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1]; 
    int num_args;
    struct function_decl_arg args[MSCRIPT_MAX_FUNCTION_ARGS];
};

struct struct_decl_arg {
    struct mscript_type type;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    int offset;
};

struct struct_decl {
    int size;
    char name[MSCRIPT_MAX_SYMBOL_LEN + 1];
    int num_members;
    struct struct_decl_arg members[MSCRIPT_MAX_STRUCT_MEMBERS];
};

typedef map_t(struct function_decl) map_function_decl_t;
typedef map_t(struct struct_decl) map_struct_decl_t;
array_t(struct mscript_program *, array_program_ptr)

struct mscript_program {
    struct array_program_ptr imported_programs_array;
    map_struct_decl_t struct_decl_map;
    map_function_decl_t function_decl_map;

    struct parser parser;
    struct pre_compiler pre_compiler;
    struct compiler compiler; 
    struct vm vm;

    char *error;
    struct token error_token;
};

typedef map_t(struct mscript_program *) map_mscript_program_ptr_t;

struct mscript {
    map_mscript_program_ptr_t map;
};

static void program_init(struct mscript_program *program, struct mscript *mscript, const char *prog_text);
static void program_add_struct_decl(struct mscript_program *program, struct stmt *stmt);
static struct struct_decl *program_get_struct_decl(struct mscript_program *program, const char *name);
static bool program_get_struct_decl_member(struct struct_decl *decl, const char *member, struct mscript_type *type, int *offset);
static void program_add_function_decl(struct mscript_program *program, struct stmt *stmt);
static struct function_decl *program_get_function_decl(struct mscript_program *program, const char *name);
static void program_error(struct mscript_program *program, struct token token, char *fmt, ...);

//
// DEFINITIONS
//

static struct mscript_type void_type(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_VOID;
    type.struct_name = "";
    return type;
}

static struct mscript_type void_star_type(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_VOID_STAR;
    type.struct_name = "";
    return type;
}

static struct mscript_type int_type(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_INT;
    type.struct_name = "";
    return type;
}

static struct mscript_type float_type(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_FLOAT;
    type.struct_name = "";
    return type;
}

static struct mscript_type struct_type(char *struct_name) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_STRUCT;
    type.struct_name = struct_name;
    return type;
}

static struct mscript_type array_type(enum mscript_type_type array_type, char *struct_name) {
    assert(array_type != MSCRIPT_TYPE_ARRAY);

    struct mscript_type type;
    type.type = MSCRIPT_TYPE_ARRAY;
    type.array_type = array_type;
    type.struct_name = struct_name;
    return type;
}

static struct mscript_type string_type(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_STRING;
    type.struct_name = "";
    return type;
}

static bool types_equal(struct mscript_type a, struct mscript_type b) {
    if (a.type != b.type) {
        return false;
    }

    if (a.type == MSCRIPT_TYPE_STRUCT) {
        return strcmp(a.struct_name, b.struct_name) == 0;
    }
    else if (a.type == MSCRIPT_TYPE_ARRAY) {
        return (a.array_type == b.array_type) && (strcmp(a.struct_name, b.struct_name) == 0);
    }
    else {
        return true;
    }
}

static void type_to_string(struct mscript_type type, char *buffer, int buffer_len) {
    enum mscript_type_type t = type.type;
    if (t == MSCRIPT_TYPE_ARRAY) {
        t = type.array_type;
    }
    
    buffer[buffer_len - 1] = 0;
    switch (t) {
        case MSCRIPT_TYPE_VOID:
            {
                strncpy(buffer, "void", buffer_len - 1);
                break;
            }
        case MSCRIPT_TYPE_VOID_STAR:
            {
                strncpy(buffer, "void*", buffer_len - 1);
                break;
            }
        case MSCRIPT_TYPE_INT:
            {
                strncpy(buffer, "int", buffer_len - 1);
                break;
            }
        case MSCRIPT_TYPE_FLOAT:
            {
                strncpy(buffer, "float", buffer_len - 1);
                break;
            }
        case MSCRIPT_TYPE_STRUCT:
            {
                strncpy(buffer, type.struct_name, buffer_len - 1);
                break;
            }
        case MSCRIPT_TYPE_STRING:
            {
                strncpy(buffer, "char*", buffer_len - 1);
                break;
            }
        case MSCRIPT_TYPE_ARRAY:
            {
                assert(false);
                break;
            }
    }

    if (type.type == MSCRIPT_TYPE_ARRAY) {
        int len = (int)strlen(buffer);
        strncat(buffer, "[]", buffer_len - len - 1);
    }
}

static int type_size(struct mscript_program *program, struct mscript_type type) {
    switch (type.type) {
        case MSCRIPT_TYPE_VOID:
            return 0;
            break;
        case MSCRIPT_TYPE_VOID_STAR:
            return 4;
            break;
        case MSCRIPT_TYPE_INT:
            return 4;
            break;
        case MSCRIPT_TYPE_FLOAT:
            return 4;
            break;
        case MSCRIPT_TYPE_STRUCT: 
            {
                struct struct_decl *decl = program_get_struct_decl(program, type.struct_name);
                assert(decl);
                return decl->size;
            }
            break;
        case MSCRIPT_TYPE_ARRAY:
            return 4;
            break;
        case MSCRIPT_TYPE_STRING:
            return 4;
            break;
    }

    assert(false);
    return 0;
}

struct lvalue lvalue_invalid(void) {
    struct lvalue lvalue;
    lvalue.type = LVALUE_INVALID;
    return lvalue;
}

struct lvalue lvalue_local(int offset) {
    struct lvalue lvalue;
    lvalue.type = LVALUE_LOCAL;
    lvalue.offset = offset;
    return lvalue;
}

struct lvalue lvalue_array(void) {
    struct lvalue lvalue;
    lvalue.type = LVALUE_ARRAY;
    return lvalue;
}

static struct expr *new_unary_op_expr(struct allocator *allocator, struct token token, enum unary_op_type type, struct expr *operand) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_UNARY_OP;
    expr->token = token;
    expr->unary_op.type = type;
    expr->unary_op.operand = operand;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_binary_op_expr(struct allocator *allocator, struct token token, enum binary_op_type type, struct expr *left, struct expr *right) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_BINARY_OP;
    expr->token = token;
    expr->binary_op.type = type;
    expr->binary_op.left = left;
    expr->binary_op.right = right;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_assignment_expr(struct allocator *allocator, struct token token, struct expr *left, struct expr *right) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_ASSIGNMENT;
    expr->token = token;
    expr->assignment.left = left;
    expr->assignment.right = right;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_array_access_expr(struct allocator *allocator, struct token token, struct expr *left, struct expr *right) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_ARRAY_ACCESS;
    expr->token = token;
    expr->array_access.left = left;
    expr->array_access.right = right;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_member_access_expr(struct allocator *allocator, struct token token, struct expr *left, char *member_name) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_MEMBER_ACCESS;
    expr->token = token;
    expr->member_access.left = left;
    expr->member_access.member_name = member_name;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_call_expr(struct allocator *allocator, struct token token, struct expr *function, struct array_expr_ptr args) {
    int num_args = args.length;

    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_CALL;
    expr->token = token;
    expr->call.function = function;
    expr->call.num_args = num_args;
    expr->call.args = allocator_alloc(allocator, num_args * sizeof(struct expr*));
    memcpy(expr->call.args, args.data, num_args * sizeof(struct expr*));

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_debug_print_expr(struct allocator *allocator, struct token token, struct array_expr_ptr args) {
    int num_args = args.length;

    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_DEBUG_PRINT;
    expr->token = token;
    expr->debug_print.num_args = num_args;
    expr->debug_print.args = allocator_alloc(allocator, num_args * sizeof(struct expr*));
    memcpy(expr->debug_print.args, args.data, num_args * sizeof(struct expr*));

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_array_expr(struct allocator *allocator, struct token token, struct array_expr_ptr args) {
    int num_args = args.length;

    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_ARRAY;
    expr->token = token;
    expr->array.num_args = num_args;
    expr->array.args = allocator_alloc(allocator, num_args * sizeof(struct expr*));
    memcpy(expr->array.args, args.data, num_args * sizeof(struct expr*));

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_object_expr(struct allocator *allocator, struct token token, struct array_char_ptr names, struct array_expr_ptr args) {
    assert(names.length == args.length);
    int num_args = args.length;

    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_OBJECT;
    expr->token = token;
    expr->object.num_args = num_args;
    expr->object.names = allocator_alloc(allocator, num_args * sizeof(char *));
    memcpy(expr->object.names, names.data, num_args * sizeof(char *));
    expr->object.args = allocator_alloc(allocator, num_args * sizeof(struct expr *));
    memcpy(expr->object.args, args.data, num_args * sizeof(struct expr *));

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_cast_expr(struct allocator *allocator, struct token token, struct mscript_type type, struct expr *arg) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_CAST;
    expr->token = token;
    expr->cast.type = type;
    expr->cast.arg = arg;

    expr->result_type = type;
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_int_expr(struct allocator *allocator, struct token token, int int_value) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_INT;
    expr->token = token;
    expr->int_value = int_value;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_float_expr(struct allocator *allocator, struct token token, float float_value) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_FLOAT;
    expr->token = token;
    expr->float_value = float_value;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_symbol_expr(struct allocator *allocator, struct token token, char *symbol) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_SYMBOL;
    expr->token = token;
    expr->symbol = symbol;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct expr *new_string_expr(struct allocator *allocator, struct token token, char *string) {
    struct expr *expr = allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_STRING;
    expr->token = token;
    expr->string = string;

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
    return expr;
}

static struct stmt *new_if_stmt(struct allocator *allocator, struct token token, struct array_expr_ptr conds, struct array_stmt_ptr stmts, struct stmt *else_stmt) {
    assert(conds.length == stmts.length);
    int num_stmts = conds.length;

    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_IF;
    stmt->token = token;
    stmt->if_stmt.num_stmts = num_stmts;
    stmt->if_stmt.conds = allocator_alloc(allocator, num_stmts * sizeof(struct expr *));
    memcpy(stmt->if_stmt.conds, conds.data, num_stmts * sizeof(struct expr *));
    stmt->if_stmt.stmts = allocator_alloc(allocator, num_stmts * sizeof(struct stmt *));
    memcpy(stmt->if_stmt.stmts, stmts.data, num_stmts * sizeof(struct stmt *));
    stmt->if_stmt.else_stmt = else_stmt;
    return stmt;
}

static struct stmt *new_return_stmt(struct allocator *allocator, struct token token, struct expr *expr) {
    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_RETURN;
    stmt->token = token;
    stmt->return_stmt.expr = expr;
    return stmt;
}

static struct stmt *new_block_stmt(struct allocator *allocator, struct token token, struct array_stmt_ptr stmts) {
    int num_stmts = stmts.length;

    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_BLOCK;
    stmt->token = token;
    stmt->block.num_stmts = num_stmts;
    stmt->block.stmts = allocator_alloc(allocator, num_stmts * sizeof(struct stmt *));
    memcpy(stmt->block.stmts, stmts.data, num_stmts * sizeof(struct stmt *));
    return stmt;
}

static struct stmt *new_function_declaration_stmt(struct allocator *allocator, struct token token, struct mscript_type return_type, char *name, 
        struct array_mscript_type arg_types, struct array_char_ptr arg_names, struct stmt *body) {
    assert(arg_types.length == arg_names.length);
    int num_args = arg_types.length;

    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_FUNCTION_DECLARATION;
    stmt->token = token;
    stmt->function_declaration.token = token;
    stmt->function_declaration.return_type = return_type;
    stmt->function_declaration.name = name;
    stmt->function_declaration.num_args = num_args;
    stmt->function_declaration.arg_types = allocator_alloc(allocator, num_args * sizeof(struct mscript_type));
    memcpy(stmt->function_declaration.arg_types, arg_types.data, num_args * sizeof(struct mscript_type));
    stmt->function_declaration.arg_names = allocator_alloc(allocator, num_args * sizeof(char *));
    memcpy(stmt->function_declaration.arg_names, arg_names.data, num_args * sizeof(char *));
    stmt->function_declaration.body = body;
    stmt->function_declaration.block_size = -1;
    return stmt;
}

static struct stmt *new_variable_declaration_stmt(struct allocator *allocator, struct token token, struct mscript_type type, char *name, struct expr *assignment_expr) {
    if (assignment_expr) {
        assert(assignment_expr->type == EXPR_ASSIGNMENT);
    }

    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_VARIABLE_DECLARATION;
    stmt->token = token;
    stmt->variable_declaration.type = type;
    stmt->variable_declaration.name = name;
    stmt->variable_declaration.assignment_expr = assignment_expr;
    return stmt;
}

static struct stmt *new_struct_declaration_stmt(struct allocator *allocator, struct token token, char *name, 
        struct array_mscript_type member_types, struct array_char_ptr member_names) {
    assert(member_types.length == member_names.length);
    int num_members = member_types.length;

    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_STRUCT_DECLARATION;
    stmt->token = token;
    stmt->struct_declaration.name = name;
    stmt->struct_declaration.num_members = num_members;
    stmt->struct_declaration.member_types = allocator_alloc(allocator, num_members * sizeof(struct mscript_type));
    memcpy(stmt->struct_declaration.member_types, member_types.data, num_members * sizeof(struct mscript_type));
    stmt->struct_declaration.member_names = allocator_alloc(allocator, num_members * sizeof(char *));
    memcpy(stmt->struct_declaration.member_names, member_names.data, num_members * sizeof(char *));
    return stmt;
}

static struct stmt *new_for_stmt(struct allocator *allocator, struct token token, struct expr *init, struct expr *cond, struct expr *inc, struct stmt *body) {
    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_FOR;
    stmt->token = token;
    stmt->for_stmt.init = init;
    stmt->for_stmt.cond = cond;
    stmt->for_stmt.inc = inc;
    stmt->for_stmt.body = body;
    return stmt;
}

static struct stmt *new_import_stmt(struct allocator *allocator, struct token token, char *program_name) {
    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_IMPORT;
    stmt->token = token;
    stmt->import.program_name = program_name;
    return stmt;
}

static struct stmt *new_import_function_stmt(struct allocator *allocator, struct token token, struct mscript_type return_type, char *name, struct array_mscript_type arg_types, struct array_char_ptr arg_names) {
    int num_args = arg_types.length;
    assert(arg_types.length == arg_names.length);

    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_IMPORT_FUNCTION;
    stmt->token = token;
    stmt->import_function.return_type = return_type;
    stmt->import_function.name = name;
    stmt->import_function.num_args = num_args;
    stmt->import_function.arg_types = allocator_alloc(allocator, num_args * sizeof(struct mscript_type));
    memcpy(stmt->import_function.arg_types, arg_types.data, num_args * sizeof(struct mscript_type));
    stmt->import_function.arg_names = allocator_alloc(allocator, num_args * sizeof(char *));
    memcpy(stmt->import_function.arg_names, arg_names.data, num_args * sizeof(char *));
    return stmt;
}

static struct stmt *new_expr_stmt(struct allocator *allocator, struct token token, struct expr *expr) {
    struct stmt *stmt = allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_EXPR;
    stmt->token = token;
    stmt->expr = expr;
    return stmt;
}

static void parse_type(struct mscript_program *program, struct mscript_type *type) {
    if (match_symbol(program, "void")) {
        if (match_char(program, '*')) {
            *type = void_star_type();
        }
        else {
            *type = void_type();
        }
    }
    else if (match_symbol(program, "int")) {
        *type = int_type();
    }
    else if (match_symbol(program, "float")) {
        *type = float_type();
    }
    else {
        struct token tok = peek(program);
        if (tok.type != TOKEN_SYMBOL) {
            program_error(program, tok, "Expected symbol");
            return;
        }
        eat(program);

        *type = struct_type(tok.symbol);
    }

    if (match_char_n(program, 2, '[', ']')) {
        *type = array_type(type->type, type->struct_name);
    }
}

static struct expr *parse_object_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct array_char_ptr names;
    struct array_expr_ptr args;
    array_init(&names);
    array_init(&args);

    struct expr *expr = NULL;

    if (!match_char(program, '}')) {
        while (true) {
            struct token tok = peek(program);
            if (tok.type != TOKEN_SYMBOL) {
                program_error(program, tok, "Expected symbol"); 
                goto cleanup;
            }
            array_push(&names, tok.symbol);
            eat(program);

            if (!match_char(program, '=')) {
                program_error(program, peek(program), "Expected '='");
                goto cleanup;
            }

            struct expr *arg = parse_expr(program);
            if (program->error) goto cleanup;
            array_push(&args, arg);

            if (!match_char(program, ',')) {
                if (!match_char(program, '}')) {
                    program_error(program, peek(program), "Expected '}'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = new_object_expr(&program->parser.allocator, token, names, args);

cleanup:
    array_deinit(&names);
    array_deinit(&args);
    return expr;
}

static struct expr *parse_array_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct array_expr_ptr args;
    array_init(&args);

    struct expr *expr = NULL;
    if (!match_char(program, ']')) {
        while (true) {
            struct expr *arg = parse_expr(program);
            if (program->error) goto cleanup;
            array_push(&args, arg);

            if (!match_char(program, ',')) {
                if (!match_char(program, ']')) {
                    program_error(program, peek(program), "Expected ']'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = new_array_expr(&program->parser.allocator, token, args);

cleanup:
    array_deinit(&args);
    return expr;
}

static struct expr *parse_primary_expr(struct mscript_program *program) {
    struct token tok = peek(program);
    struct expr *expr = NULL;

    if (tok.type == TOKEN_INT) {
        expr = new_int_expr(&program->parser.allocator, tok, tok.int_value);
        eat(program);
    }
    else if (tok.type == TOKEN_FLOAT) {
        expr = new_float_expr(&program->parser.allocator, tok, tok.float_value);
        eat(program);
    }
    else if (tok.type == TOKEN_SYMBOL) {
        expr = new_symbol_expr(&program->parser.allocator, tok, tok.symbol);
        eat(program);
    }
    else if (tok.type == TOKEN_STRING) {
        expr = new_string_expr(&program->parser.allocator, tok, tok.string);
        eat(program);
    }
    else if (match_char(program, '[')) {
        expr = parse_array_expr(program);
    }
    else if (match_char(program, '{')) {
        expr = parse_object_expr(program);
    }
    else if (match_char(program, '(')) {
        expr = parse_expr(program);
        if (!match_char(program, ')')) {
            program_error(program, peek(program), "Expected ')'."); 
            goto cleanup;
        }
    }
    else {
        program_error(program, tok, "Unknown token.");
        goto cleanup;
    }

cleanup:
    return expr;
}

static struct expr *parse_call_expr(struct mscript_program *program) {
    struct array_expr_ptr args;
    array_init(&args);

    struct token token = peek(program);
    struct expr *expr = parse_primary_expr(program);
    if (program->error) goto cleanup;

    if (match_char(program, '(')) {
        if (!match_char(program, ')')) {
            while (true) {
                struct expr *arg = parse_expr(program);
                if (program->error) goto cleanup;
                array_push(&args, arg);

                if (!match_char(program, ',')) {
                    if (!match_char(program, ')')) {
                        program_error(program, peek(program), "Expected ')'");
                        goto cleanup;
                    }
                    break;
                }
            }
        }

        if (expr->type == EXPR_SYMBOL && (strcmp(expr->symbol, "debug_print") == 0)) {
            expr = new_debug_print_expr(&program->parser.allocator, token, args);
        }
        else {
            expr = new_call_expr(&program->parser.allocator, token, expr, args);
        }
    }

cleanup:
    array_deinit(&args);
    return expr;
}

static struct expr *parse_member_access_or_array_access_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct expr *expr = parse_call_expr(program);
    if (program->error) goto cleanup;

    while (true) {
        if (match_char(program, '.')) {
            struct token tok = peek(program);
            if (tok.type != TOKEN_SYMBOL) {
                program_error(program, tok, "Expected symbol token");
                goto cleanup;
            }
            eat(program);

            expr = new_member_access_expr(&program->parser.allocator, token, expr, tok.symbol);
        }
        else if (match_char(program, '[')) {
            struct expr *right = parse_expr(program);
            if (program->error) goto cleanup;
            expr = new_array_access_expr(&program->parser.allocator, token, expr, right);

            if (!match_char(program, ']')) {
                program_error(program, peek(program), "Expected ']'");
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

static struct expr *parse_unary_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct expr *expr = parse_member_access_or_array_access_expr(program);
    if (program->error) goto cleanup;

    if (match_char_n(program, 2, '+', '+')) {
        expr = new_unary_op_expr(&program->parser.allocator, token, UNARY_OP_POST_INC, expr);
    }

cleanup:
    return expr;
}

static struct expr *parse_factor_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct expr *expr = parse_unary_expr(program);
    if (program->error) goto cleanup;

    while (true) {
        enum binary_op_type binary_op_type;

        if (match_char(program, '*')) {
            binary_op_type = BINARY_OP_MUL;
        }
        else if (match_char(program, '/')) {
            binary_op_type = BINARY_OP_DIV;
        }
        else {
            break;
        }

        struct expr *right = parse_unary_expr(program);
        if (program->error) goto cleanup;
        expr = new_binary_op_expr(&program->parser.allocator, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static struct expr *parse_term_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct expr *expr = parse_factor_expr(program);
    if (program->error) goto cleanup;

    while (true) {
        enum binary_op_type binary_op_type;

        if (match_char(program, '+')) {
            binary_op_type = BINARY_OP_ADD;
        }
        else if (match_char(program, '-')) {
            binary_op_type = BINARY_OP_SUB;
        }
        else {
            break;
        }

        struct expr *right = parse_factor_expr(program);
        if (program->error) goto cleanup;
        expr = new_binary_op_expr(&program->parser.allocator, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static struct expr *parse_comparison_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct expr *expr = parse_term_expr(program);
    if (program->error) goto cleanup;

    while (true) {
        enum binary_op_type binary_op_type;

        if (match_char_n(program, 2, '<', '=')) {
            binary_op_type = BINARY_OP_LTE;
        }
        else if (match_char_n(program, 1, '<')) {
            binary_op_type = BINARY_OP_LT;
        }
        else if (match_char_n(program, 2, '>', '=')) {
            binary_op_type = BINARY_OP_GTE;
        }
        else if (match_char_n(program, 1, '>')) {
            binary_op_type = BINARY_OP_GT;
        }
        else if (match_char_n(program, 2, '=', '=')) {
            binary_op_type = BINARY_OP_EQ;
        }
        else if (match_char_n(program, 2, '!', '=')) {
            binary_op_type = BINARY_OP_NEQ;
        }
        else {
            break;
        }

        struct expr *right = parse_term_expr(program);
        if (program->error) goto cleanup;
        expr = new_binary_op_expr(&program->parser.allocator, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static struct expr *parse_assignment_expr(struct mscript_program *program) {
    struct token token = peek(program);
    struct expr *expr = parse_comparison_expr(program);
    if (program->error) goto cleanup;

    while (true) {
        if (match_char(program, '=')) {
            struct expr *right = parse_assignment_expr(program);
            if (program->error) goto cleanup;

            expr = new_assignment_expr(&program->parser.allocator, token, expr, right);
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static struct expr *parse_expr(struct mscript_program *program) {
    struct expr *expr = parse_assignment_expr(program);
    return expr;
}

static struct stmt *parse_stmt(struct mscript_program *program) {
    if (match_symbol(program, "if")) {
        return parse_if_stmt(program);
    }
    else if (match_symbol(program, "for")) {
        return parse_for_stmt(program);
    }
    else if (match_symbol(program, "return")) {
        return parse_return_stmt(program);
    }
    else if (check_type(program)) {
        return parse_variable_declaration_stmt(program);
    }
    else if (match_char(program, '{')) {
        return parse_block_stmt(program);
    }
    else {
        struct token token = peek(program);
        struct expr *expr = parse_expr(program);
        if (program->error) return NULL;

        if (!match_char(program, ';')) {
            program_error(program, peek(program), "Expected ';'");
            return NULL;
        }
        return new_expr_stmt(&program->parser.allocator, token, expr);
    }
}

static struct stmt *parse_if_stmt(struct mscript_program *program) {
    struct array_expr_ptr conds;
    struct array_stmt_ptr stmts;
    struct stmt *else_stmt = NULL;
    array_init(&conds);
    array_init(&stmts);

    struct stmt *stmt = NULL;
    struct token token = peek(program);

    if (!match_char(program, '(')) {
        program_error(program, peek(program), "Expected '('");
        goto cleanup;
    }

    {
        struct expr *cond = parse_expr(program);
        if (program->error) goto cleanup;

        if (!match_char(program, ')')) {
            program_error(program, peek(program), "Expected ')'");
            goto cleanup;
        }

        struct stmt *stmt = parse_stmt(program);
        if (program->error) goto cleanup;

        array_push(&conds, cond);
        array_push(&stmts, stmt);
    }

    while (true) {
        if (match_symbol_n(program, 2, "else", "if")) {
            if (!match_char(program, '(')) {
                program_error(program, peek(program), "Expected '('");
                goto cleanup;
            }

            struct expr *cond = parse_expr(program);
            if (program->error) goto cleanup;

            if (!match_char(program, ')')) {
                program_error(program, peek(program), "Expected ')'");
                goto cleanup;
            }

            struct stmt *stmt = parse_stmt(program);
            if (program->error) goto cleanup;

            array_push(&conds, cond);
            array_push(&stmts, stmt);
        }
        else if (match_symbol(program, "else")) {
            else_stmt = parse_stmt(program);
            if (program->error) goto cleanup;
            break;
        }
        else {
            break;
        }
    }

    stmt = new_if_stmt(&program->parser.allocator, token, conds, stmts, else_stmt);

cleanup:
    array_deinit(&conds);
    array_deinit(&stmts);
    return stmt;
}

static struct stmt *parse_block_stmt(struct mscript_program *program) {
    struct array_stmt_ptr stmts;
    array_init(&stmts);

    struct stmt *stmt = NULL;
    struct token token = peek(program);
    
    while (true) {
        if (match_char(program, '}')) {
            break;
        }

        struct stmt *stmt = parse_stmt(program);
        if (program->error) goto cleanup;
        array_push(&stmts, stmt);
    }

    stmt = new_block_stmt(&program->parser.allocator, token, stmts);

cleanup:
    array_deinit(&stmts);
    return stmt;
}

static struct stmt *parse_for_stmt(struct mscript_program *program) {
    struct token token = peek(program);
    struct stmt *stmt = NULL;

    if (!match_char(program, '(')) {
        program_error(program, peek(program), "Expected '('");
        goto cleanup;
    }

    struct expr *init = parse_expr(program);
    if (program->error) goto cleanup;
    if (!match_char(program, ';')) {
        program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    struct expr *cond = parse_expr(program);
    if (program->error) goto cleanup;
    if (!match_char(program, ';')) {
        program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    struct expr *inc = parse_expr(program);
    if (program->error) goto cleanup;
    if (!match_char(program, ')')) {
        program_error(program, peek(program), "Expected ')'");
        goto cleanup;
    }

    struct stmt *body = parse_stmt(program);
    if (program->error) goto cleanup;

    stmt = new_for_stmt(&program->parser.allocator, token, init, cond, inc, body);

cleanup:
    return stmt;
}

static struct stmt *parse_return_stmt(struct mscript_program *program) {
    struct token token = peek(program);
    struct stmt *stmt = NULL;
    struct expr *expr = NULL;

    if (!match_char(program, ';')) {
        expr = parse_expr(program);
        if (program->error) goto cleanup;
        if (!match_char(program, ';')) {
            program_error(program, peek(program), "Expected ';'");
            goto cleanup;
        }
    }

    stmt = new_return_stmt(&program->parser.allocator, token, expr);

cleanup:
    return stmt;
}

static struct stmt *parse_variable_declaration_stmt(struct mscript_program *program) {
    struct token token = peek(program);
    struct stmt *stmt = NULL;

    struct mscript_type type;
    parse_type(program, &type);
    if (program->error) goto cleanup;

    struct token name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    struct expr *assignment_expr = NULL;
    if (match_char(program, '=')) {
        struct expr *left = new_symbol_expr(&program->parser.allocator, token, name.symbol);
        struct expr *right = parse_expr(program);
        if (program->error) goto cleanup;

        assignment_expr = new_assignment_expr(&program->parser.allocator, token, left, right);
    }

    if (!match_char(program, ';')) {
        program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    stmt = new_variable_declaration_stmt(&program->parser.allocator, token, type, name.symbol, assignment_expr);

cleanup:
    return stmt;
}

static struct stmt *parse_function_declaration_stmt(struct mscript_program *program) {
    struct array_mscript_type arg_types;
    struct array_char_ptr arg_names;
    array_init(&arg_types);
    array_init(&arg_names);

    struct stmt *stmt = NULL;

    struct mscript_type return_type;
    parse_type(program, &return_type);
    if (program->error) goto cleanup;

    struct token name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '(')) {
        program_error(program, peek(program), "Expected '('");
        goto cleanup;
    }

    if (!match_char(program, ')')) {
        while (true) {
            struct mscript_type arg_type;
            parse_type(program, &arg_type);
            if (program->error) goto cleanup;

            struct token arg_name = peek(program);
            if (arg_name.type != TOKEN_SYMBOL) {
                program_error(program, arg_name, "Expected symbol");
                goto cleanup;
            }
            eat(program);

            array_push(&arg_types, arg_type);
            array_push(&arg_names, arg_name.symbol);

            if (!match_char(program, ',')) {
                if (!match_char(program, ')')) {
                    program_error(program, peek(program), "Expected ')'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    if (!match_char(program, '{')) {
        program_error(program, peek(program), "Expected '{'");
        goto cleanup;
    }

    struct stmt *body_stmt = parse_block_stmt(program);
    if (program->error) goto cleanup;

    stmt = new_function_declaration_stmt(&program->parser.allocator, name, return_type, name.symbol, arg_types, arg_names, body_stmt);

cleanup:
    array_deinit(&arg_types);
    array_deinit(&arg_names);
    return stmt;
}

static struct stmt *parse_struct_declaration_stmt(struct mscript_program *program) {
    struct token token = peek(program);
    struct array_mscript_type member_types;
    struct array_char_ptr member_names;
    array_init(&member_types);
    array_init(&member_names);

    struct stmt *stmt = NULL;

    struct token name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        program_error(program, name, "Expected symbol");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '{')) {
        program_error(program, peek(program), "Expected '{'");
        goto cleanup;
    }

    while (true) {
        if (match_char(program, '}')) {
            break;
        }

        struct mscript_type member_type;
        parse_type(program, &member_type);
        if (program->error) goto cleanup;

        while (true) {
            struct token member_name = peek(program);
            if (member_name.type != TOKEN_SYMBOL) {
                program_error(program, member_name, "Expected symbol");
                goto cleanup;
            }
            eat(program);

            array_push(&member_types, member_type);
            array_push(&member_names, member_name.symbol);

            if (!match_char(program, ',')) {
                break;
            }
        }

        if (!match_char(program, ';')) {
            program_error(program, peek(program), "Expected ';'");
            goto cleanup;
        }
    }

    stmt = new_struct_declaration_stmt(&program->parser.allocator, token, name.symbol, member_types, member_names);

cleanup:
    array_deinit(&member_types);
    array_deinit(&member_names);
    return stmt;
}

static struct stmt *parse_import_stmt(struct mscript_program *program) {
    struct token token = peek(program);
    struct stmt *stmt = NULL;

    struct token program_name = peek(program);
    if (program_name.type != TOKEN_STRING) {
        program_error(program, program_name, "Expected string");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, ';')) {
        program_error(program, peek(program), "Expected ';'");
        goto cleanup;
    }

    stmt = new_import_stmt(&program->parser.allocator, token, program_name.symbol);

cleanup:
    return stmt;
}

static struct stmt *parse_import_function_stmt(struct mscript_program *program) {
    struct stmt *stmt = NULL;

    struct array_mscript_type arg_types;
    struct array_char_ptr arg_names;
    array_init(&arg_types);
    array_init(&arg_names);

    struct mscript_type return_type;
    parse_type(program, &return_type);
    if (program->error) goto cleanup;

    struct token name = peek(program);
    if (name.type != TOKEN_SYMBOL) {
        program_error(program, name, "Expected a symbol.");
        goto cleanup;
    }
    eat(program);

    if (!match_char(program, '(')) {
        program_error(program, name, "Expected '('.");
        goto cleanup;
    }

    while (true) {
        struct mscript_type arg_type;
        parse_type(program, &arg_type);
        if (program->error) goto cleanup;

        struct token arg_name = peek(program);
        if (arg_name.type != TOKEN_SYMBOL) {
            program_error(program, name, "Expected a symbol.");
            goto cleanup;
        }
        eat(program);

        array_push(&arg_types, arg_type);
        array_push(&arg_names, arg_name.symbol);

        if (!match_char(program, ',')) {
            if (!match_char(program, ')')) {
                program_error(program, arg_name, "Expected ')'.");
                goto cleanup;
            }
            break;
        }
    }

    if (!match_char(program, ';')) {
        program_error(program, name, "Expected ';'.");
        goto cleanup;
    }

    stmt = new_import_function_stmt(&program->parser.allocator, name, return_type, name.symbol, arg_types, arg_names); 

cleanup:
    return stmt;
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

static struct token number_token(const char *text, int *len, int line, int col) {
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
        struct token token;
        token.type = TOKEN_FLOAT;
        token.float_value = (float)int_part + float_part;
        token.line = line;
        token.col = col;
        return token;
    }
    else {
        struct token token;
        token.type = TOKEN_INT;
        token.int_value = int_part;
        token.line = line;
        token.col = col;
        return token;
    }
}

static struct token char_token(char c, int line, int col) {
    struct token token;
    token.type = TOKEN_CHAR;
    token.char_value = c;
    token.line = line;
    token.col = col;
    return token;
}

static struct token string_token(struct mscript_program *program, const char *text, int *len, int line, int col) {
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
                struct token token;
                token.type = TOKEN_CHAR;
                token.char_value = text[i + 1];
                token.line = line;
                token.col = col;
                program_error(program, token, "Invalid escape character %c", token.char_value);
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

    struct token token;
    token.type = TOKEN_STRING;
    token.string = string;
    token.line = line;
    token.col = col;
    return token;
}

static struct token symbol_token(const char *text, int *len, int line, int col) {
    *len = 0;
    while (is_char_part_of_symbol(text[*len])) {
        (*len)++;
    }

    char *symbol = malloc((*len) + 1);
    for (int i = 0; i < *len; i++) {
        symbol[i] = text[i];
    }
    symbol[*len] = 0;

    struct token token;
    token.type = TOKEN_SYMBOL;
    token.symbol = symbol;
    token.line = line;
    token.col = col;
    return token;
}

static struct token eof_token(int line, int col) {
    struct token token;
    token.type = TOKEN_EOF;
    token.line = line;
    token.col = col;
    return token;
}

static void tokenize(struct mscript_program *program) {
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
            array_push(&parser->tokens, eof_token(line, col));
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
            array_push(&parser->tokens, string_token(program, prog + i, &len, line, col));
            if (program->error) return;
            i += (len + 1);
            col += (len + 1);
        }
        else if (is_char_start_of_symbol(prog[i])) {
            int len = 0;
            array_push(&parser->tokens, symbol_token(prog + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (is_char_digit(prog[i])) {
            int len = 0;
            array_push(&parser->tokens, number_token(prog + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (is_char(prog[i])) {
            array_push(&parser->tokens, char_token(prog[i], line, col));
            i++;
            col++;
        }
        else {
            struct token tok = char_token(prog[i], line, col);
            program_error(program, tok, "Unknown character: %c", prog[i]);
            return;
        }
    }
}

static void parser_run(struct mscript_program *program, struct mscript *mscript) {
    struct array_stmt_ptr global_stmts;
    array_init(&global_stmts);

    tokenize(program);
    if (program->error) goto cleanup;
    //debug_log_tokens(program->parser.tokens.data);

    while (true) {
        if (match_eof(program)) {
            break;
        }

        struct stmt *stmt;
        if (match_symbol(program, "import")) {
            stmt = parse_import_stmt(program);
            if (program->error) goto cleanup;
        }
        else if (match_symbol(program, "import_function")) {
            stmt = parse_import_function_stmt(program);
            if (program->error) goto cleanup;
        }
        else if (match_symbol(program, "struct")) {
            stmt = parse_struct_declaration_stmt(program);
            if (program->error) goto cleanup;
        }
        else if (check_type(program)) {
            stmt = parse_function_declaration_stmt(program);
            if (program->error) goto cleanup;
        }
        else {
            program_error(program, peek(program), "Unknown token.");
            goto cleanup;
        }

        array_push(&global_stmts, stmt);
    }

    //
    // First pass parsing
    //

    for (int i = 0; i < global_stmts.length; i++) {
        struct stmt *stmt = global_stmts.data[i];
        if (stmt->type == STMT_IMPORT) {
        }
        else if (stmt->type == STMT_IMPORT_FUNCTION) {
        }
        else if (stmt->type == STMT_STRUCT_DECLARATION) {
            if (program_get_struct_decl(program, stmt->struct_declaration.name)) {
                program_error(program, stmt->token, "Multiple declarations of struct %s.", stmt->struct_declaration.name);
                goto cleanup;
            }
            program_add_struct_decl(program, stmt);
        }
        else if (stmt->type == STMT_FUNCTION_DECLARATION) {
            if (program_get_function_decl(program, stmt->function_declaration.name)) {
                program_error(program, stmt->token, "Multiple declarations of function %s.", stmt->function_declaration.name);
                goto cleanup;
            }
            program_add_function_decl(program, stmt);
        }
        else {
            assert(false);
        }
    }

    //
    // Second pass pre compiler
    //

    for (int i = 0; i < global_stmts.length; i++) {
        struct stmt *stmt = global_stmts.data[i];
        if (stmt->type == STMT_IMPORT) {
            struct mscript_program *import = mscript_load_program(mscript, stmt->import.program_name);
            if (!import || import->error) {
                program_error(program, stmt->token, "Failed to import program \"%s\".", stmt->import.program_name);
                goto cleanup;
            }

            {
                const char *key;
                map_iter_t iter = map_iter(&import->function_decl_map);

                while ((key = map_next(&import->function_decl_map, &iter))) {
                    if (program_get_function_decl(program, key)) {
                        program_error(program, stmt->token, "Multiple declarations of function %s.", key);
                        goto cleanup;
                    }
                }
            }

            {
                const char *key;
                map_iter_t iter = map_iter(&import->struct_decl_map);

                while ((key = map_next(&import->struct_decl_map, &iter))) {
                    if (program_get_struct_decl(program, key)) {
                        program_error(program, stmt->token, "Multiple declarations of struct %s.", key);
                        goto cleanup;
                    }
                }
            }

            array_push(&program->imported_programs_array, import);
        }
        else if (stmt->type == STMT_IMPORT_FUNCTION) {
            pre_compiler_import_function(program, stmt);
            if (program_get_function_decl(program, stmt->import_function.name)) {
                program_error(program, stmt->token, "Multiple declarations of function %s.", stmt->import_function.name);
                goto cleanup;
            }
            program_add_function_decl(program, stmt);
        }
        else if (stmt->type == STMT_STRUCT_DECLARATION) {
        }
        else if (stmt->type == STMT_FUNCTION_DECLARATION) {
        }
        else {
            assert(false);
        }
    }

    for (int i = 0; i < global_stmts.length; i++) {
        struct stmt *stmt = global_stmts.data[i];
        if (stmt->type == STMT_IMPORT) {
        }
        else if (stmt->type == STMT_IMPORT_FUNCTION) {
        }
        else if (stmt->type == STMT_STRUCT_DECLARATION) {
            pre_compiler_struct_declaration(program, stmt);
            if (program->error) goto cleanup;
        }
        else if (stmt->type == STMT_FUNCTION_DECLARATION) {
        }
        else {
            assert(false);
        }
    }

    for (int i = 0; i < global_stmts.length; i++) {
        struct stmt *stmt = global_stmts.data[i];
        if (stmt->type == STMT_IMPORT) {
        }
        else if (stmt->type == STMT_IMPORT_FUNCTION) {
        }
        else if (stmt->type == STMT_STRUCT_DECLARATION) {
        }
        else if (stmt->type == STMT_FUNCTION_DECLARATION) {
            pre_compiler_start(program, stmt);
            if (program->error) goto cleanup;
        }
        else {
            assert(false);
        }

        //debug_log_stmt(stmt);
    }

    for (int i = 0; i < global_stmts.length; i++) {
        struct stmt *stmt = global_stmts.data[i];
        if (stmt->type == STMT_IMPORT) {
        }
        else if (stmt->type == STMT_IMPORT_FUNCTION) {
        }
        else if (stmt->type == STMT_STRUCT_DECLARATION) {
        }
        else if (stmt->type == STMT_FUNCTION_DECLARATION) {
            if (strcmp(stmt->function_declaration.name, "test") == 0) {
                compile_stmt(program, stmt);
                debug_log_opcodes(program->compiler.opcodes.data, 
                        program->compiler.opcodes.length);
                vm_run(program, program->compiler.opcodes);
            }
            if (program->error) goto cleanup;
        }
        else {
            assert(false);
        }

        //debug_log_stmt(stmt);
    }

cleanup:
    array_deinit(&global_stmts);
}

static void program_init(struct mscript_program *prog, struct mscript *mscript, const char *prog_text) {
    prog->error = NULL;
    array_init(&prog->imported_programs_array);
    map_init(&prog->struct_decl_map);
    map_init(&prog->function_decl_map);
    parser_init(&prog->parser, prog_text);
    pre_compiler_init(prog);
    compiler_init(prog);
    vm_init(prog);

    parser_run(prog, mscript);
}

static void program_add_struct_decl(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_STRUCT_DECLARATION);

    struct struct_decl decl;
    decl.size = -1;
    strncpy(decl.name, stmt->struct_declaration.name, MSCRIPT_MAX_SYMBOL_LEN);
    decl.name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
    decl.num_members = stmt->struct_declaration.num_members;
    for (int i = 0; i < decl.num_members; i++) {
        decl.members[i].type = stmt->struct_declaration.member_types[i];
        strncpy(decl.members[i].name, stmt->struct_declaration.member_names[i], MSCRIPT_MAX_SYMBOL_LEN);
        decl.members[i].name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
        decl.members[i].offset = -1;
    }

    map_set(&program->struct_decl_map, decl.name, decl);
}

static struct struct_decl *program_get_struct_decl(struct mscript_program *program, const char *name) {
    struct struct_decl *decl = map_get(&program->struct_decl_map, name);
    if (decl) {
        return decl;
    }

    for (int i = 0; i < program->imported_programs_array.length; i++) {
        struct mscript_program *import = program->imported_programs_array.data[i];
        decl = program_get_struct_decl(import, name);
        if (decl) {
            return decl;
        }
    }

    return NULL;
}

static bool program_get_struct_decl_member(struct struct_decl *decl, const char *member, struct mscript_type *type, int *offset) {
    for (int i = 0; i < decl->num_members; i++) {
        if (strcmp(decl->members[i].name, member) == 0) {
            *type = decl->members[i].type;
            *offset = decl->members[i].offset;
            return true;
        }
    }
    return false;
}

static void program_add_function_decl(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_FUNCTION_DECLARATION || stmt->type == STMT_IMPORT_FUNCTION);

    if (stmt->type == STMT_FUNCTION_DECLARATION) {
        struct function_decl decl;
        decl.return_type = stmt->function_declaration.return_type;
        strncpy(decl.name, stmt->function_declaration.name, MSCRIPT_MAX_SYMBOL_LEN);
        decl.name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
        decl.num_args = stmt->function_declaration.num_args;
        for (int i = 0; i < decl.num_args; i++) {
            decl.args[i].type = stmt->function_declaration.arg_types[i];
            strncpy(decl.args[i].name, stmt->function_declaration.arg_names[i], MSCRIPT_MAX_SYMBOL_LEN);
            decl.args[i].name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
        }
        map_set(&program->function_decl_map, decl.name, decl);
    }
    
    if (stmt->type == STMT_IMPORT_FUNCTION) {
        struct function_decl decl;
        decl.return_type = stmt->import_function.return_type;
        strncpy(decl.name, stmt->import_function.name, MSCRIPT_MAX_SYMBOL_LEN);
        decl.name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
        decl.num_args = stmt->import_function.num_args;
        for (int i = 0; i < decl.num_args; i++) {
            decl.args[i].type = stmt->import_function.arg_types[i];
            strncpy(decl.args[i].name, stmt->import_function.arg_names[i], MSCRIPT_MAX_SYMBOL_LEN);
            decl.args[i].name[MSCRIPT_MAX_SYMBOL_LEN] = 0;
        }
        map_set(&program->function_decl_map, decl.name, decl);
    }

}

static struct function_decl *program_get_function_decl(struct mscript_program *program, const char *name) {
    struct function_decl *decl = map_get(&program->function_decl_map, name);
    if (decl) {
        return decl;
    }

    for (int i = 0; i < program->imported_programs_array.length; i++) {
        struct mscript_program *import = program->imported_programs_array.data[i];
        decl = program_get_function_decl(import, name);
        if (decl) {
            return decl;
        }
    }

    return NULL;
}

static void program_error(struct mscript_program *program, struct token token, char *fmt, ...) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[255] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 255, fmt, args);
    va_end(args);

    program->error = buffer;
    program->error_token = token;
}


static void allocator_init(struct allocator *allocator) {
    allocator->bytes_allocated = 0;
    array_init(&allocator->ptrs);
}

static void allocator_deinit(struct allocator *allocator) {
    array_deinit(&allocator->ptrs);
}

static void *allocator_alloc(struct allocator *allocator, size_t size) {
    allocator->bytes_allocated += size;
    void *mem = malloc(size);
    array_push(&allocator->ptrs, mem);
    return mem;
}

static void parser_init(struct parser *parser, const char *prog_text) {
    parser->prog_text = prog_text;
    allocator_init(&parser->allocator);
    parser->token_idx = 0;
    array_init(&parser->tokens);
    parser->error = NULL;
}

static void parser_deinit(struct parser *parser) {
}

static struct token peek(struct mscript_program *program) {
    struct parser *parser = &program->parser;

    if (parser->token_idx >= parser->tokens.length) {
        // Return EOF
        return parser->tokens.data[parser->tokens.length - 1];
    }
    else {
        return parser->tokens.data[parser->token_idx];
    }
}

static struct token peek_n(struct mscript_program *program, int n) {
    struct parser *parser = &program->parser;

    if (parser->token_idx + n >= parser->tokens.length) {
        // Return EOF
        return parser->tokens.data[parser->tokens.length - 1];
    }
    else {
        return parser->tokens.data[parser->token_idx + n];
    }
}

static void eat(struct mscript_program *program) {
    struct parser *parser = &program->parser;
    parser->token_idx++;
}

static bool match_char(struct mscript_program *program, char c) {
    struct token tok = peek(program);
    if (tok.type == TOKEN_CHAR && tok.char_value == c) {
        eat(program);
        return true;
    }
    else {
        return false;
    }
}

static bool match_char_n(struct mscript_program *program, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        char c = va_arg(ap, int);
        struct token tok = peek_n(program, i);
        if (tok.type != TOKEN_CHAR || tok.char_value != c) {
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

static bool match_symbol(struct mscript_program *program, const char *symbol) {
    struct token tok = peek(program);
    if (tok.type == TOKEN_SYMBOL && (strcmp(symbol, tok.symbol) == 0)) {
        eat(program);
        return true;
    }
    else {
        return false;
    }
}

static bool match_symbol_n(struct mscript_program *program, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *symbol = va_arg(ap, const char *);
        struct token tok = peek_n(program, i);
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

static bool match_eof(struct mscript_program *program) {
    struct token tok = peek(program);
    return tok.type == TOKEN_EOF;
}

static bool check_type(struct mscript_program *program) {
    // Type's begin with 2 symbols or 1 symbol followed by [] for an array.
    // Or void*
    struct token tok0 = peek_n(program, 0);
    struct token tok1 = peek_n(program, 1);
    struct token tok2 = peek_n(program, 2);
    return ((tok0.type == TOKEN_SYMBOL) && (tok1.type == TOKEN_SYMBOL)) ||
            ((tok0.type == TOKEN_SYMBOL) &&
             (tok1.type == TOKEN_CHAR) &&
             (tok1.char_value == '[') &&
             (tok2.type == TOKEN_CHAR) &&
             (tok2.char_value == ']')) ||
            ((tok0.type == TOKEN_SYMBOL) && (strcmp(tok0.symbol, "void") == 0) &&
             (tok1.type == TOKEN_CHAR) && (tok1.char_value == '*'));
}

static void pre_compiler_init(struct mscript_program *program) {
    struct pre_compiler *pre_compiler = &program->pre_compiler;
    pre_compiler->function_decl = NULL;
    array_init(&pre_compiler->env_blocks);
}

static void pre_compiler_env_push_block(struct mscript_program *program) {
    struct pre_compiler *pre_compiler = &program->pre_compiler;
    struct pre_compiler_env_block block;
    block.size = 0;
    block.max_size = 0;

    if (pre_compiler->env_blocks.length == 0) {
        block.offset = 0;
    }
    else {
        int l = pre_compiler->env_blocks.length;
        struct pre_compiler_env_block prev_block = pre_compiler->env_blocks.data[l - 1];
        block.offset = prev_block.offset + prev_block.size;
    }

    map_init(&block.map);
    array_push(&pre_compiler->env_blocks, block);
}

static void pre_compiler_env_pop_block(struct mscript_program *program) {
    struct pre_compiler *pre_compiler = &program->pre_compiler;
    struct pre_compiler_env_block block = array_pop(&pre_compiler->env_blocks);
    map_deinit(&block.map);
}

static void pre_compiler_env_add_var(struct mscript_program *program, const char *symbol, struct mscript_type type) {
    struct pre_compiler *pre_compiler = &program->pre_compiler;
    assert(pre_compiler->env_blocks.length > 0);
    int l = pre_compiler->env_blocks.length;
    struct pre_compiler_env_block *block = &(pre_compiler->env_blocks.data[l - 1]);
    struct pre_compiler_env_var var;
    var.offset = block->offset + block->size;
    var.type = type;
    if (l > 1) {
        // don't count function arguments for the blocks size 
        block->size += type_size(program, type);
    }
    map_set(&block->map, symbol, var);

    int cur_size = 0;
    for (int i = 0; i < l; i++) {
        struct pre_compiler_env_block *b = &(pre_compiler->env_blocks.data[i]);
        cur_size += b->size;
    }

    struct pre_compiler_env_block *b0 = &(pre_compiler->env_blocks.data[0]);
    if (b0->max_size < cur_size) {
        b0->max_size += cur_size;
    }
}

static struct pre_compiler_env_var *pre_compiler_env_get_var(struct mscript_program *program, const char *symbol) {
    for (int i = program->pre_compiler.env_blocks.length - 1; i >= 0; i--) {
        struct pre_compiler_env_block *block = &(program->pre_compiler.env_blocks.data[i]);
        struct pre_compiler_env_var *var = map_get(&block->map, symbol);
        if (var) {
            return var;
        }
    }
    return NULL;
}

static struct pre_compiler_env_var *pre_compiler_top_env_get_var(struct mscript_program *program, const char *symbol) {
    assert(program->pre_compiler.env_blocks.length > 0);
    int i = program->pre_compiler.env_blocks.length - 1;
    struct pre_compiler_env_block *block = &(program->pre_compiler.env_blocks.data[i]);
    struct pre_compiler_env_var *var = map_get(&block->map, symbol);
    if (var) {
        return var;
    }
    return NULL;
}

static void pre_compiler_start(struct mscript_program *program, struct stmt *function_decl) {
    assert(function_decl->type == STMT_FUNCTION_DECLARATION);
    program->pre_compiler.function_decl = function_decl;

    pre_compiler_env_push_block(program);
    for (int i = 0; i < function_decl->function_declaration.num_args; i++) {
        char *arg_name = function_decl->function_declaration.arg_names[i];
        struct mscript_type arg_type = function_decl->function_declaration.arg_types[i];
        pre_compiler_type(program, function_decl->token, arg_type);
        if (program->error) goto cleanup;
        pre_compiler_env_add_var(program, arg_name, arg_type);
    }
    bool all_paths_return;
    pre_compiler_stmt(program, function_decl->function_declaration.body, &all_paths_return);
    if (program->error) goto cleanup;

    if (function_decl->function_declaration.return_type.type != MSCRIPT_TYPE_VOID && !all_paths_return) {
        program_error(program, function_decl->function_declaration.token, "Not all paths return from function.");
    }

   function_decl->function_declaration.block_size =  program->pre_compiler.env_blocks.data[0].max_size;
cleanup:
    pre_compiler_env_pop_block(program);
}

static void pre_compiler_type(struct mscript_program *program, struct token token, struct mscript_type type) {
    switch (type.type) {
        case MSCRIPT_TYPE_VOID:
        case MSCRIPT_TYPE_VOID_STAR:
        case MSCRIPT_TYPE_INT:
        case MSCRIPT_TYPE_FLOAT:
        case MSCRIPT_TYPE_STRING:
            break;
        case MSCRIPT_TYPE_STRUCT:
            {
                if (!program_get_struct_decl(program, type.struct_name)) {
                    program_error(program, token, "Unknown struct type %s.", type.struct_name);
                }
            }
            break;
        case MSCRIPT_TYPE_ARRAY:
            {
                if (type.array_type == MSCRIPT_TYPE_STRUCT) {
                    if (!program_get_struct_decl(program, type.struct_name)) {
                        program_error(program, token, "Unknown struct type %s.", type.struct_name);
                    }
                }
            }
            break;
    }
}

static void pre_compiler_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    *all_paths_return = false;
    switch (stmt->type) {
        case STMT_IF:
            pre_compiler_if_stmt(program, stmt, all_paths_return);
            break;
        case STMT_FOR:
            pre_compiler_for_stmt(program, stmt, all_paths_return);
            break;
        case STMT_RETURN:
            pre_compiler_return_stmt(program, stmt, all_paths_return);
            break;
        case STMT_BLOCK:
            pre_compiler_block_stmt(program, stmt, all_paths_return);
            break;
        case STMT_EXPR:
            pre_compiler_expr_stmt(program, stmt, all_paths_return);
            break;
        case STMT_VARIABLE_DECLARATION:
            pre_compiler_variable_declaration_stmt(program, stmt, all_paths_return);
            break;
        case STMT_FUNCTION_DECLARATION:
        case STMT_STRUCT_DECLARATION:
        case STMT_IMPORT:
        case STMT_IMPORT_FUNCTION:
            // shouldn't do analysis on global statements
            assert(false);
            break;
    }
}

static void pre_compiler_if_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    assert(stmt->type == STMT_IF);

    *all_paths_return = true;
    for (int i = 0; i < stmt->if_stmt.num_stmts; i++) {
        pre_compiler_expr_with_cast(program, &(stmt->if_stmt.conds[i]), int_type());
        if (program->error) return;

        bool stmt_all_paths_return;
        pre_compiler_stmt(program, stmt->if_stmt.stmts[i], &stmt_all_paths_return);
        if (program->error) return;

        if (!stmt_all_paths_return) {
            *all_paths_return = false;
        }
    }

    if (stmt->if_stmt.else_stmt) {
        bool stmt_all_paths_return;
        pre_compiler_stmt(program, stmt->if_stmt.else_stmt, &stmt_all_paths_return);
        if (program->error) return;

        if (!stmt_all_paths_return) {
            *all_paths_return = false;
        }
    }
    else {
        *all_paths_return = false;
    }
}

static void pre_compiler_for_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    assert(stmt->type == STMT_FOR);

    pre_compiler_expr(program, stmt->for_stmt.init, NULL);
    if (program->error) return;
    pre_compiler_expr_with_cast(program, &(stmt->for_stmt.cond), int_type());
    if (program->error) return;
    pre_compiler_expr(program, stmt->for_stmt.inc, NULL);
    if (program->error) return;
    pre_compiler_stmt(program, stmt->for_stmt.body, all_paths_return);
    if (program->error) return;
}

static void pre_compiler_return_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    assert(stmt->type == STMT_RETURN);
    *all_paths_return = true;

    struct mscript_type return_type = program->pre_compiler.function_decl->function_declaration.return_type;

    if (return_type.type == MSCRIPT_TYPE_VOID) {
        if (stmt->return_stmt.expr) {
            program_error(program, stmt->token, "Cannot return expression for void function.");
            return;
        }
     }
    else {
        if (!stmt->return_stmt.expr) {
            program_error(program, stmt->token, "Must return expression for non-void function.");
            return;
        }
        else {
            pre_compiler_expr_with_cast(program, &(stmt->return_stmt.expr), return_type);
        }
    }
}

static void pre_compiler_block_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    assert(stmt->type == STMT_BLOCK);

    pre_compiler_env_push_block(program);
    for (int i = 0; i < stmt->block.num_stmts; i++) {
        bool stmt_all_paths_return;
        pre_compiler_stmt(program, stmt->block.stmts[i], &stmt_all_paths_return);
        if (program->error) goto cleanup;
        if (stmt_all_paths_return) {
            *all_paths_return = true;
        }

        if (stmt_all_paths_return && (i < stmt->block.num_stmts - 1)) {
            program_error(program, stmt->block.stmts[i + 1]->token, "Unreachable statement.");
            goto cleanup;

        }
    }

cleanup:
    pre_compiler_env_pop_block(program);
}

static void pre_compiler_expr_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    assert(stmt->type == STMT_EXPR);
    pre_compiler_expr(program, stmt->expr, NULL);
}

static void pre_compiler_variable_declaration_stmt(struct mscript_program *program, struct stmt *stmt, bool *all_paths_return) {
    assert(stmt->type == STMT_VARIABLE_DECLARATION);

    char *name = stmt->variable_declaration.name;
    struct mscript_type type = stmt->variable_declaration.type;

    pre_compiler_type(program, peek(program), type);
    if (program->error) return;

    if (pre_compiler_top_env_get_var(program, name)) {
        program_error(program, stmt->token, "Symbol already declared.");
        return;
    }

    pre_compiler_env_add_var(program, name, type);
    if (stmt->variable_declaration.assignment_expr) {
        pre_compiler_expr(program, stmt->variable_declaration.assignment_expr, NULL);
        if (program->error) return;
    }
}

static bool pre_compiler_is_struct_declaration_recursive(struct mscript_program *program, struct stmt *stmt, struct mscript_type cur) {
    assert(stmt->type == STMT_STRUCT_DECLARATION);

    struct mscript_type parent = struct_type(stmt->struct_declaration.name);
    if (types_equal(parent, cur)) {
        return true;
    }

    if (cur.type != MSCRIPT_TYPE_STRUCT) {
        return false;
    }

    struct struct_decl *decl = program_get_struct_decl(program, cur.struct_name);
    if (!decl) {
        program_error(program, stmt->token, "Unknown type %s", cur.struct_name);
        return false;
    }

    for (int i = 0; i < decl->num_members; i++) {
        struct mscript_type type = decl->members[i].type;
        if (pre_compiler_is_struct_declaration_recursive(program, stmt, type)) {
            return true;
        }
    }

    return false;
}

static void pre_compiler_set_struct_declaration_size(struct mscript_program *program, struct struct_decl *decl) {
    // If it's already been set then skip this
    if (decl->size >= 0) {
        return;
    }

    int size = 0;
    for (int i = 0; i < decl->num_members; i++) {
        decl->members[i].offset = size;
        struct mscript_type type = decl->members[i].type;
        switch (type.type) {
            case MSCRIPT_TYPE_VOID:
                assert(false);
                break;
            case MSCRIPT_TYPE_VOID_STAR:
                size += 4;
                break;
            case MSCRIPT_TYPE_INT:
                size += 4;
                break;
            case MSCRIPT_TYPE_FLOAT:
                size += 4;
                break;
            case MSCRIPT_TYPE_STRUCT: 
                {
                    struct struct_decl *member_decl = program_get_struct_decl(program, type.struct_name);
                    assert(member_decl);
                    pre_compiler_set_struct_declaration_size(program, member_decl);
                    size += member_decl->size;
                }
                break;
            case MSCRIPT_TYPE_ARRAY:
                size += 4;
                break;
            case MSCRIPT_TYPE_STRING:
                size += 4;
                break;
        }
    }
    decl->size = size;
}

static void pre_compiler_struct_declaration(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_STRUCT_DECLARATION);

    for (int i = 0; i < stmt->struct_declaration.num_members; i++) {
        struct mscript_type type = stmt->struct_declaration.member_types[i];
        pre_compiler_type(program, stmt->token, type);
        if (program->error) return;
        if (pre_compiler_is_struct_declaration_recursive(program, stmt, type)) {
            program_error(program, stmt->token, "Recursive type definition not allowed.");
            return;
        }
    }

    struct struct_decl *decl = program_get_struct_decl(program, stmt->struct_declaration.name);
    assert(decl);
    pre_compiler_set_struct_declaration_size(program, decl); 
}

static void pre_compiler_import_function(struct mscript_program *program, struct stmt *stmt) {
    pre_compiler_type(program, stmt->token, stmt->import_function.return_type);
    for (int i = 0; i < stmt->import_function.num_args; i++) {
        pre_compiler_type(program, stmt->token, stmt->import_function.arg_types[i]);
    }
}

static void pre_compiler_expr_with_cast(struct mscript_program *program, struct expr **expr, struct mscript_type type) {
    pre_compiler_expr(program, *expr, &type);
    if (program->error) return;

    if (types_equal((*expr)->result_type, type)) {
        return;
    }

    pre_compiler_type(program, peek(program), type);

    char buffer1[MSCRIPT_MAX_SYMBOL_LEN + 1], buffer2[MSCRIPT_MAX_SYMBOL_LEN + 1];
    type_to_string((*expr)->result_type, buffer1, MSCRIPT_MAX_SYMBOL_LEN + 1);
    type_to_string(type, buffer2, MSCRIPT_MAX_SYMBOL_LEN + 1);

    if (type.type == MSCRIPT_TYPE_INT) {
        if ((*expr)->result_type.type == MSCRIPT_TYPE_FLOAT) {
            *expr = new_cast_expr(&program->parser.allocator, (*expr)->token, type, *expr);
        }
        else {

            program_error(program, (*expr)->token, "Unable to cast from %s to %s.", buffer1, buffer2);
            return;
        }
    }
    else if (type.type == MSCRIPT_TYPE_FLOAT) {
        if ((*expr)->result_type.type == MSCRIPT_TYPE_INT) {
            *expr = new_cast_expr(&program->parser.allocator, (*expr)->token, type, *expr);
        }
        else {
            program_error(program, (*expr)->token, "Unable to cast from %s to %s", buffer1, buffer2);
            return;
        }
    }
    else {
        program_error(program, (*expr)->token, "Unable to cast from %s to %s", buffer1, buffer2);
        return;
    }
}

static void pre_compiler_expr_lvalue(struct mscript_program *program, struct expr *expr) {
    switch (expr->type) {
        case EXPR_UNARY_OP:
        case EXPR_BINARY_OP:
        case EXPR_CALL:
        case EXPR_DEBUG_PRINT:
        case EXPR_ASSIGNMENT:
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_ARRAY:
        case EXPR_OBJECT:
        case EXPR_CAST:
            {
                program_error(program, expr->token, "Invalid lvalue.");
            }
            break;
        case EXPR_ARRAY_ACCESS:
            {
                pre_compiler_expr(program, expr->array_access.left, NULL);
                if (program->error) return;

                struct mscript_type left_type = expr->array_access.left->result_type;
                if (left_type.type != MSCRIPT_TYPE_ARRAY) {
                    char buffer[MSCRIPT_MAX_SYMBOL_LEN + 1];
                    type_to_string(left_type, buffer, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    program_error(program, expr->array_access.left->token, "Cannot perform array access on type %s.", buffer);
                    return;
                }

                pre_compiler_expr_with_cast(program, &(expr->array_access.right), int_type());

                expr->result_type = left_type;
                expr->result_type.type = left_type.array_type;
                expr->lvalue = lvalue_array();
            }
            break;
        case EXPR_MEMBER_ACCESS:
            {
                pre_compiler_expr(program, expr->member_access.left, NULL);
                if (program->error) return;

                struct mscript_type left_type = expr->member_access.left->result_type;
                if (left_type.type != MSCRIPT_TYPE_STRUCT) {
                    char buffer[MSCRIPT_MAX_SYMBOL_LEN + 1];
                    type_to_string(left_type, buffer, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    program_error(program, expr->member_access.left->token, "Cannot perform member access on type %s.", buffer);
                    return;
                }

                struct struct_decl *decl = program_get_struct_decl(program, left_type.struct_name);
                if (!decl) {
                    program_error(program, expr->member_access.left->token, "Unknown struct type %s.", left_type.struct_name);
                    return;
                }

                struct mscript_type member_type;
                int member_offset;
                if (!program_get_struct_decl_member(decl, expr->member_access.member_name, &member_type, &member_offset)) {
                    program_error(program, expr->token, "Invalid member %s on type %s.", expr->member_access.member_name, left_type.struct_name);
                    return;
                }

                struct lvalue lvalue;
                struct lvalue left_lvalue = expr->member_access.left->lvalue;
                switch (left_lvalue.type) {
                    case LVALUE_INVALID:
                        assert(false);
                        break;
                    case LVALUE_LOCAL:
                        lvalue = lvalue_local(left_lvalue.offset + member_offset);
                        break;
                    case LVALUE_ARRAY:
                        lvalue = lvalue_array();
                        break;
                }

                expr->result_type = member_type;
                expr->lvalue = lvalue;
            }
            break;
        case EXPR_SYMBOL:
            {
                pre_compiler_expr(program, expr, NULL);
            }
            break;
        case EXPR_STRING:
            {
                program_error(program, expr->token, "A string cannot be an lvalue.");
                return;
            }
            break;
    }
}

static void pre_compiler_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    switch (expr->type) {
        case EXPR_UNARY_OP:
            pre_compiler_unary_op_expr(program, expr, expected_type);
            break;
        case EXPR_BINARY_OP:
            pre_compiler_binary_op_expr(program, expr, expected_type);
            break;
        case EXPR_CALL:
            pre_compiler_call_expr(program, expr, expected_type);
            break;
        case EXPR_DEBUG_PRINT:
            pre_compiler_debug_print_expr(program, expr, expected_type);
            break;
        case EXPR_MEMBER_ACCESS:
            pre_compiler_member_access_expr(program, expr, expected_type);
            break;
        case EXPR_ASSIGNMENT:
            pre_compiler_assignment_expr(program, expr, expected_type);
            break;
        case EXPR_INT:
            pre_compiler_int_expr(program, expr, expected_type);
            break;
        case EXPR_FLOAT:
            pre_compiler_float_expr(program, expr, expected_type);
            break;
        case EXPR_SYMBOL:
            pre_compiler_symbol_expr(program, expr, expected_type);
            break;
        case EXPR_STRING:
            pre_compiler_string_expr(program, expr, expected_type);
            break;
        case EXPR_ARRAY:
            pre_compiler_array_expr(program, expr, expected_type);
            break;
        case EXPR_ARRAY_ACCESS:
            pre_compiler_array_access_expr(program, expr, expected_type);
            break;
        case EXPR_OBJECT:
            pre_compiler_object_expr(program, expr, expected_type);
            break;
        case EXPR_CAST:
            pre_compiler_cast_expr(program, expr, expected_type);
            break;
    }
}

static void pre_compiler_unary_op_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_UNARY_OP);

    pre_compiler_expr_lvalue(program, expr->unary_op.operand);
    if (program->error) return;

    struct mscript_type operand_type = expr->unary_op.operand->result_type;
    switch (expr->unary_op.type) {
        case UNARY_OP_POST_INC:
            {
                if (operand_type.type == MSCRIPT_TYPE_INT) {
                    expr->result_type = int_type();
                }
                else if (operand_type.type == MSCRIPT_TYPE_FLOAT) {
                    expr->result_type = float_type();
                }
                else {
                    char buffer[MSCRIPT_MAX_SYMBOL_LEN + 1];
                    type_to_string(operand_type, buffer, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    program_error(program, expr->token, "Unable to do increment on type %s.", buffer);
                    return;
                }
            }
            break;
    }

    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_binary_op_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_BINARY_OP);

    pre_compiler_expr(program, expr->binary_op.left, expected_type);
    if (program->error) return;
    pre_compiler_expr(program, expr->binary_op.right, expected_type);
    if (program->error) return;

    struct mscript_type left_result_type = expr->binary_op.left->result_type;
    struct mscript_type right_result_type = expr->binary_op.right->result_type;

    switch (expr->binary_op.type) {
        case BINARY_OP_ADD:
        case BINARY_OP_SUB:
        case BINARY_OP_MUL:
        case BINARY_OP_DIV:
            {
                if (left_result_type.type == MSCRIPT_TYPE_INT && right_result_type.type == MSCRIPT_TYPE_INT) {
                    expr->result_type = int_type();
                }
                else if (left_result_type.type == MSCRIPT_TYPE_FLOAT && right_result_type.type == MSCRIPT_TYPE_FLOAT) {
                    expr->result_type = float_type();
                }
                else if (left_result_type.type == MSCRIPT_TYPE_FLOAT && right_result_type.type == MSCRIPT_TYPE_INT) {
                    expr->result_type = float_type();
                    expr->binary_op.right = new_cast_expr(&program->parser.allocator, expr->token, float_type(), expr->binary_op.right);
                }
                else if (left_result_type.type == MSCRIPT_TYPE_INT && right_result_type.type == MSCRIPT_TYPE_FLOAT) {
                    expr->result_type = float_type();
                    expr->binary_op.left = new_cast_expr(&program->parser.allocator, expr->token, float_type(), expr->binary_op.left);
                }
                else {
                    char buffer1[MSCRIPT_MAX_SYMBOL_LEN + 1], buffer2[MSCRIPT_MAX_SYMBOL_LEN + 1];
                    type_to_string(left_result_type, buffer1, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    type_to_string(right_result_type, buffer2, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    program_error(program, expr->token, "Unable to do this binary operation on types %s and %s.", buffer1, buffer2);
                    return;
                }
            }
            break;
        case BINARY_OP_LTE:
        case BINARY_OP_LT:
        case BINARY_OP_GTE:
        case BINARY_OP_GT:
        case BINARY_OP_EQ:
        case BINARY_OP_NEQ:
            {
                expr->result_type = int_type();

                if (left_result_type.type == MSCRIPT_TYPE_INT && right_result_type.type == MSCRIPT_TYPE_INT) {
                    // no casts needed
                }
                else if (left_result_type.type == MSCRIPT_TYPE_FLOAT && right_result_type.type == MSCRIPT_TYPE_FLOAT) {
                    // no casts needed
                }
                else if (left_result_type.type == MSCRIPT_TYPE_FLOAT && right_result_type.type == MSCRIPT_TYPE_INT) {
                    expr->binary_op.right = new_cast_expr(&program->parser.allocator, expr->token, float_type(), expr->binary_op.right);
                }
                else if (left_result_type.type == MSCRIPT_TYPE_INT && right_result_type.type == MSCRIPT_TYPE_FLOAT) {
                    expr->binary_op.left = new_cast_expr(&program->parser.allocator, expr->token, float_type(), expr->binary_op.left);
                }
                else {
                    char buffer1[MSCRIPT_MAX_SYMBOL_LEN + 1], buffer2[MSCRIPT_MAX_SYMBOL_LEN + 1];
                    type_to_string(left_result_type, buffer1, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    type_to_string(right_result_type, buffer2, MSCRIPT_MAX_SYMBOL_LEN + 1);
                    program_error(program, expr->token, "Unable to do this binary operation on types %s and %s.", buffer1, buffer2);
                    return;
                }
            }
            break;
    }

    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_call_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_CALL);

    struct expr *fn = expr->call.function;
    if (fn->type != EXPR_SYMBOL) {
        program_error(program, fn->token, "Expect symbol in function position.");
        return;
    }

    struct function_decl *decl = program_get_function_decl(program, fn->symbol);
    if (!decl) {
        program_error(program, fn->token, "Unknown function %s.", fn->symbol);
        return;
    }

    if (decl->num_args != expr->call.num_args) {
        program_error(program, expr->token, "Invalid number of arguments to function %s. Expected %d but got %d.", fn->symbol, decl->num_args, expr->call.num_args);
        return;
    }

    for (int i = 0; i < expr->call.num_args; i++) {
        pre_compiler_expr_with_cast(program, &(expr->call.args[i]), decl->args[i].type);
        if (program->error) return;
    }

    expr->result_type = decl->return_type;
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_debug_print_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_DEBUG_PRINT);

    for (int i = 0; i < expr->debug_print.num_args; i++) {
        pre_compiler_expr(program, expr->debug_print.args[i], NULL);
        if (program->error) return;
    }

    expr->result_type = void_type();
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_member_access_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_MEMBER_ACCESS);

    pre_compiler_expr(program, expr->member_access.left, NULL);
    if (program->error) return;

    struct mscript_type left_type = expr->member_access.left->result_type;
    if (left_type.type == MSCRIPT_TYPE_STRUCT) {
        struct struct_decl *decl = program_get_struct_decl(program, left_type.struct_name);
        if (!decl) {
            program_error(program, expr->token, "Invalid struct type %s.", left_type.struct_name);
            return;
        }

        struct mscript_type member_type;
        int member_offset;
        if (!program_get_struct_decl_member(decl, expr->member_access.member_name, &member_type, &member_offset)) {
            program_error(program, expr->token, "Invalid member %s on struct %s.", expr->member_access.member_name, left_type.struct_name);
            return;
        }

        struct lvalue lvalue;
        struct lvalue left_lvalue = expr->member_access.left->lvalue;
        switch (left_lvalue.type) {
            case LVALUE_INVALID:
                assert(false);
                break;
            case LVALUE_LOCAL:
                lvalue = lvalue_local(left_lvalue.offset + member_offset);
                break;
            case LVALUE_ARRAY:
                lvalue = lvalue_array();
                break;
        }

        expr->result_type = member_type;
        expr->lvalue = lvalue;
    }
    else if (left_type.type == MSCRIPT_TYPE_ARRAY) {
        if (strcmp(expr->member_access.member_name, "length") == 0) {
            expr->result_type = int_type();
        }
        else {
            program_error(program, expr->token, "Invalid member %s on array.", expr->member_access.member_name);
            return;
        }

        expr->lvalue = lvalue_array();
    }
    else {
        program_error(program, expr->token, "Invalid type for member access.");
        return;
    }
}

static void pre_compiler_assignment_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_ASSIGNMENT);

    pre_compiler_expr_lvalue(program, expr->assignment.left);
    if (program->error) return;

    struct mscript_type left_type = expr->assignment.left->result_type;

    pre_compiler_expr_with_cast(program, &(expr->assignment.right), left_type);
    if (program->error) return;

    expr->result_type = left_type;
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_int_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_INT);
    expr->result_type = int_type();
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_float_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_FLOAT);
    expr->result_type = float_type();
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_symbol_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_SYMBOL);

    struct pre_compiler_env_var *var = pre_compiler_env_get_var(program, expr->symbol);
    if (!var) {
        program_error(program, expr->token, "Undeclared variable %s.", expr->symbol);
        return;
    }
    expr->result_type = var->type;
    expr->lvalue = lvalue_local(var->offset);
}

static void pre_compiler_string_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_STRING);
    expr->result_type = string_type();
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_array_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_ARRAY);

    if (!expected_type) {
        program_error(program, expr->token, "Cannot determine type of array.");
        return;
    }

    if (expected_type->type != MSCRIPT_TYPE_ARRAY) {
        program_error(program, expr->token, "Not expecting array.");
        return;
    }

    struct mscript_type arg_type = *expected_type;
    arg_type.type = arg_type.array_type;
    for (int i = 0; i < expr->array.num_args; i++) {
        pre_compiler_expr_with_cast(program, &(expr->array.args[i]), arg_type);
        if (program->error) return;
    }

    expr->result_type = *expected_type;
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_array_access_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_ARRAY_ACCESS);

    pre_compiler_expr(program, expr->array_access.left, expected_type);
    if (program->error) return;

    struct mscript_type left_type = expr->array_access.left->result_type;
    if (left_type.type != MSCRIPT_TYPE_ARRAY) {
        program_error(program, expr->array_access.left->token, "Expected array.");
        return;
    }

    pre_compiler_expr_with_cast(program, &(expr->array_access.right), int_type());
    if (program->error) return;

    expr->result_type = left_type;
    expr->result_type.type = left_type.array_type;
    expr->lvalue = lvalue_array();
}

static void pre_compiler_object_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(expr->type == EXPR_OBJECT);
    
    if (!expected_type) {
        program_error(program, expr->token, "Cannot determine type of struct.");
        return;
    }

    if (expected_type->type != MSCRIPT_TYPE_STRUCT) {
        program_error(program, expr->token, "Not expecting struct.");
        return;
    }

    struct struct_decl *decl = program_get_struct_decl(program, expected_type->struct_name);
    if (!decl) {
        program_error(program, expr->token, "Invalid struct type %s.", expected_type->struct_name);
        return;
    }

    if (expr->object.num_args != decl->num_members) {
        program_error(program, expr->token, "Invalid number of members in object. Expected %d but got %d.", decl->num_members, expr->object.num_args);
        return;
    }

    for (int i = 0; i < expr->object.num_args; i++) {
        if (strcmp(expr->object.names[i], decl->members[i].name) != 0) {
            program_error(program, expr->token, "Incorrect member position for type %s. Expected %s but got %s.", expected_type->struct_name, decl->members[i].name, expr->object.names[i]);
            return;
        }

        struct mscript_type member_type = decl->members[i].type;
        pre_compiler_expr_with_cast(program, &(expr->object.args[i]), member_type);
        if (program->error) return;
    }

    expr->result_type = *expected_type;
    expr->lvalue = lvalue_invalid();
}

static void pre_compiler_cast_expr(struct mscript_program *program, struct expr *expr, struct mscript_type *expected_type) {
    assert(false);
}

static void opcode_iadd(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IADD;
    compiler_push_opcode(program, op);
}

static void opcode_fadd(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FADD;
    compiler_push_opcode(program, op);
}

static void opcode_isub(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_ISUB;
    array_push(&(program->compiler.opcodes), op);
}

static void opcode_fsub(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FSUB;
    compiler_push_opcode(program, op);
}

static void opcode_imul(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IMUL;
    compiler_push_opcode(program, op);
}

static void opcode_fmul(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FMUL;
    compiler_push_opcode(program, op);
}

static void opcode_idiv(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IDIV;
    compiler_push_opcode(program, op);
}

static void opcode_fdiv(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FDIV;
    compiler_push_opcode(program, op);
}

static void opcode_ilte(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_ILTE;
    compiler_push_opcode(program, op);
}

static void opcode_flte(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FLTE;
    compiler_push_opcode(program, op);
}

static void opcode_ilt(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_ILT;
    compiler_push_opcode(program, op);
}

static void opcode_flt(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FLT;
    compiler_push_opcode(program, op);
}

static void opcode_igte(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IGTE;
    compiler_push_opcode(program, op);
}

static void opcode_fgte(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FGTE;
    compiler_push_opcode(program, op);
}

static void opcode_igt(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IGT;
    compiler_push_opcode(program, op);
}

static void opcode_fgt(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FGT;
    compiler_push_opcode(program, op);
}

static void opcode_ieq(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IEQ;
    compiler_push_opcode(program, op);
}

static void opcode_feq(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FEQ;
    compiler_push_opcode(program, op);
}

static void opcode_ineq(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_INEQ;
    compiler_push_opcode(program, op);
}

static void opcode_fneq(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FNEQ;
    compiler_push_opcode(program, op);
}

static void opcode_iinc(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_IINC;
    compiler_push_opcode(program, op);
}

static void opcode_finc(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_FINC;
    compiler_push_opcode(program, op);
}

static void opcode_f2i(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_F2I;
    compiler_push_opcode(program, op);
}

static void opcode_i2f(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_I2F;
    compiler_push_opcode(program, op);
}

static void opcode_copy(struct mscript_program *program, int offset, int size) {
    struct opcode op;
    op.type = OPCODE_COPY;
    op.load_store.offset = offset;
    op.load_store.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_int(struct mscript_program *program, int val) {
    struct opcode op;
    op.type = OPCODE_INT;
    op.int_val = val;
    compiler_push_opcode(program, op);
}

static void opcode_float(struct mscript_program *program, float val) {
    struct opcode op;
    op.type = OPCODE_FLOAT;
    op.float_val = val;
    compiler_push_opcode(program, op);
}

static void opcode_local_store(struct mscript_program *program, int offset, int size) {
    struct opcode op;
    op.type = OPCODE_LOCAL_STORE;
    op.load_store.offset = offset;
    op.load_store.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_local_load(struct mscript_program *program, int offset, int size) {
    struct opcode op;
    op.type = OPCODE_LOCAL_LOAD;
    op.load_store.offset = offset;
    op.load_store.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_jf(struct mscript_program *program, int label) {
    struct opcode op;
    op.type = OPCODE_JF;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_jmp(struct mscript_program *program, int label) {
    struct opcode op;
    op.type = OPCODE_JMP;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_call(struct mscript_program *program, int function) {
    struct opcode op;
    op.type = OPCODE_CALL;
    op.function = function;
    compiler_push_opcode(program, op);
}

static void opcode_return(struct mscript_program *program, int size) {
    struct opcode op;
    op.type = OPCODE_RETURN;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_push(struct mscript_program *program, int size) {
    struct opcode op;
    op.type = OPCODE_PUSH;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_pop(struct mscript_program *program, int size) {
    struct opcode op;
    op.type = OPCODE_POP;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_create(struct mscript_program *program, int size) {
    struct opcode op;
    op.type = OPCODE_ARRAY_CREATE;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_store(struct mscript_program *program, int size) {
    struct opcode op;
    op.type = OPCODE_ARRAY_STORE;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_load(struct mscript_program *program, int size) {
    struct opcode op;
    op.type = OPCODE_ARRAY_LOAD;
    op.size = size;
    compiler_push_opcode(program, op);
}

static void opcode_array_length(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_ARRAY_LENGTH;
    compiler_push_opcode(program, op);
}

static void opcode_label(struct mscript_program *program, int label) {
    struct opcode op;
    op.type = OPCODE_LABEL;
    op.label = label;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_int(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_INT;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_float(struct mscript_program *program) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_FLOAT;
    compiler_push_opcode(program, op);
}

static void opcode_debug_print_string(struct mscript_program *program, char *string) {
    struct opcode op;
    op.type = OPCODE_DEBUG_PRINT_STRING;
    strncpy(op.string, string, OPCODE_MAX_STRING_LEN);
    op.string[OPCODE_MAX_STRING_LEN] = 0;
    compiler_push_opcode(program, op);
}

static void compiler_init(struct mscript_program *program) {
    struct compiler *compiler = &program->compiler;
    compiler->cur_label = 0;
    array_init(&compiler->opcodes);
}

static void compiler_deinit(struct mscript_program *program) {
}

static void compiler_push_opcode(struct mscript_program *program, struct opcode op) {
    array_push(&(program->compiler.opcodes), op);
}

static int compiler_new_label(struct mscript_program *program) {
    struct compiler *compiler = &program->compiler;
    return compiler->cur_label++;
}

static void compile_stmt(struct mscript_program *program, struct stmt *stmt) {
    switch (stmt->type) {
        case STMT_IF:
            compile_if_stmt(program, stmt);
            break;
        case STMT_RETURN:
            compile_return_stmt(program, stmt);
            break;
        case STMT_BLOCK:
            compile_block_stmt(program, stmt);
            break;
        case STMT_FUNCTION_DECLARATION:
            compile_function_declaration_stmt(program, stmt);
            break;
        case STMT_VARIABLE_DECLARATION:
            compile_variable_declaration_stmt(program, stmt);
            break;
        case STMT_EXPR:
            compile_expr_stmt(program, stmt);
            break;
        case STMT_FOR:
            compile_for_stmt(program, stmt);
            break;
        case STMT_STRUCT_DECLARATION:
        case STMT_IMPORT:
        case STMT_IMPORT_FUNCTION:
            break;

    }
}

static void compile_if_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_IF);

    int num_stmts = stmt->if_stmt.num_stmts;
    struct expr **conds = stmt->if_stmt.conds;
    struct stmt **stmts = stmt->if_stmt.stmts;
    struct stmt *else_stmt = stmt->if_stmt.else_stmt;

    int else_if_label = -1;
    int final_label = compiler_new_label(program);

    for (int i = 0; i < num_stmts; i++) {
        compile_expr(program, conds[i]);
        else_if_label = compiler_new_label(program);
        opcode_jf(program, else_if_label);
        compile_stmt(program, stmts[i]);
        opcode_jmp(program, final_label);
        opcode_label(program, else_if_label);
    }

    if (else_stmt) {
        compile_stmt(program, else_stmt);
    }

    opcode_label(program, final_label);
}

static void compile_for_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_FOR);

    struct expr *init = stmt->for_stmt.init;
    struct expr *cond = stmt->for_stmt.cond;
    struct expr *inc = stmt->for_stmt.inc;
    struct stmt *body = stmt->for_stmt.body;

    int cond_label = compiler_new_label(program);
    int end_label = compiler_new_label(program);

    compile_expr(program, init);
    opcode_pop(program, type_size(program, init->result_type));
    opcode_label(program, cond_label);
    compile_expr(program, cond);
    opcode_jf(program, end_label);
    compile_stmt(program, body);
    compile_expr(program, inc);
    opcode_pop(program, type_size(program, inc->result_type));
    opcode_jmp(program, cond_label);
    opcode_label(program, end_label);
}

static void compile_return_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_RETURN);

    struct expr *expr = stmt->return_stmt.expr;
    compile_expr(program, stmt->return_stmt.expr);
    opcode_return(program, type_size(program, expr->result_type));
}

static void compile_block_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_BLOCK);

    int num_stmts = stmt->block.num_stmts;
    struct stmt **stmts = stmt->block.stmts;
    for (int i = 0; i < num_stmts; i++) {
        compile_stmt(program, stmts[i]);
    }
}

static void compile_expr_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_EXPR);

    struct expr *expr = stmt->expr;
    compile_expr(program, expr);
    opcode_pop(program, type_size(program, expr->result_type));
}

static void compile_function_declaration_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_FUNCTION_DECLARATION);

    opcode_push(program, stmt->function_declaration.block_size);
    compile_stmt(program, stmt->function_declaration.body); 
}

static void compile_variable_declaration_stmt(struct mscript_program *program, struct stmt *stmt) {
    assert(stmt->type == STMT_VARIABLE_DECLARATION);

    struct expr *assignment_expr = stmt->variable_declaration.assignment_expr;
    if (assignment_expr) {
        compile_expr(program, assignment_expr);
        opcode_pop(program, type_size(program, assignment_expr->result_type));
    }
}

static void compile_expr(struct mscript_program *program, struct expr *expr) {
    switch (expr->type) {
        case EXPR_UNARY_OP:
            compile_unary_op_expr(program, expr);
            break;
        case EXPR_BINARY_OP:
            compile_binary_op_expr(program, expr);
            break;
        case EXPR_CALL:
            compile_call_expr(program, expr);
            break;
        case EXPR_DEBUG_PRINT:
            compile_debug_print_expr(program, expr);
            break;
        case EXPR_ARRAY_ACCESS:
            compile_array_access_expr(program, expr);
            break;
        case EXPR_MEMBER_ACCESS:
            compile_member_access_expr(program, expr);
            break;
        case EXPR_ASSIGNMENT:
            compile_assignment_expr(program, expr);
            break;
        case EXPR_INT:
            compile_int_expr(program, expr);
            break;
        case EXPR_FLOAT:
            compile_float_expr(program, expr);
            break;
        case EXPR_SYMBOL:
            compile_symbol_expr(program, expr);
            break;
        case EXPR_STRING:
            compile_string_expr(program, expr);
            break;
        case EXPR_ARRAY:
            compile_array_expr(program, expr);
            break;
        case EXPR_OBJECT:
            compile_object_expr(program, expr);
            break;
        case EXPR_CAST:
            compile_cast_expr(program, expr);
            break;
    }
}

static void compile_lvalue_expr(struct mscript_program *program, struct expr *expr) {
    switch (expr->type) {
        case EXPR_UNARY_OP:
        case EXPR_BINARY_OP:
        case EXPR_CALL:
        case EXPR_DEBUG_PRINT:
        case EXPR_ASSIGNMENT:
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_STRING:
        case EXPR_ARRAY:
        case EXPR_OBJECT:
        case EXPR_CAST:
            assert(false);
            break;
        case EXPR_ARRAY_ACCESS:
            {
                compile_expr(program, expr->array_access.left);
                compile_expr(program, expr->array_access.right);
                opcode_int(program, type_size(program, expr->result_type));
                opcode_imul(program);
            }
            break;
        case EXPR_MEMBER_ACCESS:
            {
                struct mscript_type struct_type = expr->member_access.left->result_type;
                assert(struct_type.type == MSCRIPT_TYPE_STRUCT);

                struct struct_decl *decl = program_get_struct_decl(program, struct_type.struct_name);
                assert(decl);

                bool found_member = false;
                int offset = 0;
                for (int i = 0; i < decl->num_members; i++) {
                    if (strcmp(expr->member_access.member_name, decl->members[i].name) == 0) {
                        found_member = true;
                        break;
                    }
                    offset += type_size(program, decl->members[i].type);
                }
                assert(found_member);

                compile_lvalue_expr(program, expr->member_access.left);

                struct lvalue left_lvalue = expr->member_access.left->lvalue; 
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
        case EXPR_SYMBOL:
            break;
    }
}

static void compile_unary_op_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_UNARY_OP);

    struct expr *operand = expr->unary_op.operand;
    compile_expr(program, operand);

    if (operand->result_type.type == MSCRIPT_TYPE_INT) {
        opcode_iinc(program);
    }
    else if (operand->result_type.type == MSCRIPT_TYPE_FLOAT) {
        opcode_finc(program);
    }
    else {
        assert(false);
    }

    compile_lvalue_expr(program, operand);
    switch (operand->lvalue.type) {
        case LVALUE_LOCAL:
            opcode_local_store(program, operand->lvalue.offset, type_size(program, expr->result_type));
            break;
        case LVALUE_ARRAY:
            opcode_array_store(program, type_size(program, expr->result_type));
            break;
        case LVALUE_INVALID:
            assert(false);
            break;
    }
}

static void compile_binary_op_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_BINARY_OP);

    struct expr *left = expr->binary_op.left;
    struct expr *right = expr->binary_op.right;
    assert(left->result_type.type == right->result_type.type);

    compile_expr(program, left);
    compile_expr(program, right);

    if (left->result_type.type == MSCRIPT_TYPE_INT) {
        switch (expr->binary_op.type) {
            case BINARY_OP_ADD:
                opcode_iadd(program);
                break;
            case BINARY_OP_SUB:
                opcode_isub(program);
                break;
            case BINARY_OP_MUL:
                opcode_imul(program);
                break;
            case BINARY_OP_DIV:
                opcode_idiv(program);
                break;
            case BINARY_OP_LTE:
                opcode_ilte(program);
                break;
            case BINARY_OP_LT:
                opcode_ilt(program);
                break;
            case BINARY_OP_GTE:
                opcode_igte(program);
                break;
            case BINARY_OP_GT:
                opcode_igt(program);
                break;
            case BINARY_OP_EQ:
                opcode_ieq(program);
                break;
            case BINARY_OP_NEQ:
                opcode_ineq(program);
                break;
        }
    }
    else if (left->result_type.type == MSCRIPT_TYPE_FLOAT) {
        switch (expr->binary_op.type) {
            case BINARY_OP_ADD:
                opcode_fadd(program);
                break;
            case BINARY_OP_SUB:
                opcode_fsub(program);
                break;
            case BINARY_OP_MUL:
                opcode_fmul(program);
                break;
            case BINARY_OP_DIV:
                opcode_fdiv(program);
                break;
            case BINARY_OP_LTE:
                opcode_flte(program);
                break;
            case BINARY_OP_LT:
                opcode_flt(program);
                break;
            case BINARY_OP_GTE:
                opcode_fgte(program);
                break;
            case BINARY_OP_GT:
                opcode_fgt(program);
                break;
            case BINARY_OP_EQ:
                opcode_feq(program);
                break;
            case BINARY_OP_NEQ:
                opcode_fneq(program);
                break;
        }
    }
    else {
        assert(false);
    }
}

static void compile_call_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_CALL);
}

static void compile_debug_print_type(struct mscript_program *program, struct mscript_type type) {
    switch (type.type) {
        case MSCRIPT_TYPE_VOID:
            break;
        case MSCRIPT_TYPE_VOID_STAR:
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
        case MSCRIPT_TYPE_STRUCT:
            {
                struct struct_decl *decl = program_get_struct_decl(program, type.struct_name);
                assert(decl);
                int size = decl->size;

                opcode_debug_print_string(program, "{");
                int member_offset = 0;
                for (int i = 0; i < decl->num_members; i++) {
                    struct mscript_type member_type = decl->members[i].type;
                    int member_type_size = type_size(program, member_type);
                    char *member_name = decl->members[i].name;

                    opcode_debug_print_string(program, member_name);
                    opcode_debug_print_string(program, ": ");
                    opcode_copy(program, size - member_offset, member_type_size); 
                    compile_debug_print_type(program, member_type);
                    member_offset += member_type_size;
                    
                    if (i != decl->num_members - 1) {
                        opcode_debug_print_string(program, ", ");
                    }
                }
                opcode_debug_print_string(program, "}");

                opcode_pop(program, size);
            }
            break;
        case MSCRIPT_TYPE_ARRAY:
            break;
        case MSCRIPT_TYPE_STRING:
            break;
    }
}

static void compile_debug_print_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_DEBUG_PRINT);

    int num_args = expr->debug_print.num_args;
    struct expr **args = expr->debug_print.args;
    for (int i = 0; i < num_args; i++) {
        struct expr *arg = args[i];
        struct mscript_type arg_type = arg->result_type;

        compile_expr(program, arg);
        if (arg_type.type == MSCRIPT_TYPE_STRING) {
            opcode_debug_print_string(program, arg->string);
        }
        else {
            compile_debug_print_type(program, arg_type);
        }
    }
}

static void compile_array_access_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_ARRAY_ACCESS);

    struct expr *left = expr->array_access.left;
    struct expr *right = expr->array_access.right;

    assert(left->result_type.type == MSCRIPT_TYPE_ARRAY);
    assert(right->result_type.type == MSCRIPT_TYPE_INT);

    compile_lvalue_expr(program, left);

    struct lvalue lvalue = left->lvalue;
    switch (lvalue.type) {
        case LVALUE_LOCAL:
            opcode_local_load(program, lvalue.offset, type_size(program, left->result_type));
            break;
        case LVALUE_ARRAY:
            opcode_array_load(program, type_size(program, left->result_type));
            break;
        case LVALUE_INVALID:
            assert(false);
            break;
    }

    compile_expr(program, right);
    opcode_int(program, type_size(program, expr->result_type));
    opcode_imul(program);
    opcode_array_load(program, type_size(program, expr->result_type));
}

static void compile_member_access_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_MEMBER_ACCESS);

    struct expr *left = expr->member_access.left;

    if (left->result_type.type == MSCRIPT_TYPE_STRUCT) {
        compile_lvalue_expr(program, expr);
        struct lvalue lvalue = expr->lvalue;
        switch (lvalue.type) {
            case LVALUE_LOCAL:
                opcode_local_load(program, lvalue.offset, type_size(program, expr->result_type));
                break;
            case LVALUE_ARRAY:
                opcode_array_load(program, type_size(program, expr->result_type));
                break;
            case LVALUE_INVALID:
                assert(false);
                break;
        }
    }
    else if (left->result_type.type == MSCRIPT_TYPE_ARRAY) {
        compile_lvalue_expr(program, left);
        struct lvalue lvalue = left->lvalue;
        switch (lvalue.type) {
            case LVALUE_LOCAL:
                opcode_local_load(program, lvalue.offset, type_size(program, left->result_type));
                break;
            case LVALUE_ARRAY:
                opcode_array_load(program, type_size(program, left->result_type));
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

static void compile_assignment_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_ASSIGNMENT);
    assert(types_equal(expr->result_type, expr->assignment.right->result_type));

    struct expr *left = expr->assignment.left;
    struct expr *right = expr->assignment.right;
    struct lvalue lvalue = left->lvalue;

    compile_expr(program, right);
    compile_lvalue_expr(program, left);

    switch (lvalue.type) {
        case LVALUE_LOCAL:
            opcode_local_store(program, lvalue.offset, type_size(program, expr->result_type));
            break;
        case LVALUE_ARRAY:
            opcode_array_store(program, type_size(program, expr->result_type));
            break;
        case LVALUE_INVALID:
            assert(false);
            break;
    }
}

static void compile_int_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_INT);
    opcode_int(program, expr->int_value);
}

static void compile_float_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_FLOAT);
    opcode_float(program, expr->float_value);
}

static void compile_symbol_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_SYMBOL);

    struct lvalue lvalue = expr->lvalue;
    switch (lvalue.type) {
        case LVALUE_INVALID:
        case LVALUE_ARRAY:
            assert(false);
            break;
        case LVALUE_LOCAL:
            opcode_local_load(program, lvalue.offset, type_size(program, expr->result_type));
            break;
    }
}

static void compile_string_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_STRING);
    //opcode_int(program, expr->lvalue);
}

static void compile_array_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_ARRAY);

    struct mscript_type arg_type = expr->result_type;
    assert(arg_type.type == MSCRIPT_TYPE_ARRAY);
    arg_type.type = arg_type.array_type;

    int size = type_size(program, arg_type);
    opcode_array_create(program, size);

    int num_args = expr->array.num_args;
    if (num_args > 0) {
        for (int i = 0; i < expr->array.num_args; i++) {
            struct expr *arg = expr->array.args[i];
            compile_expr(program, arg);
        }

        int result_type_size = type_size(program, expr->result_type);
        opcode_copy(program, num_args * size + result_type_size, result_type_size);
        opcode_int(program, 0);
        opcode_array_store(program, num_args * size);
        opcode_pop(program, num_args * size);
    }
}

static void compile_object_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_OBJECT);

    int num_args = expr->object.num_args;
    for (int i = 0; i < num_args; i++) {
        struct expr *arg = expr->object.args[i];
        compile_expr(program, arg);
    }
}

static void compile_cast_expr(struct mscript_program *program, struct expr *expr) {
    assert(expr->type == EXPR_CAST);

    struct mscript_type cast_type = expr->cast.type;
    struct mscript_type arg_type = expr->cast.arg->result_type;
    struct expr *arg = expr->cast.arg;

    compile_expr(program, arg);

    if (cast_type.type == MSCRIPT_TYPE_INT && arg_type.type == MSCRIPT_TYPE_INT) {
    }
    else if (cast_type.type == MSCRIPT_TYPE_FLOAT && arg_type.type == MSCRIPT_TYPE_FLOAT) {
    }
    else if (cast_type.type == MSCRIPT_TYPE_FLOAT && arg_type.type == MSCRIPT_TYPE_INT) {
        opcode_i2f(program);
    }
    else if (cast_type.type == MSCRIPT_TYPE_INT && arg_type.type == MSCRIPT_TYPE_FLOAT) {
        opcode_f2i(program);
    }
    else {
        assert(false);
    }
}

static void vm_init(struct mscript_program *program) {
    struct vm *vm = &program->vm;
    vm->ip = 0;
    array_init(&vm->ip_stack);
    vm->fp = 0;
    array_init(&vm->fp_stack);
    array_init(&vm->stack);
    array_init(&vm->arrays);

    array_push(&vm->ip_stack, -1);
    array_push(&vm->fp_stack, 0);
}

static void vm_run(struct mscript_program *program, struct array_opcode opcodes) {
    struct vm *vm = &program->vm;

    while (true) {
        if (vm->ip == -1) {
            break;
        }

        struct opcode op = opcodes.data[vm->ip];
        switch (op.type) {
            case OPCODE_IADD:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 + *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FADD:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    float v = *v0 + *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_ISUB:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 - *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FSUB:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    float v = *v0 - *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_IMUL:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 * *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FMUL:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    float v = *v0 * *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_IDIV:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 / *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FDIV:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    float v = *v0 / *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_ILTE:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 <= *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FLTE:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = *v0 <= *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_ILT:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 < *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FLT:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = *v0 < *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_IGTE:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 >= *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FGTE:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = *v0 >= *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_IGT:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 > *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FGT:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = *v0 > *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_IEQ:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 == *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FEQ:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = *v0 == *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_INEQ:
                {
                    int *v1 = (int*) vm_pop(program, 4);
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = *v0 != *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FNEQ:
                {
                    float *v1 = (float*) vm_pop(program, 4);
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = *v0 != *v1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_IINC:
                {
                    int *v0 = (int*) vm_pop(program, 4);
                    int v = (*v0) + 1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FINC:
                {
                    float *v0 = (float*) vm_pop(program, 4);
                    float v = (*v0) + 1;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_F2I:
                {
                    float *v0 = (float*) vm_pop(program, 4);
                    int v = (int) *v0;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_I2F:
                {
                    int *v0 = (int*) vm_pop(program, 4);
                    float v = (float) *v0;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_COPY:
                {
                    int start = vm->stack.length - op.load_store.offset; 
                    assert((start >= 0) && (start + op.load_store.size <= vm->stack.length));
                    for (int i = 0; i < op.load_store.size; i++) {
                        char data = vm->stack.data[start + i];
                        vm_push(program, &data, 1);
                    }
                }
                break;
            case OPCODE_INT:
                {
                    int v = op.int_val;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_FLOAT:
                {
                    float v = op.float_val;
                    vm_push(program, (char*) (&v), 4);
                }
                break;
            case OPCODE_LOCAL_STORE:
                {
                    int offset = op.load_store.offset;
                    int size = op.load_store.size;
                    assert(vm->fp + offset < vm->stack.length);
                    char *src = vm->stack.data + vm->stack.length - size;
                    char *dest = vm->stack.data + vm->fp + offset;
                    memcpy(dest, src, size);
                }
                break;
            case OPCODE_LOCAL_LOAD:
                {
                    int offset = op.load_store.offset;
                    int size = op.load_store.size;
                    assert(vm->fp + offset < vm->stack.length);
                    array_reserve(&vm->stack, vm->stack.length + size);
                    char *data = vm->stack.data + vm->fp + offset;
                    vm_push(program, data, size);
                }
                break;
            case OPCODE_JF:
                {
                    int *v = (int*) vm_pop(program, 4);
                    if (!(*v)) {
                        bool found_label = false;
                        for (int i = 0; i < opcodes.length; i++) {
                            if (opcodes.data[i].type == OPCODE_LABEL && opcodes.data[i].label == op.label) {
                                vm->ip = i;
                                found_label = true;
                                break;
                            }
                        }
                        assert(found_label);
                    }
                }
                break;
            case OPCODE_JMP:
                {
                    bool found_label = false;
                    for (int i = 0; i < opcodes.length; i++) {
                        if (opcodes.data[i].type == OPCODE_LABEL && opcodes.data[i].label == op.label) {
                            vm->ip = i;
                            found_label = true;
                            break;
                        }
                    }
                    assert(found_label);
                }
                break;
            case OPCODE_CALL:
                {
                    assert(false);
                }
                break;
            case OPCODE_RETURN:
                {
                    vm->ip = -2;
                }
                break;
            case OPCODE_PUSH:
                {
                    int size = op.size;
                    vm_push_val(program, 0, size);
                }
                break;
            case OPCODE_POP:
                {
                    vm_pop(program, op.size);
                }
                break;
            case OPCODE_ARRAY_CREATE:
                {
                    int size = op.size;
                    int array_idx = vm->arrays.length;

                    struct vm_array new_array;
                    new_array.member_size = size;
                    array_init(&new_array.array);
                    array_push(&vm->arrays, new_array);
                    vm_push(program, (char*) (&array_idx), 4);
                }
                break;
            case OPCODE_ARRAY_STORE:
                {
                    int size = op.size;

                    int *offset = (int*) vm_pop(program, 4);
                    int *array_idx = (int*) vm_pop(program, 4);
                    char *data = vm->stack.data + vm->stack.length - size;
                    struct vm_array *array = vm->arrays.data + (*array_idx);

                    int reserve_size = (*offset) + size;
                    if ((reserve_size % array->member_size) != 0) {
                        reserve_size = array->member_size * ((int) (reserve_size / array->member_size) + 1);
                    }

                    array_reserve(&array->array, reserve_size);
                    if (array->array.length < reserve_size) {
                        array->array.length = reserve_size;
                    }
                    
                    for (int i = 0; i < size; i++) {
                        array->array.data[(*offset) + i] = data[i];
                    }
                }
                break;
            case OPCODE_ARRAY_LOAD:
                {
                    int size = op.size;

                    int *offset = (int*) vm_pop(program, 4);
                    int *array_idx = (int*) vm_pop(program, 4);
                    struct vm_array *array = vm->arrays.data + (*array_idx);

                    assert((*offset) + size <= array->array.length);
                    vm_push(program, array->array.data + (*offset), size);
                }
                break;
            case OPCODE_ARRAY_LENGTH:
                {
                    int *array_idx = (int*) vm_pop(program, 4);
                    struct vm_array *array = vm->arrays.data + (*array_idx);

                    int len = array->array.length / array->member_size;
                    vm_push(program, (char*) (&len), 4);
                }
                break;
            case OPCODE_LABEL:
                {
                }
                break;
            case OPCODE_DEBUG_PRINT_INT:
                {
                    int *v = (int*) vm_pop(program, 4);
                    m_logf("%d", *v);
                }
                break;
            case OPCODE_DEBUG_PRINT_FLOAT:
                {
                    float *v = (float*) vm_pop(program, 4);
                    m_logf("%f", *v);
                }
                break;
            case OPCODE_DEBUG_PRINT_STRING:
                {
                    m_logf(op.string);
                }
                break;
        }
        (vm->ip)++;
    }
}

static void vm_push(struct mscript_program *program, char *v, int n) {
    struct vm *vm = &program->vm;
    array_pusharr(&vm->stack, v, n);
}

static void vm_push_val(struct mscript_program *program, char v, int n) {
    struct vm *vm = &program->vm;
    for (int i = 0; i < n; i++) {
        array_push(&vm->stack, v);
    }
}

static char *vm_pop(struct mscript_program *program, int n) {
    struct vm *vm = &program->vm;
    if (vm->stack.length < n) {
        assert(false);
    }
    vm->stack.length -= n;
    return vm->stack.data + vm->stack.length;
}

static void debug_log_token(struct token token) {
    switch (token.type) {
        case TOKEN_INT:
            m_logf("[INT: %d]\n", token.int_value);
            break;
        case TOKEN_FLOAT:
            m_logf("[FLOAT: %d]\n", token.float_value);
            break;
        case TOKEN_STRING:
            m_logf("[STRING: %s]\n", token.string);
            break;
        case TOKEN_SYMBOL:
            m_logf("[SYMBOL: %s]\n", token.symbol);
            break;
        case TOKEN_CHAR:
            m_logf("[CHAR: %c]\n", token.char_value);
            break;
        case TOKEN_EOF:
            m_logf("[EOF]\n");
            return;
            break;
    }
}

static void debug_log_tokens(struct token *tokens) {
    while (tokens->type != TOKEN_EOF) {
        debug_log_token(*tokens);
        tokens++;
    }
}

static void debug_log_type(struct mscript_type type) {
    enum mscript_type_type t = type.type;
    if (type.type == MSCRIPT_TYPE_ARRAY) {
        t = type.array_type;
    }

    switch (t) {
        case MSCRIPT_TYPE_VOID:
            m_logf("void");
            break;
        case MSCRIPT_TYPE_VOID_STAR:
            m_logf("void*");
            break;
        case MSCRIPT_TYPE_INT:
            m_logf("int");
            break;
        case MSCRIPT_TYPE_FLOAT:
            m_logf("float");
            break;
        case MSCRIPT_TYPE_STRUCT:
            m_logf("%s", type.struct_name);
            break;
        case MSCRIPT_TYPE_STRING:
            m_logf("char*");
            break;
        case MSCRIPT_TYPE_ARRAY:
            assert(false);
            break;
    }

    if (type.type == MSCRIPT_TYPE_ARRAY) {
        m_logf("[]");
    }
}

static void debug_log_stmt(struct stmt *stmt) {
    switch (stmt->type) {
        case STMT_IF:
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
        case STMT_FOR:
            m_logf("for (");
            debug_log_expr(stmt->for_stmt.init);
            m_logf(";");
            debug_log_expr(stmt->for_stmt.cond);
            m_logf(";");
            debug_log_expr(stmt->for_stmt.inc);
            m_logf(") ");
            debug_log_stmt(stmt->for_stmt.body);
            break;
        case STMT_RETURN:
            m_logf("return ");
            if (stmt->return_stmt.expr) {
                debug_log_expr(stmt->return_stmt.expr);
            }
            m_logf(";\n");
            break;
        case STMT_BLOCK:
            m_logf("{\n");
            for (int i = 0; i < stmt->block.num_stmts; i++) {
                debug_log_stmt(stmt->block.stmts[i]);
            }
            m_logf("}\n");
            break;
        case STMT_FUNCTION_DECLARATION:
            debug_log_type(stmt->function_declaration.return_type);
            m_logf(" %s(", stmt->function_declaration.name);
            for (int i = 0; i < stmt->function_declaration.num_args; i++) {
                debug_log_type(stmt->function_declaration.arg_types[i]);
                m_logf(" %s", stmt->function_declaration.arg_names[i]);
                if (i != stmt->function_declaration.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf(")");
            debug_log_stmt(stmt->function_declaration.body);
            break;
        case STMT_VARIABLE_DECLARATION:
            debug_log_type(stmt->variable_declaration.type);
            m_logf(" %s;\n", stmt->variable_declaration.name);

            if (stmt->variable_declaration.assignment_expr) {
                debug_log_expr(stmt->variable_declaration.assignment_expr);
                m_logf(";\n");
            }
            break;
        case STMT_EXPR:
            debug_log_expr(stmt->expr);
            m_logf(";\n");
            break;
        case STMT_STRUCT_DECLARATION:
            m_logf("struct %s {\n", stmt->struct_declaration.name);
            for (int i = 0; i < stmt->struct_declaration.num_members; i++) {
                debug_log_type(stmt->struct_declaration.member_types[i]);
                m_logf(" %s;\n", stmt->struct_declaration.member_names[i]);
            }
            m_logf("}\n");
            break;
        case STMT_IMPORT:
            m_logf("import \"%s\"\n", stmt->import.program_name);
            break;
        case STMT_IMPORT_FUNCTION:
            m_logf("import_function %s();\n", stmt->import_function.name);
            break;
    }
}

static void debug_log_expr(struct expr *expr) {
    switch (expr->type) {
        case EXPR_CAST:
            m_log("((");
            debug_log_type(expr->cast.type);
            m_log(")");
            debug_log_expr(expr->cast.arg);
            m_log(")");
            break;
        case EXPR_ARRAY_ACCESS:
            m_log("(");
            debug_log_expr(expr->array_access.left);
            m_log("[");
            debug_log_expr(expr->array_access.right);
            m_log("]");
            m_log(")");
            break;
        case EXPR_MEMBER_ACCESS:
            m_log("(");
            debug_log_expr(expr->member_access.left);
            m_logf(".%s)", expr->member_access.member_name);
            m_log(")");
            break;
        case EXPR_ASSIGNMENT:
            m_log("(");
            debug_log_expr(expr->assignment.left);
            m_log("=");
            debug_log_expr(expr->assignment.right);
            m_log(")");
            break;
        case EXPR_UNARY_OP:
            switch (expr->unary_op.type) {
                case UNARY_OP_POST_INC:
                    m_log("(");
                    debug_log_expr(expr->unary_op.operand);
                    m_log(")++");
                    break;
            }
            break;
        case EXPR_BINARY_OP:
            m_logf("(");
            debug_log_expr(expr->binary_op.left);
            switch (expr->binary_op.type) {
                case BINARY_OP_ADD:
                    m_log("+");
                    break;
                case BINARY_OP_SUB:
                    m_log("-");
                    break;
                case BINARY_OP_MUL:
                    m_log("*");
                    break;
                case BINARY_OP_DIV:
                    m_log("/");
                    break;
                case BINARY_OP_LTE:
                    m_log("<=");
                    break;
                case BINARY_OP_LT:
                    m_log("<");
                    break;
                case BINARY_OP_GTE:
                    m_log(">=");
                    break;
                case BINARY_OP_GT:
                    m_log(">");
                    break;
                case BINARY_OP_EQ:
                    m_log("==");
                    break;
                case BINARY_OP_NEQ:
                    m_log("!=");
                    break;
            }
            debug_log_expr(expr->binary_op.right);
            m_logf(")");
            break;
        case EXPR_INT:
            m_logf("%d", expr->int_value);
            break;
            break;
        case EXPR_FLOAT:
            m_logf("%f", expr->float_value);
            break;
        case EXPR_SYMBOL:
            m_logf("%s", expr->symbol);
            break;
        case EXPR_STRING:
            m_logf("\"%s\"", expr->string);
            break;
        case EXPR_ARRAY:
            m_logf("[");
            for (int i = 0; i < expr->array.num_args; i++) {
                debug_log_expr(expr->array.args[i]);
                if (i != expr->array.num_args - 1) {
                    m_logf(", ");
                }
            }
            m_logf("]");
            break;
        case EXPR_OBJECT:
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
        case EXPR_CALL:
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
        case EXPR_DEBUG_PRINT:
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
                m_logf("CALL %d", op.function);
                break;
            case OPCODE_RETURN:
                m_logf("RETURN %d", op.size);
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
            case OPCODE_LABEL:
                m_logf("LABEL %d", op.label);
                break;
            case OPCODE_DEBUG_PRINT_INT:
                m_logf("DEBUG_PRINT_INT");
                break;
            case OPCODE_DEBUG_PRINT_FLOAT:
                m_logf("DEBUG_PRINT_FLOAT");
                break;
            case OPCODE_DEBUG_PRINT_STRING:
                m_logf("DEBUG_PRINT_STRING: %s", op.string);
                break;
        }
        m_logf("\n");
    }
}

struct mscript *mscript_create(void) {
    struct mscript *mscript = malloc(sizeof(struct mscript));
    map_init(&mscript->map);
    return mscript;
}

struct mscript_program *mscript_load_program(struct mscript *mscript, const char *name) {
    struct mscript_program **cached_program = map_get(&mscript->map, name);
    if (cached_program) {
        return *cached_program;
    }

    struct file file = file_init(name);
    if (!file_load_data(&file)) {
        return NULL;
    }

    struct mscript_program *program = malloc(sizeof(struct mscript_program));
    map_set(&mscript->map, name, program);
    program_init(program, mscript, file.data);
    file_delete_data(&file);
    if (program->error) {
        struct token tok = program->error_token;
        int line = tok.line;
        int col = tok.col;

        m_logf("ERROR: %s. Line %d. Col %d.\n", name, line, col); 
        m_logf("%s\n", program->error);
    }
    return program;
}
