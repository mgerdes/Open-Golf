#include "script.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "common/common.h"
#include "common/file.h"
#include "common/log.h"
#include "common/maths.h"
#include "common/map.h"
#include "common/string.h"
#include "common/vec.h"
#include "common/level.h"

typedef struct gs_expr gs_expr_t;
typedef struct gs_stmt gs_stmt_t;

static void gs_debug_print_type(gs_val_type type);

static int gs_val_to_int(gs_val_t val);
static float gs_val_to_float(gs_val_t val);
static void gs_debug_print_val(gs_val_t val);

static bool gs_is_char_digit(char c);
static bool gs_is_char_start_of_symbol(char c);
static bool gs_is_char_part_of_symbol(char c);
static gs_token_t gs_token_eof(int line, int col);
static gs_token_t gs_token_char(char c, int line, int col);
static gs_token_t gs_token_symbol(const char *text, int *len, int line, int col);
static gs_token_t gs_token_string(const char *text, int *len, int line, int col);
static gs_token_t gs_token_number(const char *text, int *len, int line, int col);
static void gs_tokenize(gs_parser_t *parser, const char *src);
static void gs_debug_print_token(gs_token_t token);
static void gs_debug_print_tokens(vec_gs_token_t *tokens);

static gs_expr_t *gs_expr_int_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_float_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_symbol_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_string_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_binary_op_new(gs_parser_t *parser, gs_token_t token, gs_binary_op_type type, gs_expr_t *left, gs_expr_t *right);
static gs_expr_t *gs_expr_assignment_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *left, gs_expr_t *right);
static gs_expr_t *gs_expr_call_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *fn, vec_gs_expr_t args);
static gs_expr_t *gs_expr_member_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_token_t member);
static gs_expr_t *gs_expr_array_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_expr_t *arg);
static gs_expr_t *gs_expr_array_decl_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t args);
static gs_expr_t *gs_expr_cast_new(gs_parser_t *parser, gs_token_t token, gs_val_type type, gs_expr_t *arg);
static void gs_debug_print_expr(gs_expr_t *expr);

static gs_stmt_t *gs_stmt_if_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t conds, vec_gs_stmt_t stmts, gs_stmt_t *else_stmt);
static gs_stmt_t *gs_stmt_for_new(gs_parser_t *parser, gs_token_t token, gs_val_type decl_type, gs_token_t decl_symbol, gs_expr_t *init, gs_expr_t *cond, gs_expr_t *inc, gs_stmt_t *body);
static gs_stmt_t *gs_stmt_return_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr);
static gs_stmt_t *gs_stmt_block_new(gs_parser_t *parser, gs_token_t token, vec_gs_stmt_t stmts);
static gs_stmt_t *gs_stmt_expr_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr);
static gs_stmt_t *gs_stmt_var_decl_new(gs_parser_t *parser, gs_val_type type, vec_gs_token_t tokens, gs_expr_t *init);
static gs_stmt_t *gs_stmt_fn_decl_new(gs_parser_t *parser, gs_val_type return_type, gs_token_t symbol, vec_gs_val_type_t arg_types, vec_gs_token_t arg_symbols, gs_stmt_t *body);
static void gs_debug_print_stmt(gs_stmt_t *stmt, int tabs);

static void *gs_parser_alloc(gs_parser_t *parser, size_t size);
static void gs_parser_error(gs_parser_t *parser, gs_token_t token, const char *fmt, ...);
static gs_token_t gs_peek(gs_parser_t *parser);
static gs_token_t gs_peek_n(gs_parser_t *parser, int n);
static bool gs_peek_eof(gs_parser_t *parser);
static bool gs_peek_char(gs_parser_t *parser, char c);
static bool gs_peek_symbol(gs_parser_t *parser, const char *symbol);
static bool gs_peek_val_type(gs_parser_t *parser, gs_val_type *type);
static bool gs_peek_val_type_n(gs_parser_t *parser, gs_val_type *type, int n);
static void gs_eat(gs_parser_t *parser);
static void gs_eat_n(gs_parser_t *parser, int n);
static bool gs_eat_char(gs_parser_t *parser, char c);
static bool gs_eat_char_n(gs_parser_t *parser, int n, ...);
static bool gs_eat_symbol(gs_parser_t *parser, const char *symbol);
static bool gs_eat_symbol_n(gs_parser_t *parser, int n, ...);
static bool gs_eat_val_type(gs_parser_t *parser, gs_val_type *type);

static gs_val_type gs_parse_type(gs_parser_t *parser);

static gs_stmt_t *gs_parse_stmt(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_if(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_for(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_return(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_block(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_decl(gs_parser_t *parser);

static gs_expr_t *gs_parse_expr(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_assignment(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_comparison(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_factor(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_term(gs_parser_t *parser) ;
static gs_expr_t *gs_parse_expr_cast(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_member_access_or_array_access(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_call(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_primary(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_array_decl(gs_parser_t *parser);

static gs_val_t gs_eval_stmt(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_if(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_for(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_return(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_block(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_expr(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_var_decl(gs_eval_t *eval, gs_stmt_t *stmt);
static gs_val_t gs_eval_stmt_fn_decl(gs_eval_t *eval, gs_stmt_t *stmt);

static gs_val_t gs_eval_expr(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_binary_op(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_assignment(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_int(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_float(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_symbol(gs_eval_t *eval, gs_expr_t *expr) ;
static gs_val_t gs_eval_expr_string(gs_eval_t *eval, gs_expr_t *expr) ;
static gs_val_t gs_eval_expr_call(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_member_access(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_array_access(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_array_decl(gs_eval_t *eval, gs_expr_t *expr);
static gs_val_t gs_eval_expr_cast(gs_eval_t *eval, gs_expr_t *expr);

//gs_val_t gs_eval_cast(gs_eval_t *eval, gs_val_t val, gs_val_type type);
static gs_val_t gs_eval_binary_op_add(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_sub(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_mul(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_div(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_lt(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_gt(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_lte(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_gte(gs_eval_t *eval, gs_val_t left, gs_val_t right);
static gs_val_t gs_eval_binary_op_eq(gs_eval_t *eval, gs_val_t left, gs_val_t right);

static gs_val_t gs_eval_lval(gs_eval_t *eval, gs_expr_t *expr, gs_val_t **lval);

static void gs_debug_print_type(gs_val_type type) {
    switch (type) {
        case GS_VAL_VOID:
            printf("void");
            break;
        case GS_VAL_BOOL:
            printf("bool");
            break;
        case GS_VAL_INT:
            printf("int");
            break;
        case GS_VAL_FLOAT:
            printf("float");
            break;
        case GS_VAL_VEC2:
            printf("vec2");
            break;
        case GS_VAL_VEC3:
            printf("vec3");
            break;
        case GS_VAL_LIST:
            printf("list");
            break;
        case GS_VAL_STRING:
            printf("string");
            break;
        case GS_VAL_FN:
            printf("[FN]");
            break;
        case GS_VAL_C_FN:
            printf("[C_FN]");
            break;
        case GS_VAL_ERROR:
            printf("[ERROR]");
            break;
        case GS_VAL_NUM_TYPES:
            assert(false);
    }
}

static bool gs_is_char_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool gs_is_char_start_of_symbol(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c == '_');
}

static bool gs_is_char_part_of_symbol(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '_');
}

static gs_token_t gs_token_eof(int line, int col) {
    gs_token_t token;
    token.type = GS_TOKEN_EOF;
    token.line = line;
    token.col = col;
    return token;
}

static gs_token_t gs_token_char(char c, int line, int col) {
    gs_token_t token;
    token.type = GS_TOKEN_CHAR;
    token.c = c;
    token.line = line;
    token.col = col;
    return token;
}

static gs_token_t gs_token_symbol(const char *text, int *len, int line, int col) {
    *len = 0;
    while (gs_is_char_part_of_symbol(text[*len])) {
        (*len)++;
    }

    char *symbol = golf_alloc_tracked((*len) + 1, "script/parser");
    for (int i = 0; i < *len; i++) {
        symbol[i] = text[i];
    }
    symbol[*len] = 0;

    gs_token_t token;
    token.type = GS_TOKEN_SYMBOL;
    token.symbol = symbol;
    token.line = line;
    token.col = col;
    return token;
}

static gs_token_t gs_token_string(const char *text, int *len, int line, int col) {
    *len = 1;
    while (text[*len] && text[*len] != '"') {
        (*len)++;
    }

    char *string = golf_alloc_tracked((*len), "script/parser");
    for (int i = 1; i < *len; i++) {
        string[i - 1] = text[i];
    }
    string[(*len) - 1] = 0;

    if (text[*len] == '"') {
        (*len)++;
    }

    gs_token_t token;
    token.type = GS_TOKEN_STRING;
    token.string = string;
    token.line = line;
    token.col = col;
    return token;
}

static gs_token_t gs_token_number(const char *text, int *len, int line, int col) {
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
        if (gs_is_char_digit(text[*len])) {
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
        gs_token_t token;
        token.type = GS_TOKEN_FLOAT;
        token.float_val = (float)int_part + float_part;
        if (is_negative) token.float_val = -token.float_val;
        token.line = line;
        token.col = col;
        return token;
    }
    else {
        gs_token_t token;
        token.type = GS_TOKEN_INT;
        token.int_val = int_part;
        if (is_negative) token.int_val = -token.int_val;
        token.line = line;
        token.col = col;
        return token;
    }
}

static void gs_tokenize(gs_parser_t *parser, const char *src) {
    int line = 1; 
    int col = 1;
    int i = 0;

    while (true) {
        if (src[i] == ' ' || src[i] == '\t' || src[i] == '\r') {
            col++;
            i++;
        }
        else if (src[i] == '\n') {
            col = 1;
            line++;
            i++;
        }
        else if (src[i] == 0) {
            vec_push(&parser->tokens, gs_token_eof(line, col));
            break;
        }
        else if (src[i] == '/' && src[i + 1] == '/') {
            while (src[i] && src[i] != '\n') i++;
        }
        else if (gs_is_char_start_of_symbol(src[i])) {
            int len = 0;
            vec_push(&parser->tokens, gs_token_symbol(src + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (gs_is_char_digit(src[i])) {
            int len;
            vec_push(&parser->tokens, gs_token_number(src + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (src[i] == '"') {
            int len;
            vec_push(&parser->tokens, gs_token_string(src + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (src[i] == '+' || src[i] == '-' || src[i] == '*' || src[i] == '/' ||
                src[i] == '(' || src[i] == ')' || src[i] == '{' || src[i] == '}' ||
                src[i] == ';' || src[i] == '=' || src[i] == '>' || src[i] == '<' ||
                src[i] == ',' || src[i] == '.' || src[i] == '[' || src[i] == ']') {
            vec_push(&parser->tokens, gs_token_char(src[i], line, col));
            col++;
            i++;
        }
        else {
            gs_token_t token = gs_token_char(src[i], line, col);
            gs_parser_error(parser, token, "Unknown character when tokenizing");
            break;
        }
    }
}

static void gs_debug_print_token(gs_token_t token) {
    switch (token.type) {
        case GS_TOKEN_INT:
            printf("[INT %d (%d, %d)]", token.int_val, token.line, token.col);
            break;
        case GS_TOKEN_FLOAT:
            printf("[FLOAT %f (%d, %d)]", token.float_val, token.line, token.col);
            break;
        case GS_TOKEN_SYMBOL:
            printf("[SYM %s (%d, %d)]", token.symbol, token.line, token.col);
            break;
        case GS_TOKEN_CHAR:
            printf("[CHAR %c (%d, %d)]", token.c, token.line, token.col);
            break;
        case GS_TOKEN_STRING:
            printf("[STRING %s (%d, %d)]", token.string, token.line, token.col);
            break;
        case GS_TOKEN_EOF:
            printf("[EOF (%d, %d)]", token.line, token.col);
            break;
    }
}

static void gs_debug_print_tokens(vec_gs_token_t *tokens) {
    for (int i = 0; i < tokens->length; i++) {
        gs_debug_print_token(tokens->data[i]);
        printf("\n");
    }
}

static void *gs_parser_alloc(gs_parser_t *parser, size_t size) {
    void *ptr = golf_alloc_tracked(size, "script/parser");
    vec_push(&parser->allocated_memory, ptr);
    return ptr;
}

static gs_expr_t *gs_expr_int_new(gs_parser_t *parser, gs_token_t token) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_INT;
    expr->token = token;
    expr->int_val = token.int_val;
    return expr;
}

static gs_expr_t *gs_expr_float_new(gs_parser_t *parser, gs_token_t token) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_FLOAT;
    expr->token = token;
    expr->float_val = token.float_val;
    return expr;
}

static gs_expr_t *gs_expr_symbol_new(gs_parser_t *parser, gs_token_t token) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_SYMBOL;
    expr->token = token;
    expr->symbol = token.symbol;
    return expr;
}

static gs_expr_t *gs_expr_string_new(gs_parser_t *parser, gs_token_t token) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_STRING;
    expr->token = token;
    expr->string = token.string;
    return expr;
}

static gs_expr_t *gs_expr_binary_op_new(gs_parser_t *parser, gs_token_t token, gs_binary_op_type type, gs_expr_t *left, gs_expr_t *right) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_BINARY_OP;
    expr->token = token;
    expr->binary_op.type = type;
    expr->binary_op.left = left;
    expr->binary_op.right = right;
    return expr;
}

static gs_expr_t *gs_expr_assignment_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *left, gs_expr_t *right) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_ASSIGNMENT;
    expr->token = token;
    expr->assignment.left = left;
    expr->assignment.right = right;
    return expr;
}

static gs_expr_t *gs_expr_call_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *fn, vec_gs_expr_t args) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_CALL;
    expr->token = token;
    expr->call.fn = fn;
    expr->call.num_args = args.length;
    expr->call.args = gs_parser_alloc(parser, sizeof(gs_expr_t*) * args.length);
    memcpy(expr->call.args, args.data, sizeof(gs_expr_t*) * args.length);
    return expr;
}

static gs_expr_t *gs_expr_member_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_token_t member) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_MEMBER_ACCESS;
    expr->token = token;
    expr->member_access.val = val;
    expr->member_access.member = member;
    return expr;
}

static gs_expr_t *gs_expr_array_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_expr_t *arg) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_ARRAY_ACCESS;
    expr->token = token;
    expr->array_access.val = val;
    expr->array_access.arg = arg;
    return expr;
}

static gs_expr_t *gs_expr_array_decl_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t args) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_ARRAY_DECL;
    expr->token = token;
    expr->array_decl.num_args = args.length;
    expr->array_decl.args = gs_parser_alloc(parser, sizeof(gs_expr_t*) * args.length);
    memcpy(expr->array_decl.args, args.data, sizeof(gs_expr_t*) * args.length);
    return expr;
}

static gs_expr_t *gs_expr_cast_new(gs_parser_t *parser, gs_token_t token, gs_val_type type, gs_expr_t *arg) {
    gs_expr_t *expr = gs_parser_alloc(parser, sizeof(gs_expr_t));
    expr->type = GS_EXPR_CAST;
    expr->token = token;
    expr->cast.type = type;
    expr->cast.arg = arg;
    return expr;
}

static void gs_debug_print_expr(gs_expr_t *expr) {
    switch (expr->type) {
        case GS_EXPR_BINARY_OP:
            printf("(");
            gs_debug_print_expr(expr->binary_op.left);
            switch (expr->binary_op.type) {
                case GS_BINARY_OP_ADD:
                    printf("+");
                    break;
                case GS_BINARY_OP_SUB:
                    printf("-");
                    break;
                case GS_BINARY_OP_MUL:
                    printf("*");
                    break;
                case GS_BINARY_OP_DIV:
                    printf("/");
                    break;
                case GS_BINARY_OP_LT:
                    printf("<");
                    break;
                case GS_BINARY_OP_GT:
                    printf(">");
                    break;
                case GS_BINARY_OP_LTE:
                    printf("<=");
                    break;
                case GS_BINARY_OP_GTE:
                    printf(">=");
                    break;
                case GS_BINARY_OP_EQ:
                    printf("==");
                    break;
                case GS_NUM_BINARY_OPS:
                    assert(false);
                    break;
            }
            gs_debug_print_expr(expr->binary_op.right);
            printf(")");
            break;
        case GS_EXPR_ASSIGNMENT:
            printf("(");
            gs_debug_print_expr(expr->assignment.left);
            printf("=");
            gs_debug_print_expr(expr->assignment.right);
            printf(")");
            break;
        case GS_EXPR_INT:
            printf("%d", expr->int_val);
            break;
        case GS_EXPR_FLOAT:
            printf("%f", expr->float_val);
            break;
        case GS_EXPR_SYMBOL:
            printf("%s", expr->symbol);
            break;
        case GS_EXPR_STRING:
            printf("%s", expr->string);
            break;
        case GS_EXPR_CALL:
            gs_debug_print_expr(expr->call.fn);
            printf("(");
            for (int i = 0; i < expr->call.num_args; i++) {
                gs_debug_print_expr(expr->call.args[i]);
                if (i + 1< expr->call.num_args) {
                    printf(", ");
                }
            }
            printf(")");
            break;
        case GS_EXPR_MEMBER_ACCESS:
            printf("(");
            gs_debug_print_expr(expr->member_access.val);
            printf(".");
            printf("%s", expr->member_access.member.symbol);
            printf(")");
            break;
        case GS_EXPR_ARRAY_ACCESS:
            gs_debug_print_expr(expr->array_access.val);
            printf("[");
            gs_debug_print_expr(expr->array_access.arg);
            printf("]");
            break;
        case GS_EXPR_ARRAY_DECL:
            printf("[");
            for (int i = 0; i < expr->array_decl.num_args; i++) {
                gs_debug_print_expr(expr->array_decl.args[i]);
                if (i + 1 < expr->array_decl.num_args) {
                    printf(", ");
                }
            }
            printf("]");
            break;
        case GS_EXPR_CAST:
            printf("(");
            printf("(");
            gs_debug_print_type(expr->cast.type);
            printf(")");
            gs_debug_print_expr(expr->cast.arg);
            printf(")");
            break;
    }
}

static gs_stmt_t *gs_stmt_if_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t conds, vec_gs_stmt_t stmts, gs_stmt_t *else_stmt) {
    GOLF_UNUSED(token);
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_IF;
    stmt->if_stmt.num_conds = conds.length;
    stmt->if_stmt.conds = gs_parser_alloc(parser, sizeof(gs_expr_t*) * conds.length);
    memcpy(stmt->if_stmt.conds, conds.data, sizeof(gs_expr_t*) * conds.length);
    stmt->if_stmt.stmts = gs_parser_alloc(parser, sizeof(gs_stmt_t*) * stmts.length);
    memcpy(stmt->if_stmt.stmts, stmts.data, sizeof(gs_stmt_t*) * stmts.length);
    stmt->if_stmt.else_stmt = else_stmt;
    return stmt;
}

static gs_stmt_t *gs_stmt_for_new(gs_parser_t *parser, gs_token_t token, gs_val_type decl_type, gs_token_t decl_symbol, gs_expr_t *init, gs_expr_t *cond, gs_expr_t *inc, gs_stmt_t *body) {
    GOLF_UNUSED(token);
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_FOR;
    stmt->for_stmt.decl_type = decl_type;
    stmt->for_stmt.decl_symbol = decl_symbol;
    stmt->for_stmt.init = init;
    stmt->for_stmt.cond = cond;
    stmt->for_stmt.inc = inc;
    stmt->for_stmt.body = body;
    return stmt;
}

static gs_stmt_t *gs_stmt_return_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr) {
    GOLF_UNUSED(token);
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_RETURN;
    stmt->return_stmt.expr = expr;
    return stmt;
}

static gs_stmt_t *gs_stmt_block_new(gs_parser_t *parser, gs_token_t token, vec_gs_stmt_t stmts) {
    GOLF_UNUSED(token);
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_BLOCK;
    stmt->block_stmt.num_stmts = stmts.length;
    stmt->block_stmt.stmts = gs_parser_alloc(parser, sizeof(gs_stmt_t*) * stmts.length);
    memcpy(stmt->block_stmt.stmts, stmts.data, sizeof(gs_stmt_t*) * stmts.length);
    return stmt;
}

static gs_stmt_t *gs_stmt_expr_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr) {
    GOLF_UNUSED(token);
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_EXPR;
    stmt->expr = expr;
    return stmt;
}

static gs_stmt_t *gs_stmt_var_decl_new(gs_parser_t *parser, gs_val_type type, vec_gs_token_t tokens, gs_expr_t *init) {
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_VAR_DECL;
    stmt->var_decl.type = type;
    stmt->var_decl.num_ids = tokens.length;
    stmt->var_decl.tokens = gs_parser_alloc(parser, sizeof(gs_token_t) * tokens.length);
    memcpy(stmt->var_decl.tokens, tokens.data, sizeof(gs_token_t) * tokens.length);
    stmt->var_decl.init = init;
    return stmt;
}

static gs_stmt_t *gs_stmt_fn_decl_new(gs_parser_t *parser, gs_val_type return_type, gs_token_t symbol, vec_gs_val_type_t arg_types, vec_gs_token_t arg_symbols, gs_stmt_t *body) {
    gs_stmt_t *stmt = gs_parser_alloc(parser, sizeof(gs_stmt_t));
    stmt->type = GS_STMT_FN_DECL;
    stmt->fn_decl.return_type = return_type;
    stmt->fn_decl.symbol = symbol;
    stmt->fn_decl.num_args = arg_types.length;
    stmt->fn_decl.arg_types = gs_parser_alloc(parser, sizeof(gs_val_type) * arg_types.length);
    memcpy(stmt->fn_decl.arg_types, arg_types.data, sizeof(gs_val_type) * arg_types.length);
    stmt->fn_decl.arg_symbols = gs_parser_alloc(parser, sizeof(gs_token_t) * arg_types.length);
    memcpy(stmt->fn_decl.arg_symbols, arg_symbols.data, sizeof(gs_token_t) * arg_types.length);
    stmt->fn_decl.body = body;
    return stmt;
}

static void gs_debug_print_stmt(gs_stmt_t *stmt, int tabs) {
    for (int i = 0; i < tabs; i++) { 
        printf("  ");
    }

    switch (stmt->type) {
        case GS_STMT_EXPR: {
            gs_debug_print_expr(stmt->expr);
            printf(";\n");
            break;
        }
        case GS_STMT_IF: {
            for (int i = 0; i < stmt->if_stmt.num_conds; i++) {
                if (i == 0) {
                    printf("if (");
                }
                else {
                    printf("else if (");
                }
                gs_debug_print_expr(stmt->if_stmt.conds[i]);
                printf(")\n");
                gs_debug_print_stmt(stmt->if_stmt.stmts[i], tabs + 1);
                for (int i = 0; i < tabs; i++) { 
                    printf("  ");
                }
            }

            if (stmt->if_stmt.else_stmt) {
                printf("else\n");
                gs_debug_print_stmt(stmt->if_stmt.else_stmt, tabs + 1);
            }
            break;
        }
        case GS_STMT_FOR: {
            printf("for (");
            if (stmt->for_stmt.decl_type) {
                gs_debug_print_type(stmt->for_stmt.decl_type);
                printf(" %s = ", stmt->for_stmt.decl_symbol.symbol);
            }
            gs_debug_print_expr(stmt->for_stmt.init);
            printf("; ");
            gs_debug_print_expr(stmt->for_stmt.cond);
            printf("; ");
            gs_debug_print_expr(stmt->for_stmt.inc);
            printf(")\n");
            gs_debug_print_stmt(stmt->for_stmt.body, tabs + 1);
            break;
        }
        case GS_STMT_RETURN: {
            printf("return ");
            gs_debug_print_expr(stmt->return_stmt.expr);
            printf(";\n");
            break;
        }
        case GS_STMT_BLOCK: {
            printf("{\n");
            for (int i = 0; i < stmt->block_stmt.num_stmts; i++) {
                gs_debug_print_stmt(stmt->block_stmt.stmts[i], tabs + 1);
            }
            for (int i = 0; i < tabs; i++) {
                printf("  ");
            }
            printf("}\n");
            break;
        }
        case GS_STMT_VAR_DECL: {
            gs_debug_print_type(stmt->var_decl.type);
            printf(" ");
            for (int i = 0; i < stmt->var_decl.num_ids; i++) {
                printf("%s", stmt->var_decl.tokens[i].symbol);
                if (i + 1 < stmt->var_decl.num_ids) {
                    printf(", ");
                }
            }
            if (stmt->var_decl.init) {
                printf(" = ");
                gs_debug_print_expr(stmt->var_decl.init);
            }
            printf(";\n");
            break;
        }
        case GS_STMT_FN_DECL: {
            gs_debug_print_type(stmt->fn_decl.return_type);
            printf(" %s(", stmt->fn_decl.symbol.symbol);
            for (int i = 0; i < stmt->fn_decl.num_args; i++) {
                gs_debug_print_type(stmt->fn_decl.arg_types[i]);
                printf(" %s", stmt->fn_decl.arg_symbols[i].symbol);
                if (i + 1 < stmt->fn_decl.num_args) {
                    printf(", ");
                }
            }
            printf(")\n");
            gs_debug_print_stmt(stmt->fn_decl.body, tabs + 1);
            break;
        }
    }
}

static gs_val_t gs_eval_stmt(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val;
    switch (stmt->type) {
        case GS_STMT_IF:
            val = gs_eval_stmt_if(eval, stmt);
            break;
        case GS_STMT_FOR:
            val = gs_eval_stmt_for(eval, stmt);
            break;
        case GS_STMT_RETURN:
            val = gs_eval_stmt_return(eval, stmt);
            break;
        case GS_STMT_BLOCK:
            val = gs_eval_stmt_block(eval, stmt);
            break;
        case GS_STMT_EXPR:
            val = gs_eval_stmt_expr(eval, stmt);
            break;
        case GS_STMT_VAR_DECL:
            val = gs_eval_stmt_var_decl(eval, stmt);
            break;
        case GS_STMT_FN_DECL:
            val = gs_eval_stmt_fn_decl(eval, stmt);
            break;
    } 
    return val;
}

static gs_val_t gs_eval_stmt_if(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val;

    bool found_true_cond = false;
    for (int i = 0; i < stmt->if_stmt.num_conds; i++) {
        val = gs_eval_expr(eval, stmt->if_stmt.conds[i]);
        if (val.is_return) goto cleanup;

        val = gs_eval_cast(eval, val, GS_VAL_BOOL);
        if (val.is_return) goto cleanup;

        if (val.type == GS_VAL_BOOL) {
            if (val.bool_val) {
                found_true_cond = true;
                val = gs_eval_stmt(eval, stmt->if_stmt.stmts[i]);
                if (val.is_return) goto cleanup;
                break;
            }
        }
        else {
            val = gs_val_error("Expected type of val to be bool");
            goto cleanup;
        }
    }

    if (!found_true_cond && stmt->if_stmt.else_stmt) {
        val = gs_eval_stmt(eval, stmt->if_stmt.else_stmt);
        if (val.is_return) goto cleanup;
    }

cleanup:
    return val;
}

static gs_val_t gs_eval_stmt_for(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val = gs_val_int(0);

    gs_env_t *env = golf_alloc(sizeof(gs_env_t));
    map_init(&env->val_map, "script/eval");
    vec_push(&eval->env, env);

    gs_val_t init_val = gs_eval_expr(eval, stmt->for_stmt.init);
    if (init_val.is_return) {
        val = init_val;
        goto cleanup;
    }
    init_val = gs_eval_cast(eval, init_val, stmt->for_stmt.decl_type);
    if (init_val.is_return) {
        val = init_val;
        goto cleanup;
    }

    const char *sym = stmt->for_stmt.decl_symbol.symbol;
    map_set(&env->val_map, sym, init_val);

    while (true) {
        gs_val_t cond_val = gs_eval_expr(eval, stmt->for_stmt.cond);
        if (cond_val.is_return) {
            val = cond_val;
            goto cleanup;
        }

        cond_val = gs_eval_cast(eval, cond_val, GS_VAL_BOOL);
        if (cond_val.is_return) {
            val = cond_val;
            goto cleanup;
        }

        if (!cond_val.bool_val) break;

        gs_val_t body_val = gs_eval_stmt(eval, stmt->for_stmt.body);
        if (body_val.is_return) {
            val = body_val;
            goto cleanup;
        }

        gs_val_t inc_val = gs_eval_expr(eval, stmt->for_stmt.inc);
        if (inc_val.is_return) {
            val = inc_val;
            goto cleanup;
        }
    }

cleanup:
    map_deinit(&env->val_map);
    golf_free(env);
    (void)vec_pop(&eval->env);
    return val;
}

static gs_val_t gs_eval_stmt_return(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val = gs_eval_expr(eval, stmt->return_stmt.expr);
    val.is_return = true;
    return val;
}

static gs_val_t gs_eval_stmt_block(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val;
    gs_env_t *env = golf_alloc(sizeof(gs_env_t));
    map_init(&env->val_map, "script/eval");
    vec_push(&eval->env, env);

    for (int i = 0; i < stmt->block_stmt.num_stmts; i++) {
        val = gs_eval_stmt(eval, stmt->block_stmt.stmts[i]);
        if (val.is_return) goto cleanup;
    }

cleanup:
    map_deinit(&env->val_map);
    golf_free(env);
    (void)vec_pop(&eval->env);
    return val;
}

static gs_val_t gs_eval_stmt_expr(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val = gs_eval_expr(eval, stmt->expr);
    return val;
}

static gs_val_t gs_eval_stmt_var_decl(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val = gs_val_default(stmt->var_decl.type);
    if (val.is_return) goto cleanup;

    if (stmt->var_decl.init) {
        val = gs_eval_expr(eval, stmt->var_decl.init);
        if (val.is_return) goto cleanup;

        val = gs_eval_cast(eval, val, stmt->var_decl.type);
        if (val.is_return) goto cleanup;
    }

    assert(stmt->var_decl.num_ids == 1);

    gs_token_t token = stmt->var_decl.tokens[0];
    gs_env_t *env = vec_last(&eval->env);
    if (map_get(&env->val_map, token.symbol)) {
        val = gs_val_error("Variable is already declared");
        goto cleanup;
    }
    map_set(&env->val_map, token.symbol, val);

cleanup:
    return val;
}

static gs_val_t gs_eval_stmt_fn_decl(gs_eval_t *eval, gs_stmt_t *stmt) {
    gs_val_t val = gs_val_fn(stmt);
    if (val.is_return) goto cleanup;

    gs_token_t token = stmt->fn_decl.symbol;
    gs_env_t *env = vec_last(&eval->env);
    if (map_get(&env->val_map, token.symbol)) {
        val = gs_val_error("Variable is already declared");
        goto cleanup;
    }

    map_set(&env->val_map, token.symbol, val);

cleanup:
    return val;
}

static gs_val_t gs_eval_expr(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val;
    switch (expr->type) {
        case GS_EXPR_BINARY_OP:
            val = gs_eval_expr_binary_op(eval, expr);
            break;
        case GS_EXPR_ASSIGNMENT:
            val = gs_eval_expr_assignment(eval, expr);
            break;
        case GS_EXPR_INT:
            val = gs_eval_expr_int(eval, expr);
            break;
        case GS_EXPR_FLOAT:
            val = gs_eval_expr_float(eval, expr);
            break;
        case GS_EXPR_SYMBOL:
            val = gs_eval_expr_symbol(eval, expr);
            break;
        case GS_EXPR_STRING:
            val = gs_eval_expr_string(eval, expr);
            break;
        case GS_EXPR_CALL:
            val = gs_eval_expr_call(eval, expr);
            break;
        case GS_EXPR_MEMBER_ACCESS:
            val = gs_eval_expr_member_access(eval, expr);
            break;
        case GS_EXPR_ARRAY_ACCESS:
            val = gs_eval_expr_array_access(eval, expr);
            break;
        case GS_EXPR_ARRAY_DECL:
            val = gs_eval_expr_array_decl(eval, expr);
            break;
        case GS_EXPR_CAST:
            val = gs_eval_expr_cast(eval, expr);
            break;
    }
    return val;
}

static gs_val_t gs_eval_expr_binary_op(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val;

    gs_val_t left_val = gs_eval_expr(eval, expr->binary_op.left);
    if (left_val.is_return) {
        val = left_val;
        goto cleanup;
    }

    gs_val_t right_val = gs_eval_expr(eval, expr->binary_op.right);
    if (right_val.is_return) {
        val = right_val;
        goto cleanup;
    }

    switch (expr->binary_op.type) {
        case GS_BINARY_OP_ADD: 
            val = gs_eval_binary_op_add(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_SUB: 
            val = gs_eval_binary_op_sub(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_MUL:
            val = gs_eval_binary_op_mul(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_DIV:
            val = gs_eval_binary_op_div(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_LT:
            val = gs_eval_binary_op_lt(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_GT:
            val = gs_eval_binary_op_gt(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_LTE:
            val = gs_eval_binary_op_lte(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_GTE:
            val = gs_eval_binary_op_gte(eval, left_val, right_val);
            break;
        case GS_BINARY_OP_EQ:
            val = gs_eval_binary_op_eq(eval, left_val, right_val);
            break;
        case GS_NUM_BINARY_OPS:
            assert(false);
            break;
    }

cleanup:
    return val;
}

static gs_val_t gs_eval_expr_assignment(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val;

    gs_val_t right_val = gs_eval_expr(eval, expr->assignment.right);
    if (right_val.is_return) {
        val = right_val;
        goto cleanup;
    }

    gs_val_t *lval;
    val = gs_eval_lval(eval, expr->assignment.left, &lval);
    if (val.is_return) goto cleanup;
    assert(lval);

    if (expr->assignment.left->type == GS_EXPR_MEMBER_ACCESS) {
        const char *sym = expr->assignment.left->member_access.member.symbol;
        right_val = gs_eval_cast(eval, right_val, GS_VAL_FLOAT);
        if (right_val.is_return) {
            val = right_val;
            goto cleanup;
        }

        if (lval->type == GS_VAL_VEC2) {
            if (strcmp(sym, "x") == 0) {
                lval->vec2_val.x = right_val.float_val;
            }
            else if (strcmp(sym, "y") == 0) {
                lval->vec2_val.y = right_val.float_val;
            }
            else {
            }
        }
        else if (lval->type == GS_VAL_VEC3) {
            if (strcmp(sym, "x") == 0) {
                lval->vec3_val.x = right_val.float_val;
            }
            else if (strcmp(sym, "y") == 0) {
                lval->vec3_val.y = right_val.float_val;
            }
            else if (strcmp(sym, "z") == 0) {
                lval->vec3_val.z = right_val.float_val;
            }
            else {
            }
        }
        else {
            val = gs_val_error("Unable to perform member access on type");
            goto cleanup;
        }
    }
    else {
        // Any type can be put in a list
        if (expr->assignment.left->type != GS_EXPR_ARRAY_ACCESS) {
            right_val = gs_eval_cast(eval, right_val, lval->type);
            if (right_val.is_return) {
                val = right_val;
                goto cleanup;
            }
        }

        *lval = right_val;
    }

    val = right_val;

cleanup:
    return val;
}

static gs_val_t gs_eval_expr_int(gs_eval_t *eval, gs_expr_t *expr) {
    GOLF_UNUSED(eval);
    return gs_val_int(expr->int_val);
}

static gs_val_t gs_eval_expr_float(gs_eval_t *eval, gs_expr_t *expr) {
    GOLF_UNUSED(eval);
    return gs_val_float(expr->float_val);
}

static gs_val_t gs_eval_expr_symbol(gs_eval_t *eval, gs_expr_t *expr)  {
    bool found_val = false;
    gs_val_t val;

    for (int i = eval->env.length - 1; i >= 0; i--) {
        gs_env_t *env = eval->env.data[i];
        gs_val_t *val0 = map_get(&env->val_map, expr->symbol);
        if (val0) {
            found_val = true;
            val = *val0;
            break;
        }
    }

    if (!found_val) {
        val = gs_val_error("Could not find symbol %s", expr->symbol);
        goto cleanup;
    }

cleanup:
    return val;
}

static gs_val_t gs_eval_expr_string(gs_eval_t *eval, gs_expr_t *expr) {
    golf_string_t *string = golf_alloc_tracked(sizeof(golf_string_t), "script/eval");
    vec_push(&eval->allocated_strings, string);
    golf_string_init(string, "script/eval", expr->string);
    return gs_val_string(string);
}

static gs_val_t gs_eval_expr_call(gs_eval_t *eval, gs_expr_t *expr) {
    gs_env_t *env = golf_alloc(sizeof(gs_env_t));
    map_init(&env->val_map, "script/eval");

    vec_gs_val_t vals;
    vec_init(&vals, "script/eval");

    gs_val_t val = gs_eval_expr(eval, expr->call.fn);
    if (val.is_return) goto cleanup;

    if (val.type == GS_VAL_FN) {
        gs_stmt_t *fn_stmt = (gs_stmt_t*) val.fn_stmt;
        if (expr->call.num_args != fn_stmt->fn_decl.num_args) {
            val = gs_val_error("Wrong number of args passed to function call");
            goto cleanup;
        }

        for (int i = 0; i < fn_stmt->fn_decl.num_args; i++) {
            const char *symbol = fn_stmt->fn_decl.arg_symbols[i].symbol;
            gs_val_type decl_type = fn_stmt->fn_decl.arg_types[i];
            gs_val_t arg_val = gs_eval_expr(eval, expr->call.args[i]);
            if (arg_val.is_return) {
                val = arg_val;
                goto cleanup;
            }

            arg_val = gs_eval_cast(eval, arg_val, decl_type);
            if (arg_val.is_return) {
                val = arg_val;
                goto cleanup;
            }

            map_set(&env->val_map, symbol, arg_val);
        }

        vec_push(&eval->env, env);
        val = gs_eval_stmt(eval, fn_stmt->fn_decl.body);
        if (val.type != GS_VAL_ERROR) val.is_return = false;
        (void)vec_pop(&eval->env);
    }
    else if (val.type == GS_VAL_C_FN) {
        for (int i = 0; i < expr->call.num_args; i++) {
            gs_val_t arg_val = gs_eval_expr(eval, expr->call.args[i]);
            if (arg_val.is_return) {
                val = arg_val;
                goto cleanup;
            }
            vec_push(&vals, arg_val);
        }

        val = val.c_fn(eval, vals.data, vals.length);
        if (val.type != GS_VAL_ERROR) val.is_return = false;
    }
    else {
        val = gs_val_error("Expected a function when evaling call expr");
        goto cleanup;
    }

cleanup:
    map_deinit(&env->val_map);
    golf_free(env);
    vec_deinit(&vals);
    return val;
}

static gs_val_t gs_eval_expr_member_access(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val = gs_eval_expr(eval, expr->member_access.val);
    if (val.is_return) goto cleanup;

    const char *sym = expr->member_access.member.symbol;
    if (val.type == GS_VAL_VEC2) {
        if (strcmp(sym, "x") == 0) {
            val = gs_val_float(val.vec2_val.x);
        }
        else if (strcmp(sym, "y") == 0) {
            val = gs_val_float(val.vec2_val.y);
        }
        else {
            val = gs_val_error("Invalid member for vec2");
        }
    }
    else if (val.type == GS_VAL_VEC3) {
        if (strcmp(sym, "x") == 0) {
            val = gs_val_float(val.vec3_val.x);
        }
        else if (strcmp(sym, "y") == 0) {
            val = gs_val_float(val.vec3_val.y);
        }
        else if (strcmp(sym, "z") == 0) {
            val = gs_val_float(val.vec3_val.z);
        }
        else {
            val = gs_val_error("Invalid member for vec3");
        }
    }
    else if (val.type == GS_VAL_LIST) {
        if (strcmp(sym, "length") == 0) {
            val = gs_val_int(val.list_val->length);
        }
        else {
            val = gs_val_error("Invalid member for list");
        }
    }
    else {
        val = gs_val_error("Invalid type for member access");
    }

cleanup:
    return val;
}

static gs_val_t gs_eval_expr_array_access(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val;

    gs_val_t list_val = gs_eval_expr(eval, expr->array_access.val);
    if (list_val.is_return) {
        val = list_val;
        goto cleanup;
    }

    gs_val_t arg_val = gs_eval_expr(eval, expr->array_access.arg);
    if (arg_val.is_return) {
        val = list_val;
        goto cleanup;
    }

    arg_val = gs_eval_cast(eval, arg_val, GS_VAL_INT);
    if (arg_val.is_return) {
        val = list_val;
        goto cleanup;
    }

    if (list_val.type != GS_VAL_LIST) {
        val = gs_val_error("Expected type list");
        goto cleanup;
    }

    if (arg_val.int_val < 0 || arg_val.int_val >= list_val.list_val->length) {
        val = gs_val_error("Invalid list index");
        goto cleanup;
    }

    val = list_val.list_val->data[arg_val.int_val];

cleanup:
    return val;
}

static gs_val_t gs_eval_expr_array_decl(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val;
    vec_gs_val_t *list = golf_alloc_tracked(sizeof(vec_gs_val_t), "script/eval");
    vec_init(list, "script/eval");

    for (int i = 0; i < expr->array_decl.num_args; i++) {
        gs_expr_t *arg_expr = expr->array_decl.args[i];
        gs_val_t arg_val = gs_eval_expr(eval, arg_expr);
        if (arg_val.is_return) {
            val = arg_val;
            goto error;
        }
        vec_push(list, arg_val);
    }

    vec_push(&eval->allocated_lists, list);
    return gs_val_list(list);

error:
    vec_deinit(list);
    golf_free(list);
    return val;
}

static gs_val_t gs_eval_expr_cast(gs_eval_t *eval, gs_expr_t *expr) {
    gs_val_t val = gs_eval_expr(eval, expr->cast.arg);
    if (val.is_return) goto cleanup;

    val = gs_eval_cast(eval, val, expr->cast.type);
    if (val.is_return) goto cleanup;

cleanup:
    return val;
}

gs_val_t gs_eval_cast(gs_eval_t *eval, gs_val_t val, gs_val_type type) {
    GOLF_UNUSED(eval);

    if (val.type == type) {
        return val;
    }

    if (val.type == GS_VAL_INT && type == GS_VAL_FLOAT) {
        return gs_val_float((float)val.int_val);
    }
    else if (val.type == GS_VAL_FLOAT && type == GS_VAL_INT) {
        return gs_val_int((int)val.float_val);
    }

    return gs_val_error("Unable to perform cast");
}

static int gs_val_to_int(gs_val_t val) {
    switch (val.type) {
        case GS_VAL_INT:
            return val.int_val;
        case GS_VAL_FLOAT:
            return (int) val.float_val;
        case GS_VAL_VOID:
        case GS_VAL_BOOL:
        case GS_VAL_VEC2:
        case GS_VAL_VEC3:
        case GS_VAL_LIST:
        case GS_VAL_STRING:
        case GS_VAL_FN:
        case GS_VAL_C_FN:
        case GS_VAL_ERROR:
        case GS_VAL_NUM_TYPES:
            assert(false);
    }
    return 0;
}

static float gs_val_to_float(gs_val_t val) {
    switch (val.type) {
        case GS_VAL_INT:
            return (float) val.int_val;
        case GS_VAL_FLOAT:
            return val.float_val;
        case GS_VAL_VOID:
        case GS_VAL_BOOL:
        case GS_VAL_VEC2:
        case GS_VAL_VEC3:
        case GS_VAL_LIST:
        case GS_VAL_STRING:
        case GS_VAL_FN:
        case GS_VAL_C_FN:
        case GS_VAL_ERROR:
        case GS_VAL_NUM_TYPES:
            assert(false);
    }
    return 0;
}

static void gs_debug_print_val(gs_val_t val) {
    switch (val.type) {
        case GS_VAL_INT:
            printf("[INT %d]", val.int_val);
            break;
        case GS_VAL_FLOAT:
            printf("[FLOAT %f]", val.float_val);
            break;
        case GS_VAL_VOID:
            printf("[VOID]");
            break;
        case GS_VAL_BOOL:
            printf("[BOOL %d]", val.bool_val);
            break;
        case GS_VAL_VEC2:
            printf("[VEC2 %f %f]", val.vec2_val.x, val.vec2_val.y);
            break;
        case GS_VAL_VEC3:
            printf("[VEC3 %f %f %f]", val.vec3_val.x, val.vec3_val.y, val.vec3_val.z);
            break;
        case GS_VAL_LIST:
            printf("[LIST ");
            for (int i = 0; i < val.list_val->length; i++) {
                gs_debug_print_val(val.list_val->data[i]);
                if (i != val.list_val->length - 1) {
                    printf(", ");
                }
            }
            printf("]");
            break;
        case GS_VAL_STRING:
            printf("[STRING %s]", val.string_val->cstr);
            break;
        case GS_VAL_FN:
            printf("[FN]");
            break;
        case GS_VAL_C_FN:
            printf("[C_FN]");
            break;
        case GS_VAL_ERROR:
            printf("[ERROR \"%s\"]", val.error_val);
            break;
        case GS_VAL_NUM_TYPES:
            assert(false);
    }
}

static gs_val_t gs_eval_binary_op_add(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_int(gs_val_to_int(left) + gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_float(gs_val_to_float(left) + gs_val_to_float(right));
    }
    else if (left.type == GS_VAL_VEC2 && right.type == GS_VAL_VEC2) {
        return gs_val_vec2(vec2_add(left.vec2_val, right.vec2_val));
    }
    else if (left.type == GS_VAL_VEC3 && right.type == GS_VAL_VEC3) {
        return gs_val_vec3(vec3_add(left.vec3_val, right.vec3_val));
    }

    return gs_val_error("Unable to add these types");
}

static gs_val_t gs_eval_binary_op_sub(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_int(gs_val_to_int(left) - gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_float(gs_val_to_float(left) - gs_val_to_float(right));
    }
    else if (left.type == GS_VAL_VEC2 && right.type == GS_VAL_VEC2) {
        return gs_val_vec2(vec2_sub(left.vec2_val, right.vec2_val));
    }
    else if (left.type == GS_VAL_VEC3 && right.type == GS_VAL_VEC3) {
        return gs_val_vec3(vec3_sub(left.vec3_val, right.vec3_val));
    }

    return gs_val_error("Unable to subtract these types");
}

static gs_val_t gs_eval_binary_op_mul(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_int(gs_val_to_int(left) * gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_float(gs_val_to_float(left) * gs_val_to_float(right));
    }
    else if (left.type == GS_VAL_VEC2 && (right.type == GS_VAL_INT || right.type == GS_VAL_FLOAT)) {
        return gs_val_vec2(vec2_scale(left.vec2_val, gs_val_to_float(right)));
    }
    else if (left.type == GS_VAL_VEC3 && (right.type == GS_VAL_INT || right.type == GS_VAL_FLOAT)) {
        return gs_val_vec3(vec3_scale(left.vec3_val, gs_val_to_float(right)));
    }
    else if (right.type == GS_VAL_VEC2 && (left.type == GS_VAL_INT || left.type == GS_VAL_FLOAT)) {
        return gs_val_vec2(vec2_scale(right.vec2_val, gs_val_to_float(left)));
    }
    else if (right.type == GS_VAL_VEC3 && (left.type == GS_VAL_INT || left.type == GS_VAL_FLOAT)) {
        return gs_val_vec3(vec3_scale(right.vec3_val, gs_val_to_float(left)));
    }

    return gs_val_error("Unable to multiply these types");
}

static gs_val_t gs_eval_binary_op_div(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_int(gs_val_to_int(left) / gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_float(gs_val_to_float(left) / gs_val_to_float(right));
    }
    else if (left.type == GS_VAL_VEC2 && (right.type == GS_VAL_INT || right.type == GS_VAL_FLOAT)) {
        return gs_val_vec2(vec2_scale(left.vec2_val, 1.0f / gs_val_to_float(right)));
    }
    else if (left.type == GS_VAL_VEC3 && (right.type == GS_VAL_INT || right.type == GS_VAL_FLOAT)) {
        return gs_val_vec3(vec3_scale(left.vec3_val, 1.0f / gs_val_to_float(right)));
    }

    return gs_val_error("Unable to divide these types");
}

static gs_val_t gs_eval_binary_op_lt(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_bool(gs_val_to_int(left) < gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_bool(gs_val_to_float(left) < gs_val_to_float(right));
    }

    return gs_val_error("Unable to less than these types");
}

static gs_val_t gs_eval_binary_op_gt(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_bool(gs_val_to_int(left) > gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_bool(gs_val_to_float(left) > gs_val_to_float(right));
    }

    return gs_val_error("Unable to greater than these types");
}

static gs_val_t gs_eval_binary_op_lte(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_bool(gs_val_to_int(left) <= gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_bool(gs_val_to_float(left) <= gs_val_to_float(right));
    }

    return gs_val_error("Unable to less than equal add these types");
}

static gs_val_t gs_eval_binary_op_gte(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_bool(gs_val_to_int(left) >= gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_bool(gs_val_to_float(left) >= gs_val_to_float(right));
    }

    return gs_val_error("Unable to greater than equal these types");
}

static gs_val_t gs_eval_binary_op_eq(gs_eval_t *eval, gs_val_t left, gs_val_t right) {
    GOLF_UNUSED(eval);

    if (left.type == GS_VAL_INT && right.type == GS_VAL_INT) {
        return gs_val_bool(gs_val_to_int(left) == gs_val_to_int(right));
    }
    else if ((left.type == GS_VAL_INT && right.type == GS_VAL_FLOAT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_INT) ||
            (left.type == GS_VAL_FLOAT && right.type == GS_VAL_FLOAT)) {
        return gs_val_bool(gs_val_to_float(left) == gs_val_to_float(right));
    }
    else if (left.type == GS_VAL_BOOL && right.type == GS_VAL_BOOL) {
        return gs_val_bool(left.bool_val == right.bool_val);
    }

    return gs_val_error("Unable to equal these types");
}

static gs_val_t gs_eval_lval(gs_eval_t *eval, gs_expr_t *expr, gs_val_t **lval) {
    if (expr->type == GS_EXPR_SYMBOL) {
        for (int i = eval->env.length - 1; i >= 0; i--) {
            gs_env_t *env = eval->env.data[i];
            gs_val_t *val = map_get(&env->val_map, expr->symbol);
            if (val) {
                *lval = val;
                return *val;
            }
        }

        return gs_val_error("Unable to find symbol %s", expr->symbol);
    }
    else if (expr->type == GS_EXPR_MEMBER_ACCESS) {
        return gs_eval_lval(eval, expr->member_access.val, lval);   
    }
    else if (expr->type == GS_EXPR_ARRAY_ACCESS) {
        gs_val_t val = gs_eval_expr(eval, expr->array_access.val);   
        if (val.is_return) return val;
        if (val.type != GS_VAL_LIST) {
            return gs_val_error("Expecing type list");
        }

        gs_val_t arg = gs_eval_expr(eval, expr->array_access.arg);
        if (arg.is_return) return val;
        arg = gs_eval_cast(eval, arg, GS_VAL_INT);
        if (arg.is_return) return val;

        int idx = arg.int_val;
        if (idx < 0) {
            return gs_val_error("Invalid list index");
        }
        else if (idx > 16000) {
            return gs_val_error("List index larger than max of 16000");
        }
        else if (idx >= val.list_val->length) {
            for (int i = val.list_val->length; i <= idx; i++) {
                vec_push(val.list_val, gs_val_int(0));
            }
        }

        *lval = &val.list_val->data[idx];
        return val.list_val->data[idx];
    }

    return gs_val_error("Expected lval");
}

static void gs_c_fn_print_val(gs_val_t val) {
    switch (val.type) {
        case GS_VAL_VOID:
            printf("<void>");
            break;
        case GS_VAL_BOOL:
            if (val.bool_val) {
                printf("true");
            }
            else {
                printf("false");
            }
            break;
        case GS_VAL_INT:
            printf("%d", val.int_val);
            break;
        case GS_VAL_FLOAT:
            printf("%f", val.float_val);
            break;
        case GS_VAL_VEC2:
            printf("<%f, %f", val.vec2_val.x, val.vec2_val.y);
            break;
        case GS_VAL_VEC3:
            printf("<%f, %f, %f>", val.vec3_val.x, val.vec3_val.y, val.vec3_val.z);
            break;
        case GS_VAL_LIST:
            printf("[");
            for (int i = 0; i < val.list_val->length; i++) {
                gs_c_fn_print_val(val.list_val->data[i]);
                if (i != val.list_val->length - 1) {
                    printf(", ");
                }
            }
            printf("]");
            break;
        case GS_VAL_STRING:
            printf("%s", val.string_val->cstr);
            break;
        case GS_VAL_FN:
            printf("<fn>");
            break;
        case GS_VAL_C_FN:
            printf("<c_fn>");
            break;
        case GS_VAL_ERROR:
            printf("<error: %s>", val.error_val);
            break;
        case GS_VAL_NUM_TYPES:
            assert(false);
            break;
    }
}

static gs_val_t gs_c_fn_print(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    GOLF_UNUSED(eval);

    for (int i = 0; i < num_vals; i++) {
        gs_c_fn_print_val(vals[i]);
    }
    printf("\n");
    return gs_val_void();
}

gs_val_t gs_c_fn_signature(gs_eval_t *eval, gs_val_t *vals, int num_vals, gs_val_type *types, int num_types) {
    if (num_types != num_vals) {
        return gs_val_error("Wrong num of args to function");
    }

    for (int i = 0; i < num_types; i++) {
        vals[i] = gs_eval_cast(eval, vals[i], types[i]);
        if (vals[i].is_return) return vals[i];
    }

    return gs_val_int(0);
}

static gs_val_t gs_c_fn_V2(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT, GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_vec2(V2(vals[0].float_val, vals[1].float_val));
}

static gs_val_t gs_c_fn_V3(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT, GS_VAL_FLOAT, GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_vec3(V3(vals[0].float_val, vals[1].float_val, vals[2].float_val));
}

static gs_val_t gs_c_fn_vec3_normalize(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_VEC3 };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_vec3(vec3_normalize(vals[0].vec3_val));
}

static gs_val_t gs_c_fn_vec3_length(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_VEC3 };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(vec3_length(vals[0].vec3_val));
}

static gs_val_t gs_c_fn_vec3_distance(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_VEC3, GS_VAL_VEC3 };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(vec3_distance(vals[0].vec3_val, vals[1].vec3_val));
}

static gs_val_t gs_c_fn_cos(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(cosf(vals[0].float_val));
}

static gs_val_t gs_c_fn_sin(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(sinf(vals[0].float_val));
}

static gs_val_t gs_c_fn_acos(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(acosf(vals[0].float_val));
}

static gs_val_t gs_c_fn_asin(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(asinf(vals[0].float_val));
}

static gs_val_t gs_c_fn_sqrt(gs_eval_t *eval, gs_val_t *vals, int num_vals) {
    gs_val_type signature_arg_types[] = { GS_VAL_FLOAT };
    int signature_arg_count = sizeof(signature_arg_types) / sizeof(signature_arg_types[0]);;
    gs_val_t sig = gs_c_fn_signature(eval, vals, num_vals, signature_arg_types, signature_arg_count);
    if (sig.is_return) return sig;

    return gs_val_float(sqrtf(vals[0].float_val));
}

static void gs_parser_error(gs_parser_t *parser, gs_token_t token, const char *fmt, ...) {
    if (parser->error) {
        golf_log_error("There is already an error!"); 
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(parser->error_string, MAX_ERROR_STRING_LEN, fmt, args);
    va_end(args);

    parser->error = true;
    parser->error_token = token;
}

static gs_token_t gs_peek(gs_parser_t *parser) {
    return parser->tokens.data[parser->cur_token];
}

static gs_token_t gs_peek_n(gs_parser_t *parser, int n) {
    int i = parser->cur_token + n;
    if (i >= parser->tokens.length) i = parser->tokens.length - 1;
    return parser->tokens.data[i];
}

static bool gs_peek_eof(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    return token.type == GS_TOKEN_EOF;
}

static bool gs_peek_char(gs_parser_t *parser, char c) {
    gs_token_t token = gs_peek(parser);
    return token.type == GS_TOKEN_CHAR && token.c == c;
}

static bool gs_peek_symbol(gs_parser_t *parser, const char *symbol) {
    gs_token_t token = gs_peek(parser);
    return token.type == GS_TOKEN_SYMBOL && strcmp(token.symbol, symbol) == 0;
}

static bool gs_peek_val_type(gs_parser_t *parser, gs_val_type *type) {
    return gs_peek_val_type_n(parser, type, 0);
}

static bool gs_peek_val_type_n(gs_parser_t *parser, gs_val_type *type, int n) {
    gs_token_t token = gs_peek_n(parser, n);
    if (token.type == GS_TOKEN_SYMBOL) {
        if (strcmp(token.symbol, "void") == 0) {
            if (type) *type = GS_VAL_INT;
            return true;
        }
        else if (strcmp(token.symbol, "bool") == 0) {
            if (type) *type = GS_VAL_BOOL;
            return true;
        }
        else if (strcmp(token.symbol, "int") == 0) {
            if (type) *type = GS_VAL_INT;
            return true;
        }
        else if (strcmp(token.symbol, "float") == 0) {
            if (type) *type = GS_VAL_FLOAT;
            return true;
        }
        else if (strcmp(token.symbol, "vec2") == 0) {
            if (type) *type = GS_VAL_VEC2;
            return true;
        }
        else if (strcmp(token.symbol, "vec3") == 0) {
            if (type) *type = GS_VAL_VEC3;
            return true;
        }
        else if (strcmp(token.symbol, "list") == 0) {
            if (type) *type = GS_VAL_LIST;
            return true;
        }
        else if (strcmp(token.symbol, "string") == 0) {
            if (type) *type = GS_VAL_STRING;
            return true;
        }
    }
    return false;
}

static void gs_eat(gs_parser_t *parser) {
    if (parser->cur_token < parser->tokens.length) {
        parser->cur_token++;
    }
}

static void gs_eat_n(gs_parser_t *parser, int n) {
    if (parser->cur_token + n < parser->tokens.length) {
        parser->cur_token += n;
    }
    else {
        parser->cur_token = parser->tokens.length - 1;
    }
}

static bool gs_eat_char(gs_parser_t *parser, char c) {
    if (gs_peek_char(parser, c)) {
        gs_eat(parser);
        return true;
    }
    return false;
}

static bool gs_eat_char_n(gs_parser_t *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        char c = (char)va_arg(ap, int);
        gs_token_t token = gs_peek_n(parser, i);
        if (token.type != GS_TOKEN_CHAR || token.c != c) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        gs_eat_n(parser, n);
    }

    return match;
}

static bool gs_eat_symbol(gs_parser_t *parser, const char *symbol) {
    if (gs_peek_symbol(parser, symbol)) {
        gs_eat(parser);
        return true;
    }
    return false;
}

static bool gs_eat_symbol_n(gs_parser_t *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *symbol = va_arg(ap, const char *);
        gs_token_t token = gs_peek_n(parser, i);
        if (token.type != GS_TOKEN_SYMBOL || strcmp(token.symbol, symbol) != 0) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        gs_eat_n(parser, n);
    }

    return match;
}

static bool gs_eat_val_type(gs_parser_t *parser, gs_val_type *type) {
    if (gs_peek_val_type(parser, type)) {
        gs_eat(parser);
        return true;
    }
    else {
        return false;
    }
}

static gs_val_type gs_parse_type(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);

    gs_val_type type;
    if (gs_eat_val_type(parser, &type)) {
        return type;
    }
    else {
        gs_parser_error(parser, token, "Expected a type when parsing");
        return 0;
    }
    return 0;
}

static gs_stmt_t *gs_parse_stmt(gs_parser_t *parser) {
    if (gs_eat_symbol(parser, "if")) {
        return gs_parse_stmt_if(parser);
    }
    else if (gs_eat_symbol(parser, "for")) {
        return gs_parse_stmt_for(parser);
    }
    else if (gs_eat_symbol(parser, "return")) {
        return gs_parse_stmt_return(parser);
    }
    else if (gs_eat_char(parser, '{')) {
        return gs_parse_stmt_block(parser);
    }
    else {
        if (gs_peek_val_type(parser, NULL)) {
            return gs_parse_stmt_decl(parser);
        }
        else {
            gs_token_t token = gs_peek(parser);
            gs_expr_t *expr = gs_parse_expr(parser);
            if (parser->error) return NULL;
            if (!gs_eat_char(parser, ';')) {
                gs_parser_error(parser, gs_peek(parser), "Expected ';' when parsing expression statement");
                return NULL;
            }
            return gs_stmt_expr_new(parser, token, expr);
        }
    }
}

static gs_stmt_t *gs_parse_stmt_if(gs_parser_t *parser) {
    vec_gs_expr_t conds;
    vec_gs_stmt_t stmts;
    gs_stmt_t *else_stmt = NULL;
    vec_init(&conds, "script/parser");
    vec_init(&stmts, "script/parser");

    gs_stmt_t *stmt = NULL;
    gs_token_t token = gs_peek(parser);

    {
        if (!gs_eat_char(parser, '(')) {
            gs_parser_error(parser, token, "Expected '(' when parsing if statment");
            goto cleanup;
        }

        gs_expr_t *cond = gs_parse_expr(parser);
        if (parser->error) goto cleanup;

        if (!gs_eat_char(parser, ')')) {
            gs_parser_error(parser, token, "Expected ')' when parsing if statement");
            goto cleanup;
        }

        gs_stmt_t *stmt = gs_parse_stmt(parser);
        if (parser->error) goto cleanup;

        vec_push(&conds, cond);
        vec_push(&stmts, stmt);
    }

    while (gs_eat_symbol_n(parser, 2, "else", "if")) {
        if (!gs_eat_char(parser, '(')) {
            gs_parser_error(parser, token, "Expected '(' when parsing if statement");
            goto cleanup;
        }

        gs_expr_t *cond = gs_parse_expr(parser);
        if (parser->error) goto cleanup;

        if (!gs_eat_char(parser, ')')) {
            gs_parser_error(parser, token, "Expected ')' when parsing if statement");
            goto cleanup;
        }

        gs_stmt_t *stmt = gs_parse_stmt(parser);
        if (parser->error) goto cleanup;

        vec_push(&conds, cond);
        vec_push(&stmts, stmt);
    }

    if (gs_eat_symbol(parser, "else")) {
        else_stmt = gs_parse_stmt(parser);
        if (parser->error) goto cleanup;
    }

    stmt = gs_stmt_if_new(parser, token, conds, stmts, else_stmt);

cleanup:
    vec_deinit(&conds);
    vec_deinit(&stmts);
    return stmt;
}

static gs_stmt_t *gs_parse_stmt_for(gs_parser_t *parser) {
    gs_expr_t *init, *cond, *inc;
    gs_stmt_t *body;

    gs_stmt_t *stmt = NULL;
    gs_token_t token = gs_peek(parser);

    if (!gs_eat_char(parser, '(')) {
        gs_parser_error(parser, gs_peek(parser), "Expected '(' when parsing for statement");
        goto cleanup;
    }

    gs_val_type decl_type;
    gs_token_t decl_symbol = gs_peek(parser);
    if (gs_eat_val_type(parser, &decl_type)) {
        decl_symbol = gs_peek(parser);
        if (decl_symbol.type != GS_TOKEN_SYMBOL) {
            gs_parser_error(parser, gs_peek(parser), "Expected symbol when parsing variable declaration in for statment");
            goto cleanup;
        }
        gs_eat(parser);

        if (!gs_eat_char(parser, '=')) {
            gs_parser_error(parser, gs_peek(parser), "Expected '=' when parsing variable declaration in for statment");
            goto cleanup;
        }
    }

    init = gs_parse_expr(parser);
    if (parser->error) goto cleanup;

    if (!gs_eat_char(parser, ';')) {
        gs_parser_error(parser, gs_peek(parser), "Expected ';' when parsing for statement");
        goto cleanup;
    }

    cond = gs_parse_expr(parser);
    if (parser->error) goto cleanup;

    if (!gs_eat_char(parser, ';')) {
        gs_parser_error(parser, gs_peek(parser), "Expected ';' when parsing for statement");
        goto cleanup;
    }

    inc = gs_parse_expr(parser);
    if (parser->error) goto cleanup;

    if (!gs_eat_char(parser, ')')) {
        gs_parser_error(parser, gs_peek(parser), "Expected ')' when parsing for statement");
        goto cleanup;
    }

    body = gs_parse_stmt(parser);
    if (parser->error) goto cleanup;

    stmt = gs_stmt_for_new(parser, token, decl_type, decl_symbol, init, cond, inc, body);

cleanup:
    return stmt;
}

static gs_stmt_t *gs_parse_stmt_return(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_stmt_t *stmt = NULL;

    gs_expr_t *expr = gs_parse_expr(parser);
    if (parser->error) goto cleanup;

    if (!gs_eat_char(parser, ';')) {
        gs_parser_error(parser, gs_peek(parser), "Expected ';' when parsing return statement");
        goto cleanup;
    }

    stmt = gs_stmt_return_new(parser, token, expr);

cleanup:
    return stmt;
}

static gs_stmt_t *gs_parse_stmt_block(gs_parser_t *parser) {
    vec_gs_stmt_t stmts;
    vec_init(&stmts, "script/parser");

    gs_token_t token = gs_peek(parser);
    gs_stmt_t *stmt = NULL;

    while (!gs_eat_char(parser, '}')) {
        gs_stmt_t *stmt = gs_parse_stmt(parser);
        if (parser->error) goto cleanup;

        vec_push(&stmts, stmt);
    }

    stmt = gs_stmt_block_new(parser, token, stmts);

cleanup:
    vec_deinit(&stmts);
    return stmt;
}

static gs_stmt_t *gs_parse_stmt_decl(gs_parser_t *parser) {
    vec_gs_token_t tokens;
    vec_init(&tokens, "script/parser");

    vec_gs_val_type_t arg_types;
    vec_init(&arg_types, "script/parser");

    vec_gs_token_t arg_symbols;
    vec_init(&arg_symbols, "script/parser");

    gs_expr_t *init = NULL;
    gs_stmt_t *stmt = NULL;

    gs_val_type type = gs_parse_type(parser);
    if (parser->error) goto cleanup;

    {
        gs_token_t symbol = gs_peek(parser);
        if (symbol.type != GS_TOKEN_SYMBOL) {
            gs_parser_error(parser, symbol, "Expected symbol when parsing declaration");
            goto cleanup;
        }
        gs_eat(parser);
        vec_push(&tokens, symbol);
    }

    if (gs_eat_char(parser, '(')) {
        const char *fn_name = tokens.data[0].symbol;

        if (!gs_eat_char(parser, ')')) {
            while (true) {
                gs_val_type arg_type = gs_parse_type(parser);
                if (parser->error) goto cleanup;

                gs_token_t arg_symbol = gs_peek(parser);
                if (arg_symbol.type != GS_TOKEN_SYMBOL) {
                    gs_parser_error(parser, arg_symbol, "Expected symbol when parsing function %s", fn_name);
                    goto cleanup;
                }
                gs_eat(parser);

                vec_push(&arg_types, arg_type);
                vec_push(&arg_symbols, arg_symbol);

                if (!gs_eat_char(parser, ',')) {
                    if (!gs_eat_char(parser, ')')) {
                        gs_parser_error(parser, gs_peek(parser), "Expected ')' when parsing function %s", fn_name);
                        goto cleanup;
                    }
                    break;
                }
            }
        }

        if (!gs_eat_char(parser, '{')) {
            gs_parser_error(parser, gs_peek(parser), "Expected '{' when parsing function %s", fn_name);
            goto cleanup;
        }

        gs_stmt_t *body = gs_parse_stmt_block(parser);
        if (parser->error) goto cleanup;

        stmt = gs_stmt_fn_decl_new(parser, type, tokens.data[0], arg_types, arg_symbols, body);
    }
    else {
        while (gs_eat_char(parser, ',')) {
            gs_token_t token = gs_peek(parser);
            if (token.type != GS_TOKEN_SYMBOL) {
                gs_parser_error(parser, token, "Expected symbol when parsing variable declaration");
                goto cleanup;
            }
            gs_eat(parser);
            vec_push(&tokens, token);
        }

        if (gs_eat_char(parser, '=')) {
            init = gs_parse_expr(parser);
            if (parser->error) goto cleanup;
        }

        if (!gs_eat_char(parser, ';')) {
            gs_parser_error(parser, gs_peek(parser), "Expected ';' when parsing variable declaration");
            goto cleanup;
        }

        stmt = gs_stmt_var_decl_new(parser, type, tokens, init);
    }

cleanup:
    vec_deinit(&tokens);
    vec_deinit(&arg_symbols);
    vec_deinit(&arg_types);
    return stmt;
}

static gs_expr_t *gs_parse_expr(gs_parser_t *parser) {
    return gs_parse_expr_assignment(parser);
}

static gs_expr_t *gs_parse_expr_assignment(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = gs_parse_expr_comparison(parser);
    if (parser->error) goto cleanup;

    while (true) {
        if (gs_eat_char(parser, '=')) {
            gs_expr_t *right = gs_parse_expr_assignment(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_assignment_new(parser, token, expr, right);
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_comparison(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = gs_parse_expr_term(parser);
    if (parser->error) goto cleanup;

    while (true) {
        if (gs_eat_char_n(parser, 2, '=', '=')) {
            gs_expr_t *right = gs_parse_expr_comparison(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_binary_op_new(parser, token, GS_BINARY_OP_EQ, expr, right);
        }
        else if (gs_eat_char_n(parser, 2, '<', '=')) {
            gs_expr_t *right = gs_parse_expr_comparison(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_binary_op_new(parser, token, GS_BINARY_OP_LTE, expr, right);
        }
        else if (gs_eat_char_n(parser, 2, '>', '=')) {
            gs_expr_t *right = gs_parse_expr_comparison(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_binary_op_new(parser, token, GS_BINARY_OP_GTE, expr, right);
        }
        else if (gs_eat_char(parser, '>')) {
            gs_expr_t *right = gs_parse_expr_comparison(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_binary_op_new(parser, token, GS_BINARY_OP_GT, expr, right);
        }
        else if (gs_eat_char(parser, '<')) {
            gs_expr_t *right = gs_parse_expr_comparison(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_binary_op_new(parser, token, GS_BINARY_OP_LT, expr, right);
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_term(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = gs_parse_expr_factor(parser);
    if (parser->error) goto cleanup;

    while (true) {
        gs_binary_op_type binary_op_type;
        if (gs_eat_char(parser, '+')) {
            binary_op_type = GS_BINARY_OP_ADD;
        }
        else if (gs_eat_char(parser, '-')) {
            binary_op_type = GS_BINARY_OP_SUB;
        }
        else {
            break;
        }

        gs_expr_t *right = gs_parse_expr_factor(parser);
        if (parser->error) goto cleanup;
        expr = gs_expr_binary_op_new(parser, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_factor(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = gs_parse_expr_cast(parser);
    if (parser->error) goto cleanup;

    while (true) {
        gs_binary_op_type binary_op_type;
        if (gs_eat_char(parser, '*')) {
            binary_op_type = GS_BINARY_OP_MUL;
        }
        else if (gs_eat_char(parser, '/')) {
            binary_op_type = GS_BINARY_OP_DIV;
        }
        else {
            break;
        }

        gs_expr_t *right = gs_parse_expr_cast(parser);
        if (parser->error) goto cleanup;
        expr = gs_expr_binary_op_new(parser, token, binary_op_type, expr, right);
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_cast(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = NULL;

    if (gs_peek_char(parser, '(')) {
        if (gs_peek_val_type_n(parser, NULL, 1)) {
            gs_eat(parser);

            gs_val_type type = gs_parse_type(parser);
            if (parser->error) goto cleanup;

            if (!gs_eat_char(parser, ')')) {
                gs_parser_error(parser, gs_peek(parser), "Expected ')' when parsing cast");
                goto cleanup;
            }

            gs_expr_t *arg = gs_parse_expr_member_access_or_array_access(parser);
            if (parser->error) goto cleanup;

            expr = gs_expr_cast_new(parser, token, type, arg);
        }
    }

    if (!expr) {
        expr = gs_parse_expr_member_access_or_array_access(parser);
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_member_access_or_array_access(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = gs_parse_expr_call(parser);
    if (parser->error) goto cleanup;

    while (true) {
        if (gs_eat_char(parser, '.')) {
            gs_token_t symbol = gs_peek(parser);
            if (symbol.type != GS_TOKEN_SYMBOL) {
                gs_parser_error(parser, symbol, "Expected symbol when parsing member access");
                goto cleanup;
            }
            gs_eat(parser);

            expr = gs_expr_member_access_new(parser, token, expr, symbol);
        }
        else if (gs_eat_char(parser, '[')) {
            gs_expr_t *arg = gs_parse_expr(parser);
            if (parser->error) goto cleanup;

            if (!gs_eat_char(parser, ']')) {
                gs_parser_error(parser, token, "Expected ']' when parsing array access");
                goto cleanup;
            }

            expr = gs_expr_array_access_new(parser, token, expr, arg);
        }
        else {
            break;
        }
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_call(gs_parser_t *parser) {
    vec_gs_expr_t args;
    vec_init(&args, "script/parser");

    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = gs_parse_expr_primary(parser);
    if (parser->error) goto cleanup;

    if (gs_eat_char(parser, '(')) {
        if (!gs_eat_char(parser, ')')) {
            while (true) {
                gs_expr_t *arg = gs_parse_expr(parser);
                if (parser->error) goto cleanup;

                vec_push(&args, arg);

                if (!gs_eat_char(parser, ',')) {
                    if (!gs_eat_char(parser, ')')) {
                        gs_parser_error(parser, gs_peek(parser), "Expected ')' when parsing call expr");
                        goto cleanup;
                    }
                    break;
                }
            }
        }

        expr = gs_expr_call_new(parser, token, expr, args);
    }

cleanup:
    vec_deinit(&args);
    return expr;
}

static gs_expr_t *gs_parse_expr_primary(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    gs_expr_t *expr = NULL;

    if (token.type == GS_TOKEN_INT) {
        expr = gs_expr_int_new(parser, token);
        gs_eat(parser);
    }
    else if (token.type == GS_TOKEN_FLOAT) {
        expr = gs_expr_float_new(parser, token);
        gs_eat(parser);
    }
    else if (token.type == GS_TOKEN_SYMBOL) {
        expr = gs_expr_symbol_new(parser, token);
        gs_eat(parser);
    }
    else if (token.type == GS_TOKEN_STRING) {
        expr = gs_expr_string_new(parser, token);
        gs_eat(parser);
    }
    else if (gs_eat_char(parser, '[')) {
        expr = gs_parse_expr_array_decl(parser);
    }
    else if (gs_eat_char(parser, '(')) {
        if (gs_peek_val_type(parser, NULL)) {
            expr = gs_parse_expr_cast(parser);
            if (parser->error) goto cleanup;
        }
        else {
            expr = gs_parse_expr(parser);
            if (parser->error) goto cleanup;

            if (!gs_eat_char(parser, ')')) {
                gs_parser_error(parser, token, "Expected ')' when parsing primary");
                goto cleanup;
            }
        }
    }
    else {
        gs_parser_error(parser, token, "Unknown token when parsing primary");
        goto cleanup;
    }

cleanup:
    return expr;
}

static gs_expr_t *gs_parse_expr_array_decl(gs_parser_t *parser) {
    gs_token_t token = gs_peek(parser);
    vec_gs_expr_t args;
    vec_init(&args, "script/parser");

    gs_expr_t *expr = NULL;

    if (!gs_eat_char(parser, ']')) {
        while (true) {
            gs_expr_t *arg = gs_parse_expr(parser);
            if (parser->error) goto cleanup;

            vec_push(&args, arg);
            if (!gs_eat_char(parser, ',')) {
                if (!gs_eat_char(parser, ']')) {
                    gs_parser_error(parser, gs_peek(parser), "Expected ']' when parsing array declaration");
                    goto cleanup;
                }
                break;
            }
        }
    }

    expr = gs_expr_array_decl_new(parser, token, args);

cleanup:
    vec_deinit(&args);
    return expr;
}

gs_val_t gs_val_default(gs_val_type type) {
    switch (type) {
        case GS_VAL_VOID:
            return gs_val_void();
        case GS_VAL_BOOL:
            return gs_val_bool(false);
        case GS_VAL_INT:
            return gs_val_int(0);
        case GS_VAL_FLOAT:
            return gs_val_float(0);
        case GS_VAL_VEC2:
            return gs_val_vec2(V2(0, 0));
        case GS_VAL_VEC3:
            return gs_val_vec3(V3(0, 0, 0));
        case GS_VAL_LIST:
            return gs_val_list(NULL);
        case GS_VAL_STRING:
            return gs_val_string(NULL);
        case GS_VAL_FN:
            assert(false);
            break;
        case GS_VAL_C_FN:
            assert(false);
            break;
        case GS_VAL_ERROR:
            assert(false);
            break;
        case GS_VAL_NUM_TYPES:
            assert(false);
            break;
    }
    return gs_val_void();
}

gs_val_t gs_val_void(void) {
    gs_val_t val;
    val.type = GS_VAL_VOID;
    val.is_return = false;
    return val;
}

gs_val_t gs_val_bool(bool v) {
    gs_val_t val;
    val.type = GS_VAL_BOOL;
    val.is_return = false;
    val.bool_val = v;
    return val;
}

gs_val_t gs_val_int(int v) {
    gs_val_t val;
    val.type = GS_VAL_INT;
    val.is_return = false;
    val.int_val = v;
    return val;
}

gs_val_t gs_val_float(float v) {
    gs_val_t val;
    val.type = GS_VAL_FLOAT;
    val.is_return = false;
    val.float_val = v;
    return val;
}

gs_val_t gs_val_vec2(vec2 v) {
    gs_val_t val;
    val.type = GS_VAL_VEC2;
    val.is_return = false;
    val.vec2_val = v;
    return val;
}

gs_val_t gs_val_vec3(vec3 v) {
    gs_val_t val;
    val.type = GS_VAL_VEC3;
    val.is_return = false;
    val.vec3_val = v;
    return val;
}

gs_val_t gs_val_list(vec_gs_val_t *list) {
    gs_val_t val;
    val.type = GS_VAL_LIST;
    val.is_return = false;
    val.list_val = list;
    return val;
}

gs_val_t gs_val_string(golf_string_t *string) {
    gs_val_t val;
    val.type = GS_VAL_STRING;
    val.is_return = false;
    val.string_val = string;
    return val;
}

gs_val_t gs_val_fn(gs_stmt_t *fn_stmt) {
    gs_val_t val;
    val.type = GS_VAL_FN;
    val.is_return = false;
    val.fn_stmt = fn_stmt;
    return val;
}

gs_val_t gs_val_c_fn(gs_val_t (*c_fn)(gs_eval_t *eval, gs_val_t *vals, int num_vals)) {
    gs_val_t val;
    val.type = GS_VAL_C_FN;
    val.is_return = false;
    val.c_fn = c_fn;
    return val;
}

gs_val_t gs_val_error(const char *v, ...) {
    static char error_string[2048]; 

    va_list args;
    va_start(args, v);
    vsnprintf(error_string, 2048, v, args);
    va_end(args);

    gs_val_t val;
    val.type = GS_VAL_ERROR;
    val.is_return = true;
    val.error_val = error_string;
    return val;
}

static golf_script_store_t _gs_store;
void golf_script_store_init(void) {
    vec_init(&_gs_store.scripts, "golf_script_store");
}

golf_script_store_t *golf_script_store_get(void) {
    return &_gs_store;
}

bool golf_script_load(golf_script_t *script, const char *path, const char *data, int data_len) {
    GOLF_UNUSED(data_len);

    vec_push(&_gs_store.scripts, script);

    snprintf(script->path, GOLF_FILE_MAX_PATH, "%s", path);
    script->error = NULL;
    vec_init(&script->parser.tokens, "script/parser");
    script->parser.cur_token = 0;
    script->parser.error = false;
    vec_init(&script->parser.allocated_memory, "script/parser");

    gs_tokenize(&script->parser, data);
    if (script->parser.error) {
        script->error = script->parser.error_string;
        return true;
    }
    if (false) {
        gs_debug_print_tokens(&script->parser.tokens);
    }

    gs_env_t *global_env = golf_alloc_tracked(sizeof(gs_env_t), "script/eval");
    map_init(&global_env->val_map, "script/eval");
    map_set(&global_env->val_map, "PI", gs_val_float(MF_PI));
    map_set(&global_env->val_map, "true", gs_val_bool(true));
    map_set(&global_env->val_map, "false", gs_val_bool(false));
    map_set(&global_env->val_map, "print", gs_val_c_fn(gs_c_fn_print));
    map_set(&global_env->val_map, "V2", gs_val_c_fn(gs_c_fn_V2));
    map_set(&global_env->val_map, "V3", gs_val_c_fn(gs_c_fn_V3));
    map_set(&global_env->val_map, "vec3_distance", gs_val_c_fn(gs_c_fn_vec3_distance));
    map_set(&global_env->val_map, "vec3_length", gs_val_c_fn(gs_c_fn_vec3_length));
    map_set(&global_env->val_map, "vec3_normalize", gs_val_c_fn(gs_c_fn_vec3_normalize));
    map_set(&global_env->val_map, "cos", gs_val_c_fn(gs_c_fn_cos));
    map_set(&global_env->val_map, "sin", gs_val_c_fn(gs_c_fn_sin));
    map_set(&global_env->val_map, "acos", gs_val_c_fn(gs_c_fn_acos));
    map_set(&global_env->val_map, "asin", gs_val_c_fn(gs_c_fn_asin));
    map_set(&global_env->val_map, "sqrt", gs_val_c_fn(gs_c_fn_sqrt));
    //map_set(&global_env->val_map, "terrain_model_add_point", gs_val_c_fn(gs_c_fn_terrain_model_add_point));
    //map_set(&global_env->val_map, "terrain_model_add_face", gs_val_c_fn(gs_c_fn_terrain_model_add_face));

    vec_init(&script->eval.env, "script/eval");
    vec_push(&script->eval.env, global_env);
    vec_init(&script->eval.allocated_strings, "script/eval");
    vec_init(&script->eval.allocated_lists, "script/eval");

    while (!gs_peek_eof(&script->parser)) {
        gs_stmt_t *stmt = gs_parse_stmt(&script->parser);
        if (script->parser.error) {
            script->error = script->parser.error_string;
            return true;
        }

        gs_val_t val = gs_eval_stmt(&script->eval, stmt);
        if (val.type == GS_VAL_ERROR) {
            script->error = val.error_val;
            return true;
        }
        if (false) {
            gs_debug_print_stmt(stmt, 0);
            gs_debug_print_val(val);
            printf("\n");
        }
    }

    return true;
}

bool golf_script_unload(golf_script_t *script) {
    for (int i = 0 ; i < _gs_store.scripts.length; i++) {
        if (_gs_store.scripts.data[i] == script) {
            vec_splice(&_gs_store.scripts, i, 1);
            break;
        }
    }

    for (int i = 0; i < script->parser.tokens.length; i++) {
        gs_token_t token = script->parser.tokens.data[i];
        if (token.type == GS_TOKEN_SYMBOL) {
            golf_free(token.symbol);
        }
        else if (token.type == GS_TOKEN_STRING) {
            golf_free(token.string);
        }
    }
    vec_deinit(&script->parser.tokens);

    for (int i = 0; i < script->parser.allocated_memory.length; i++) {
        golf_free(script->parser.allocated_memory.data[i]);
    }
    vec_deinit(&script->parser.allocated_memory);

    for (int i = 0; i < script->eval.env.length; i++) {
        gs_env_t *env = script->eval.env.data[i];
        map_deinit(&env->val_map);
        golf_free(env);
    }
    vec_deinit(&script->eval.env);

    for (int i = 0; i < script->eval.allocated_strings.length; i++) {
        golf_string_t *string = script->eval.allocated_strings.data[i];
        golf_string_deinit(string);
        golf_free(string);
    }
    vec_deinit(&script->eval.allocated_strings);

    for (int i = 0; i < script->eval.allocated_lists.length; i++) {
        vec_gs_val_t *list = script->eval.allocated_lists.data[i];
        vec_deinit(list);
        golf_free(list);
    }
    vec_deinit(&script->eval.allocated_lists);

    return true;
}

bool golf_script_get_val(golf_script_t *script, const char *name, gs_val_t *val) {
    for (int i = script->eval.env.length - 1; i >= 0; i--) {
        gs_env_t *env = script->eval.env.data[i];
        gs_val_t *val0 = map_get(&env->val_map, name);
        if (val0) {
            *val = *val0;
            return true;
        }
    }
    return false;
}

gs_val_t golf_script_eval_fn(golf_script_t *script, const char *name, gs_val_t *args, int num_args) {
    gs_val_t val;
    gs_env_t *env = golf_alloc_tracked(sizeof(gs_env_t), "script/eval");
    map_init(&env->val_map, "script/eval");
    vec_push(&script->eval.env, env);

    gs_val_t fn_val;
    if (!golf_script_get_val(script, name, &fn_val) || fn_val.type != GS_VAL_FN) {
        val = gs_val_error("Could not find function");
        goto cleanup;
    }

    gs_stmt_t *fn_stmt = (gs_stmt_t*) fn_val.fn_stmt;
    if (num_args != fn_stmt->fn_decl.num_args) {
        val = gs_val_error("Invalid number of args");
        goto cleanup;
    }

    for (int i = 0; i < num_args; i++) {
        const char *symbol = fn_stmt->fn_decl.arg_symbols[i].symbol;
        gs_val_type type = fn_stmt->fn_decl.arg_types[i];
        if (type != args[i].type) {
            val = gs_val_error("Invalid arg type");
            goto cleanup;
        }
        map_set(&env->val_map, symbol, args[i]);
    }

    val = gs_eval_stmt(&script->eval, fn_stmt->fn_decl.body);

cleanup:
    (void)vec_pop(&script->eval.env);
    map_deinit(&env->val_map);
    golf_free(env);
    return val;
}

void golf_script_set_c_fn(golf_script_t *script, const char *name, gs_val_t (*c_fn)(gs_eval_t *eval, gs_val_t *vals, int num_vals)) {
    gs_env_t *env = script->eval.env.data[0];
    map_set(&env->val_map, name, gs_val_c_fn(c_fn));
}
