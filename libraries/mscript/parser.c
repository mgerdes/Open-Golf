#define _CRT_SECURE_NO_WARNINGS
#include "mscript/parser.h"

#include <assert.h>
#include <stdbool.h>

#include "array.h"
#include "log.h"

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

static bool is_char_digit(char c);
static bool is_char_start_of_symbol(char c);
static bool is_char_part_of_symbol(char c);
static bool is_char(char c);
static struct token number_token(const char *text, int *len, int line, int col);
static struct token char_token(char c, int line, int col);
static struct token string_token(const char *text, int *len, int line, int col);
static struct token symbol_token(const char *text, int *len, int line, int col);
static struct token eof_token(int line, int col);
static void tokenize(struct parser *parser);

struct parser_allocator {
    size_t bytes_allocated;
    struct array_void_ptr ptrs;
};

static void parser_allocator_init(struct parser_allocator *allocator);
static void parser_allocator_deinit(struct parser_allocator *allocator);
static void *parser_allocator_alloc(struct parser_allocator *allocator, size_t size);

struct parser {
    const char *prog_text;

    struct parser_allocator allocator;

    int token_idx;
    struct array_token tokens;

    char *error;
    struct token error_token;
};

static void parser_init(struct parser *parser, const char *prog_text);
static void parser_deinit(struct parser *parser);
static void parser_error(struct parser *parser, char *fmt, ...);
static void parser_error_token(struct parser *parser, struct token token, char *fmt, ...);
static struct token peek(struct parser *parser);
static struct token peek_n(struct parser *parser, int n);
static void eat(struct parser *parser); 
static bool match_char(struct parser *parser, char c);
static bool match_char_n(struct parser *parser, int n, ...);
static bool match_symbol(struct parser *parser, const char *symbol);
static bool match_symbol_n(struct parser *parser, int n, ...);
static bool match_eof(struct parser *parser);
static bool check_type(struct parser *parser);

enum type_type {
    TYPE_VOID,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRUCT,
    TYPE_ARRAY,
};

struct type {
    enum type_type type;

    enum type_type array_type;
    char *struct_name; 
};
array_t(struct type, array_type)

static struct type void_type(void);
static struct type int_type(void);
static struct type float_type(void);
static struct type struct_type(char *struct_name);
static struct type array_type(enum type_type array_type, char *struct_name);

enum expr_type {
    EXPR_UNARY_OP,
    EXPR_BINARY_OP,
    EXPR_CALL,
    EXPR_ARRAY_LENGTH,
    EXPR_ARRAY_ACCESS,
    EXPR_MEMBER_ACCESS,
    EXPR_ASSIGNMENT,
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_SYMBOL,
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
            struct expr *left;
        } array_length;

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
        } array;

        struct {
            int num_args;
            char **names;
            struct expr **args;
        } object;

        struct {
            struct expr *arg;
        } cast;

        int int_value;
        float float_value;
        char *symbol;
    };
};
array_t(struct expr *, array_expr_ptr)

static struct expr *new_unary_op_expr(struct parser_allocator *allocator, enum unary_op_type type, struct expr *operand);
static struct expr *new_binary_op_expr(struct parser_allocator *allocator, enum binary_op_type type, struct expr *left, struct expr *right);
static struct expr *new_assignment_expr(struct parser_allocator *allocator, struct expr *left, struct expr *right);
static struct expr *new_array_access_expr(struct parser_allocator *allocator, struct expr *left, struct expr *right);
static struct expr *new_member_access_expr(struct parser_allocator *allocator, struct expr *left, char *member_name);
static struct expr *new_call_expr(struct parser_allocator *allocator, struct expr *function, struct array_expr_ptr args);
static struct expr *new_array_expr(struct parser_allocator *allocator, struct array_expr_ptr args);
static struct expr *new_object_expr(struct parser_allocator *allocator, struct array_char_ptr names, struct array_expr_ptr args);
static struct expr *new_cast_expr(struct parser_allocator *allocator, struct expr *expr);
static struct expr *new_int_expr(struct parser_allocator *allocator, int int_value);
static struct expr *new_float_expr(struct parser_allocator *allocator, float float_value);
static struct expr *new_symbol_expr(struct parser_allocator *allocator, char *symbol);

enum stmt_type {
    STMT_IF,
    STMT_RETURN,
    STMT_BLOCK,
    STMT_FUNCTION_DECLARATION,
    STMT_VARIABLE_DECLARATION,
    STMT_STRUCT_DECLARATION,
    STMT_IMPORT,
    STMT_EXPR,
    STMT_FOR,
};

struct stmt {
    enum stmt_type type;

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
            struct type return_type;
            char *name;
            int num_args;
            struct type *arg_types;
            char **arg_names;
            struct stmt *body;
        } function_declaration;

        struct {
            struct type type;
            char *name;
            struct expr *expr;
        } variable_declaration;

        struct {
            char *name;
            int num_members;
            struct type *member_types;
            char **member_names;
        } struct_declaration;

        struct {
            struct expr *init, *cond, *inc;
            struct stmt *body;
        } for_stmt;

        struct {
            char *module_name;
        } import;

        struct expr *expr;
    };
};
array_t(struct stmt *, array_stmt_ptr)

static struct stmt *new_if_stmt(struct parser_allocator *allocator, struct array_expr_ptr conds, struct array_stmt_ptr stmts, struct stmt *else_stmt);
static struct stmt *new_return_stmt(struct parser_allocator *allocator, struct expr *expr);
static struct stmt *new_block_stmt(struct parser_allocator *allocator, struct array_stmt_ptr stmts);
static struct stmt *new_function_declaration_stmt(struct parser_allocator *allocator, struct type return_type, char *name, 
        struct array_type arg_types, struct array_char_ptr arg_names, struct stmt *body);
static struct stmt *new_variable_declaration_stmt(struct parser_allocator *allocator, struct type type, char *name, struct expr *expr);
static struct stmt *new_struct_declaration_stmt(struct parser_allocator *allocator, char *name, 
        struct array_type member_types, struct array_char_ptr member_names);
static struct stmt *new_for_stmt(struct parser_allocator *allocator, struct expr *init, struct expr *cond, struct expr *inc, struct stmt *body);
static struct stmt *new_import_stmt(struct parser_allocator *allocator, char *module_name);
static struct stmt *new_expr_stmt(struct parser_allocator *allocator, struct expr *expr);

static void parse_type(struct parser *parser, struct type *type);  

static struct expr *parse_expr(struct parser *parser);
static struct expr *parse_assignment_expr(struct parser *parser);
static struct expr *parse_comparison_expr(struct parser *parser);
static struct expr *parse_term_expr(struct parser *parser);
static struct expr *parse_factor_expr(struct parser *parser);
static struct expr *parse_unary_expr(struct parser *parser);
static struct expr *parse_member_access_expr(struct parser *parser);
static struct expr *parse_array_access_expr(struct parser *parser);
static struct expr *parse_call_expr(struct parser *parser);
static struct expr *parse_primary_expr(struct parser *parser);
static struct expr *parse_array_expr(struct parser *parser);
static struct expr *parse_object_expr(struct parser *parser);

static struct stmt *parse_stmt(struct parser *parser);
static struct stmt *parse_if_stmt(struct parser *parser);
static struct stmt *parse_block_stmt(struct parser *parser);
static struct stmt *parse_for_stmt(struct parser *parser);
static struct stmt *parse_return_stmt(struct parser *parser);
static struct stmt *parse_variable_declaration_stmt(struct parser *parser);
static struct stmt *parse_function_declaration_stmt(struct parser *parser);
static struct stmt *parse_struct_declaration_stmt(struct parser *parser);
static struct stmt *parse_import_stmt(struct parser *parser);

static void debug_log_token(struct token token);
static void debug_log_tokens(struct token *tokens);
static void debug_log_type(struct type type);
static void debug_log_stmt(struct stmt *stmt);
static void debug_log_expr(struct expr *expr);

//
// DEFINITIONS
//

static struct type void_type(void) {
    struct type type;
    type.type = TYPE_VOID;
    return type;
}

static struct type int_type(void) {
    struct type type;
    type.type = TYPE_INT;
    return type;
}

static struct type float_type(void) {
    struct type type;
    type.type = TYPE_FLOAT;
    return type;
}

static struct type struct_type(char *struct_name) {
    struct type type;
    type.type = TYPE_STRUCT;
    type.struct_name = struct_name;
    return type;
}

static struct type array_type(enum type_type array_type, char *struct_name) {
    struct type type;
    type.type = TYPE_ARRAY;
    type.array_type = array_type;
    type.struct_name = struct_name;
    return type;
}

static struct expr *new_unary_op_expr(struct parser_allocator *allocator, enum unary_op_type type, struct expr *operand) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_UNARY_OP;
    expr->unary_op.type = type;
    expr->unary_op.operand = operand;
    return expr;
}

static struct expr *new_binary_op_expr(struct parser_allocator *allocator, enum binary_op_type type, struct expr *left, struct expr *right) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_BINARY_OP;
    expr->binary_op.type = type;
    expr->binary_op.left = left;
    expr->binary_op.right = right;
    return expr;
}

static struct expr *new_assignment_expr(struct parser_allocator *allocator, struct expr *left, struct expr *right) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_ASSIGNMENT;
    expr->assignment.left = left;
    expr->assignment.right = right;
    return expr;
}

static struct expr *new_array_access_expr(struct parser_allocator *allocator, struct expr *left, struct expr *right) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_ARRAY_ACCESS;
    expr->array_access.left = left;
    expr->array_access.right = right;
    return expr;
}

static struct expr *new_member_access_expr(struct parser_allocator *allocator, struct expr *left, char *member_name) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_MEMBER_ACCESS;
    expr->member_access.left = left;
    expr->member_access.member_name = member_name;
    return expr;
}

static struct expr *new_call_expr(struct parser_allocator *allocator, struct expr *function, struct array_expr_ptr args) {
    int num_args = args.length;

    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_CALL;
    expr->call.function = function;
    expr->call.num_args = num_args;
    expr->call.args = parser_allocator_alloc(allocator, num_args * sizeof(struct expr*));
    memcpy(expr->call.args, args.data, num_args * sizeof(struct expr*));
    return expr;
}

static struct expr *new_array_expr(struct parser_allocator *allocator, struct array_expr_ptr args) {
    int num_args = args.length;

    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_ARRAY;
    expr->array.num_args = num_args;
    expr->array.args = parser_allocator_alloc(allocator, num_args * sizeof(struct expr*));
    memcpy(expr->array.args, args.data, num_args * sizeof(struct expr*));
    return expr;
}

static struct expr *new_object_expr(struct parser_allocator *allocator, struct array_char_ptr names, struct array_expr_ptr args) {
    assert(names.length == args.length);
    int num_args = args.length;

    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_OBJECT;
    expr->object.num_args = num_args;
    expr->object.names = parser_allocator_alloc(allocator, num_args * sizeof(char *));
    memcpy(expr->object.names, names.data, num_args * sizeof(char *));
    expr->object.args = parser_allocator_alloc(allocator, num_args * sizeof(struct expr *));
    memcpy(expr->object.args, args.data, num_args * sizeof(struct expr *));
    return expr;
}

static struct expr *new_cast_expr(struct parser_allocator *allocator, struct expr *arg) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_CAST;
    expr->cast.arg = arg;
    return expr;
}

static struct expr *new_int_expr(struct parser_allocator *allocator, int int_value) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_INT;
    expr->int_value = int_value;
    return expr;
}

static struct expr *new_float_expr(struct parser_allocator *allocator, float float_value) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_FLOAT;
    expr->float_value = float_value;
    return expr;
}

static struct expr *new_symbol_expr(struct parser_allocator *allocator, char *symbol) {
    struct expr *expr = parser_allocator_alloc(allocator, sizeof(struct expr));
    expr->type = EXPR_SYMBOL;
    expr->symbol = symbol;
    return expr;
}

static struct stmt *new_if_stmt(struct parser_allocator *allocator, struct array_expr_ptr conds, struct array_stmt_ptr stmts, struct stmt *else_stmt) {
    assert(conds.length == stmts.length);
    int num_stmts = conds.length;

    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_IF;
    stmt->if_stmt.num_stmts = num_stmts;
    stmt->if_stmt.conds = parser_allocator_alloc(allocator, num_stmts * sizeof(struct expr *));
    memcpy(stmt->if_stmt.conds, conds.data, num_stmts * sizeof(struct expr *));
    stmt->if_stmt.stmts = parser_allocator_alloc(allocator, num_stmts * sizeof(struct stmt *));
    memcpy(stmt->if_stmt.stmts, stmts.data, num_stmts * sizeof(struct stmt *));
    stmt->if_stmt.else_stmt = else_stmt;
    return stmt;
}

static struct stmt *new_return_stmt(struct parser_allocator *allocator, struct expr *expr) {
    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_RETURN;
    stmt->return_stmt.expr = expr;
    return stmt;
}

static struct stmt *new_block_stmt(struct parser_allocator *allocator, struct array_stmt_ptr stmts) {
    int num_stmts = stmts.length;

    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_BLOCK;
    stmt->block.num_stmts = num_stmts;
    stmt->block.stmts = parser_allocator_alloc(allocator, num_stmts * sizeof(struct stmt *));
    memcpy(stmt->block.stmts, stmts.data, num_stmts * sizeof(struct stmt *));
    return stmt;
}

static struct stmt *new_function_declaration_stmt(struct parser_allocator *allocator, struct type return_type, char *name, 
        struct array_type arg_types, struct array_char_ptr arg_names, struct stmt *body) {
    assert(arg_types.length == arg_names.length);
    int num_args = arg_types.length;

    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_FUNCTION_DECLARATION;
    stmt->function_declaration.return_type = return_type;
    stmt->function_declaration.name = name;
    stmt->function_declaration.num_args = num_args;
    stmt->function_declaration.arg_types = parser_allocator_alloc(allocator, num_args * sizeof(struct type));
    memcpy(stmt->function_declaration.arg_types, arg_types.data, num_args * sizeof(struct type));
    stmt->function_declaration.arg_names = parser_allocator_alloc(allocator, num_args * sizeof(char *));
    memcpy(stmt->function_declaration.arg_names, arg_names.data, num_args * sizeof(char *));
    stmt->function_declaration.body = body;
    return stmt;
}

static struct stmt *new_variable_declaration_stmt(struct parser_allocator *allocator, struct type type, char *name, struct expr *expr) {
    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_VARIABLE_DECLARATION;
    stmt->variable_declaration.type = type;
    stmt->variable_declaration.name = name;
    stmt->variable_declaration.expr = expr;
    return stmt;
}

static struct stmt *new_struct_declaration_stmt(struct parser_allocator *allocator, char *name, 
        struct array_type member_types, struct array_char_ptr member_names) {
    assert(member_types.length == member_names.length);
    int num_members = member_types.length;

    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_STRUCT_DECLARATION;
    stmt->struct_declaration.name = name;
    stmt->struct_declaration.num_members = num_members;
    stmt->struct_declaration.member_types = parser_allocator_alloc(allocator, num_members * sizeof(struct type));
    memcpy(stmt->struct_declaration.member_types, member_types.data, num_members * sizeof(struct type));
    stmt->struct_declaration.member_names = parser_allocator_alloc(allocator, num_members * sizeof(char *));
    memcpy(stmt->struct_declaration.member_names, member_names.data, num_members * sizeof(char *));
    return stmt;
}

static struct stmt *new_for_stmt(struct parser_allocator *allocator, struct expr *init, struct expr *cond, struct expr *inc, struct stmt *body) {
    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_FOR;
    stmt->for_stmt.init = init;
    stmt->for_stmt.cond = cond;
    stmt->for_stmt.inc = inc;
    stmt->for_stmt.body = body;
    return stmt;
}

static struct stmt *new_import_stmt(struct parser_allocator *allocator, char *module_name) {
    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_IMPORT;
    stmt->import.module_name = module_name;
    return stmt;
}

static struct stmt *new_expr_stmt(struct parser_allocator *allocator, struct expr *expr) {
    struct stmt *stmt = parser_allocator_alloc(allocator, sizeof(struct stmt));
    stmt->type = STMT_EXPR;
    stmt->expr = expr;
    return stmt;
}

static void parse_type(struct parser *parser, struct type *type) {
    if (match_symbol(parser, "void")) {
        *type = void_type();
    }
    else if (match_symbol(parser, "int")) {
        *type = int_type();
    }
    else if (match_symbol(parser, "float")) {
        *type = float_type();
    }
    else {
        struct token tok = peek(parser);
        if (tok.type != TOKEN_SYMBOL) {
            parser_error(parser, "Expected symbol");
            return;
        }
        eat(parser);

        *type = struct_type(tok.symbol);
    }

    if (match_char_n(parser, 2, '[', ']')) {
        *type = array_type(type->type, type->struct_name);
    }
}

static struct expr *parse_object_expr(struct parser *parser) {
    struct array_char_ptr names;
    struct array_expr_ptr args;
    array_init(&names);
    array_init(&args);

    struct expr *expr = NULL;

    if (!match_char(parser, '}')) {
        while (true) {
            struct token tok = peek(parser);
            if (tok.type != TOKEN_SYMBOL) {
                parser_error(parser, "Expected symbol"); 
                goto cleanup;
            }
            array_push(&names, tok.symbol);
            eat(parser);

            if (!match_char(parser, '=')) {
                parser_error(parser, "Expected '='");
                goto cleanup;
            }

            struct expr *arg = parse_expr(parser);
            if (parser->error) goto cleanup;
            array_push(&args, arg);

            if (!match_char(parser, ',')) {
                if (!match_char(parser, '}')) {
                    parser_error(parser, "Expected '}'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = new_object_expr(&parser->allocator, names, args);

cleanup:
    array_deinit(&names);
    array_deinit(&args);
    return expr;
}

static struct expr *parse_array_expr(struct parser *parser) {
    struct array_expr_ptr args;
    array_init(&args);

    struct expr *expr = NULL;
    if (!match_char(parser, ']')) {
        while (true) {
            struct expr *arg = parse_expr(parser);
            if (parser->error) goto cleanup;
            array_push(&args, arg);

            if (!match_char(parser, ',')) {
                if (!match_char(parser, ']')) {
                    parser_error(parser, "Expected ']'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = new_array_expr(&parser->allocator, args);

cleanup:
    array_deinit(&args);
    return expr;
}

static struct expr *parse_primary_expr(struct parser *parser) {
    struct token tok = peek(parser);
    struct expr *expr = NULL;

    if (tok.type == TOKEN_INT) {
        expr = new_int_expr(&parser->allocator, tok.int_value);
        eat(parser);
    }
    else if (tok.type == TOKEN_FLOAT) {
        expr = new_float_expr(&parser->allocator, tok.float_value);
        eat(parser);
    }
    else if (tok.type == TOKEN_SYMBOL) {
        expr = new_symbol_expr(&parser->allocator, tok.symbol);
        eat(parser);
    }
    else if (match_char(parser, '[')) {
        expr = parse_array_expr(parser);
    }
    else if (match_char(parser, '{')) {
        expr = parse_object_expr(parser);
    }
    else {
        parser_error(parser, "Unknown token");
        goto cleanup;
    }

cleanup:
    return expr;
}

static struct expr *parse_call_expr(struct parser *parser) {
    struct array_expr_ptr args;
    array_init(&args);

    struct expr *expr = parse_primary_expr(parser);
    if (parser->error) goto cleanup;

    if (match_char(parser, '(')) {
        if (!match_char(parser, ')')) {
            while (true) {
                struct expr *arg = parse_expr(parser);
                if (parser->error) goto cleanup;
                array_push(&args, arg);

                if (!match_char(parser, ',')) {
                    if (!match_char(parser, ')')) {
                        parser_error(parser, "Expected ')'");
                        goto cleanup;
                    }
                    break;
                }
            }
        }

        expr = new_call_expr(&parser->allocator, expr, args);
    }

cleanup:
    array_deinit(&args);
    return expr;
}

static struct expr *parse_array_access_expr(struct parser *parser) {
    struct expr *expr = parse_call_expr(parser);
    if (parser->error) goto cleanup;

    if (match_char(parser, '[')) {
        struct expr *right = parse_expr(parser);
        if (parser->error) goto cleanup;
        expr = new_array_access_expr(&parser->allocator, expr, right);

        if (!match_char(parser, ']')) {
            parser_error(parser, "Expected ']'");
            goto cleanup;
        }
    }

cleanup:
    return expr;
}

static struct expr *parse_member_access_expr(struct parser *parser) {
    struct expr *expr = parse_array_access_expr(parser);
    if (parser->error) goto cleanup;

    while (match_char(parser, '.')) {
        struct token tok = peek(parser);
        if (tok.type != TOKEN_SYMBOL) {
            parser_error(parser, "Expected symbol token");
            goto cleanup;
        }
        eat(parser);

        expr = new_member_access_expr(&parser->allocator, expr, tok.symbol);
    }

cleanup:
    return expr;
}

static struct expr *parse_unary_expr(struct parser *parser) {
    struct expr *expr = parse_member_access_expr(parser);
    if (parser->error) goto cleanup;

    if (match_char_n(parser, 2, '+', '+')) {
        assert(false);
    }

cleanup:
    return expr;
}

static struct expr *parse_factor_expr(struct parser *parser) {
    struct expr *expr = parse_unary_expr(parser);
    if (parser->error) goto cleanup;

    while (true) {
        enum binary_op_type binary_op_type;

        if (match_char(parser, '*')) {
            binary_op_type = BINARY_OP_MUL;
        }
        else if (match_char(parser, '/')) {
            binary_op_type = BINARY_OP_DIV;
        }
        else {
            break;
        }

        struct expr *right = parse_unary_expr(parser);
        if (parser->error) goto cleanup;
        expr = new_binary_op_expr(&parser->allocator, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static struct expr *parse_term_expr(struct parser *parser) {
    struct expr *expr = parse_factor_expr(parser);
    if (parser->error) goto cleanup;

    while (true) {
        enum binary_op_type binary_op_type;

        if (match_char(parser, '+')) {
            binary_op_type = BINARY_OP_ADD;
        }
        else if (match_char(parser, '-')) {
            binary_op_type = BINARY_OP_SUB;
        }
        else {
            break;
        }

        struct expr *right = parse_factor_expr(parser);
        if (parser->error) goto cleanup;
        expr = new_binary_op_expr(&parser->allocator, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static struct expr *parse_comparison_expr(struct parser *parser) {
    struct expr *expr = parse_term_expr(parser);
    if (parser->error) goto cleanup;

    while (true) {
        enum binary_op_type binary_op_type;

        if (match_char_n(parser, 2, '<', '=')) {
            binary_op_type = BINARY_OP_LTE;
        }
        else if (match_char_n(parser, 1, '<')) {
            binary_op_type = BINARY_OP_LT;
        }
        else if (match_char_n(parser, 2, '>', '=')) {
            binary_op_type = BINARY_OP_GTE;
        }
        else if (match_char_n(parser, 1, '>')) {
            binary_op_type = BINARY_OP_GT;
        }
        else if (match_char_n(parser, 2, '=', '=')) {
            binary_op_type = BINARY_OP_EQ;
        }
        else if (match_char_n(parser, 2, '!', '=')) {
            binary_op_type = BINARY_OP_NEQ;
        }
        else {
            break;
        }

        struct expr *right = parse_term_expr(parser);
        if (parser->error) goto cleanup;
        expr = new_binary_op_expr(&parser->allocator, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static struct expr *parse_assignment_expr(struct parser *parser) {
    struct expr *expr = parse_comparison_expr(parser);
    if (parser->error) goto cleanup;

    while (true) {
        if (match_char(parser, '=')) {
            struct expr *right = parse_assignment_expr(parser);
            if (parser->error) goto cleanup;

            expr = new_assignment_expr(&parser->allocator, expr, right);
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static struct expr *parse_expr(struct parser *parser) {
    struct expr *expr = parse_assignment_expr(parser);
    return expr;
}

static struct stmt *parse_stmt(struct parser *parser) {
    if (match_symbol(parser, "if")) {
        return parse_for_stmt(parser);
    }
    else if (match_symbol(parser, "for")) {
        return parse_for_stmt(parser);
    }
    else if (match_symbol(parser, "return")) {
        return parse_return_stmt(parser);
    }
    else if (check_type(parser)) {
        return parse_variable_declaration_stmt(parser);
    }
    else if (match_char(parser, '{')) {
        return parse_block_stmt(parser);
    }
    else {
        struct expr *expr = parse_expr(parser);
        if (parser->error) return NULL;

        if (!match_char(parser, ';')) {
            parser_error(parser, "Expected ';'");
            return NULL;
        }
        return new_expr_stmt(&parser->allocator, expr);
    }
}

static struct stmt *parse_if_stmt(struct parser *parser) {
    struct array_expr_ptr conds;
    struct array_stmt_ptr stmts;
    struct stmt *else_stmt = NULL;
    array_init(&conds);
    array_init(&stmts);

    struct stmt *stmt = NULL;

    if (!match_char(parser, '(')) {
        parser_error(parser, "Expected '('");
        goto cleanup;
    }

    {
        struct expr *cond = parse_expr(parser);
        if (parser->error) goto cleanup;

        if (!match_char(parser, ')')) {
            parser_error(parser, "Expected ')'");
            goto cleanup;
        }

        struct stmt *stmt = parse_stmt(parser);
        if (parser->error) goto cleanup;

        array_push(&conds, cond);
        array_push(&stmts, stmt);
    }

    while (true) {
        if (match_symbol_n(parser, 2, "else", "if")) {
            if (!match_char(parser, '(')) {
                parser_error(parser, "Expected '('");
                goto cleanup;
            }

            struct expr *cond = parse_expr(parser);
            if (parser->error) goto cleanup;

            if (!match_char(parser, ')')) {
                parser_error(parser, "Expected ')'");
                goto cleanup;
            }

            struct stmt *stmt = parse_stmt(parser);
            if (parser->error) goto cleanup;

            array_push(&conds, cond);
            array_push(&stmts, stmt);
        }
        else if (match_symbol(parser, "else")) {
            else_stmt = parse_stmt(parser);
            if (parser->error) goto cleanup;
            break;
        }
        else {
            break;
        }
    }

    stmt = new_if_stmt(&parser->allocator, conds, stmts, else_stmt);

cleanup:
    array_deinit(&conds);
    array_deinit(&stmts);
    return stmt;
}

static struct stmt *parse_block_stmt(struct parser *parser) {
    struct array_stmt_ptr stmts;
    array_init(&stmts);

    struct stmt *stmt = NULL;
    
    while (true) {
        if (match_char(parser, '}')) {
            break;
        }

        struct stmt *stmt = parse_stmt(parser);
        if (parser->error) goto cleanup;
        array_push(&stmts, stmt);
    }

    stmt = new_block_stmt(&parser->allocator, stmts);

cleanup:
    array_deinit(&stmts);
    return stmt;
}

static struct stmt *parse_for_stmt(struct parser *parser) {
    struct stmt *stmt = NULL;

    if (!match_char(parser, '(')) {
        parser_error(parser, "Expected '('");
        goto cleanup;
    }

    struct expr *init = parse_expr(parser);
    if (parser->error) goto cleanup;
    if (!match_char(parser, ';')) {
        parser_error(parser, "Expected ';'");
        goto cleanup;
    }

    struct expr *cond = parse_expr(parser);
    if (parser->error) goto cleanup;
    if (!match_char(parser, ';')) {
        parser_error(parser, "Expected ';'");
        goto cleanup;
    }

    struct expr *inc = parse_expr(parser);
    if (parser->error) goto cleanup;
    if (!match_char(parser, ';')) {
        parser_error(parser, "Expected ';'");
        goto cleanup;
    }

    struct stmt *body = parse_stmt(parser);
    if (parser->error) goto cleanup;

    stmt = new_for_stmt(&parser->allocator, init, cond, inc, body);

cleanup:
    return stmt;
}

static struct stmt *parse_return_stmt(struct parser *parser) {
    struct stmt *stmt = NULL;
    struct expr *expr = NULL;

    if (!match_char(parser, ';')) {
        expr = parse_expr(parser);
        if (parser->error) goto cleanup;
        if (!match_char(parser, ';')) {
            parser_error(parser, "Expected ';'");
            goto cleanup;
        }
    }

    stmt = new_return_stmt(&parser->allocator, expr);

cleanup:
    return stmt;
}

static struct stmt *parse_variable_declaration_stmt(struct parser *parser) {
    struct stmt *stmt = NULL;

    struct type type;
    parse_type(parser, &type);
    if (parser->error) goto cleanup;

    struct token name = peek(parser);
    if (name.type != TOKEN_SYMBOL) {
        parser_error(parser, "Expected symbol");
        goto cleanup;
    }
    eat(parser);

    struct expr *expr = NULL;
    if (match_char(parser, '=')) {
        expr = parse_expr(parser);
        if (parser->error) goto cleanup;
    }

    if (!match_char(parser, ';')) {
        parser_error(parser, "Expected ';'");
        goto cleanup;
    }

    stmt = new_variable_declaration_stmt(&parser->allocator, type, name.symbol, expr);

cleanup:
    return stmt;
}

static struct stmt *parse_function_declaration_stmt(struct parser *parser) {
    struct array_type arg_types;
    struct array_char_ptr arg_names;
    array_init(&arg_types);
    array_init(&arg_names);

    struct stmt *stmt = NULL;

    struct type return_type;
    parse_type(parser, &return_type);
    if (parser->error) goto cleanup;

    struct token name = peek(parser);
    if (name.type != TOKEN_SYMBOL) {
        parser_error(parser, "Expected symbol");
        goto cleanup;
    }
    eat(parser);

    if (!match_char(parser, '(')) {
        parser_error(parser, "Expected '('");
        goto cleanup;
    }

    if (!match_char(parser, ')')) {
        while (true) {
            struct type arg_type;
            parse_type(parser, &arg_type);
            if (parser->error) goto cleanup;

            struct token arg_name = peek(parser);
            if (arg_name.type != TOKEN_SYMBOL) {
                parser_error(parser, "Expected symbol");
                goto cleanup;
            }
            eat(parser);

            array_push(&arg_types, arg_type);
            array_push(&arg_names, arg_name.symbol);

            if (!match_char(parser, ',')) {
                if (!match_char(parser, ')')) {
                    parser_error(parser, "Expected ')'");
                    goto cleanup;
                }
                break;
            }
        }
    }

    if (!match_char(parser, '{')) {
        parser_error(parser, "Expected '{'");
        goto cleanup;
    }

    struct stmt *body_stmt = parse_block_stmt(parser);
    if (parser->error) goto cleanup;

    stmt = new_function_declaration_stmt(&parser->allocator, return_type, name.symbol, arg_types, arg_names, body_stmt);

cleanup:
    array_deinit(&arg_types);
    array_deinit(&arg_names);
    return stmt;
}

static struct stmt *parse_struct_declaration_stmt(struct parser *parser) {
    struct array_type member_types;
    struct array_char_ptr member_names;
    array_init(&member_types);
    array_init(&member_names);

    struct stmt *stmt = NULL;

    struct token name = peek(parser);
    if (name.type != TOKEN_SYMBOL) {
        parser_error(parser, "Expected symbol");
        goto cleanup;
    }
    eat(parser);

    if (!match_char(parser, '{')) {
        parser_error(parser, "Expected '{'");
        goto cleanup;
    }

    while (true) {
        if (match_char(parser, '}')) {
            break;
        }

        struct type member_type;
        parse_type(parser, &member_type);
        if (parser->error) goto cleanup;

        struct token member_name = peek(parser);
        if (member_name.type != TOKEN_SYMBOL) {
            parser_error(parser, "Expected symbol");
            goto cleanup;
        }
        eat(parser);

        array_push(&member_types, member_type);
        array_push(&member_names, member_name.symbol);
        if (!match_char(parser, ';')) {
            parser_error(parser, "Expected ';'");
            goto cleanup;
        }
    }

    stmt = new_struct_declaration_stmt(&parser->allocator, name.symbol, member_types, member_names);

cleanup:
    array_deinit(&member_types);
    array_deinit(&member_names);
    return stmt;
}

static struct stmt *parse_import_stmt(struct parser *parser) {
    struct stmt *stmt = NULL;

    struct token module_name = peek(parser);
    if (module_name.type != TOKEN_STRING) {
        parser_error(parser, "Expected string");
        goto cleanup;
    }
    eat(parser);

    if (!match_char(parser, ';')) {
        parser_error(parser, "Expected ';'");
        goto cleanup;
    }

    stmt = new_import_stmt(&parser->allocator, module_name.symbol);

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

static struct token string_token(const char *text, int *len, int line, int col) {
    *len = 0;
    while (text[*len] != '"') {
        (*len)++;
    }

    char *string = malloc((*len) + 1);
    for (int i = 0; i < *len; i++) {
        string[i] = text[i];
    }
    string[*len] = 0;

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

static void tokenize(struct parser *parser) {
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
            array_push(&parser->tokens, string_token(prog + i, &len, line, col));
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
            parser_error_token(parser, tok, "Unknown character: %c", prog[i]);
            return;
        }
    }
}

static void parser_allocator_init(struct parser_allocator *allocator) {
    allocator->bytes_allocated = 0;
    array_init(&allocator->ptrs);
}

static void parser_allocator_deinit(struct parser_allocator *allocator) {
    array_deinit(&allocator->ptrs);
}

static void *parser_allocator_alloc(struct parser_allocator *allocator, size_t size) {
    allocator->bytes_allocated += size;
    void *mem = malloc(size);
    array_push(&allocator->ptrs, mem);
    return mem;
}

static void parser_init(struct parser *parser, const char *prog_text) {
    parser->prog_text = prog_text;
    parser_allocator_init(&parser->allocator);
    parser->token_idx = 0;
    array_init(&parser->tokens);
    parser->error = NULL;
}

static void parser_deinit(struct parser *parser) {
}

static void parser_error(struct parser *parser, char *fmt, ...) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[255] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 255, fmt, args);
    va_end(args);

    parser->error = buffer;
    parser->error_token = peek(parser);
}

static void parser_error_token(struct parser *parser, struct token token, char *fmt, ...) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[255] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, 255, fmt, args);
    va_end(args);

    parser->error = buffer;
    parser->error_token = token;
}

static struct token peek(struct parser *parser) {
    if (parser->token_idx >= parser->tokens.length) {
        // Return EOF
        return parser->tokens.data[parser->tokens.length - 1];
    }
    else {
        return parser->tokens.data[parser->token_idx];
    }
}

static struct token peek_n(struct parser *parser, int n) {
    if (parser->token_idx + n >= parser->tokens.length) {
        // Return EOF
        return parser->tokens.data[parser->tokens.length - 1];
    }
    else {
        return parser->tokens.data[parser->token_idx + n];
    }
}

static void eat(struct parser *parser) {
    parser->token_idx++;
}

static bool match_char(struct parser *parser, char c) {
    struct token tok = peek(parser);
    if (tok.type == TOKEN_CHAR && tok.char_value == c) {
        eat(parser);
        return true;
    }
    else {
        return false;
    }
}

static bool match_char_n(struct parser *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        char c = va_arg(ap, int);
        struct token tok = peek_n(parser, i);
        if (tok.type != TOKEN_CHAR || tok.char_value != c) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        for (int i = 0; i < n; i++) {
            eat(parser);
        }
    }

    return match;
}

static bool match_symbol(struct parser *parser, const char *symbol) {
    struct token tok = peek(parser);
    if (tok.type == TOKEN_SYMBOL && (strcmp(symbol, tok.symbol) == 0)) {
        eat(parser);
        return true;
    }
    else {
        return false;
    }
}

static bool match_symbol_n(struct parser *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *symbol = va_arg(ap, const char *);
        struct token tok = peek_n(parser, i);
        if (tok.type != TOKEN_SYMBOL || (strcmp(symbol, tok.symbol) != 0)) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        for (int i = 0; i < n; i++) {
            eat(parser);
        }
    }

    return match;
}

static bool match_eof(struct parser *parser) {
    struct token tok = peek(parser);
    return tok.type == TOKEN_EOF;
}

static bool check_type(struct parser *parser) {
    // Type's begin with 2 symbols or 1 symbol followed by [] for an array.
    struct token tok0 = peek_n(parser, 0);
    struct token tok1 = peek_n(parser, 1);
    struct token tok2 = peek_n(parser, 2);
    return ((tok0.type == TOKEN_SYMBOL) && (tok1.type == TOKEN_SYMBOL)) ||
            ((tok0.type == TOKEN_SYMBOL) &&
             (tok1.type == TOKEN_CHAR) &&
             (tok1.char_value == '[') &&
             (tok2.type == TOKEN_CHAR) &&
             (tok2.char_value == ']'));
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

static void debug_log_type(struct type type) {
    enum type_type t = type.type;
    if (type.type == TYPE_ARRAY) {
        t = type.array_type;
    }

    switch (type.type) {
        case TYPE_VOID:
            m_logf("void");
            break;
        case TYPE_INT:
            m_logf("int");
            break;
        case TYPE_FLOAT:
            m_logf("float");
            break;
        case TYPE_STRUCT:
            m_logf("%s", type.struct_name);
            break;
        case TYPE_ARRAY:
            assert(false);
            break;
    }

    if (type.type == TYPE_ARRAY) {
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
            debug_log_expr(stmt->return_stmt.expr);
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
            m_logf(" %s", stmt->variable_declaration.name);
            if (stmt->variable_declaration.expr) {
                m_logf(" = ");
                debug_log_expr(stmt->variable_declaration.expr);
            }
            m_logf(";\n");
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
            m_logf("import \"%s\"\n", stmt->import.module_name);
            break;
    }
}

static void debug_log_expr(struct expr *expr) {
    switch (expr->type) {
        case EXPR_CAST:
            assert(false);
            break;
        case EXPR_ARRAY_LENGTH:
            m_log("(");
            debug_log_expr(expr->array_length.left);
            m_logf(".length)");
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
    }
}

void mscript_compile_2(const char *prog_text) {
    struct parser parser;
    parser_init(&parser, prog_text);
    tokenize(&parser);
    debug_log_tokens(parser.tokens.data);

    while (true) {
        if (match_eof(&parser)) {
            break;
        }

        struct stmt *stmt;
        if (match_symbol(&parser, "import")) {
            stmt = parse_import_stmt(&parser);
        }
        else if (match_symbol(&parser, "struct")) {
            stmt = parse_struct_declaration_stmt(&parser);
        }
        else if (check_type(&parser)) {
            stmt = parse_function_declaration_stmt(&parser);
        }
        else {
            assert(false);
        }

        if (parser.error) {
            m_logf("ERROR: %s. Line: %d. Col: %d\n", parser.error, parser.error_token.line, parser.error_token.col);
            break;
        }

        debug_log_stmt(stmt);
    }
}

