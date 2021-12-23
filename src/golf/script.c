#include "golf/script.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "golf/log.h"
#include "golf/map.h"
#include "golf/vec.h"

typedef struct gs_parser gs_parser_t;

typedef enum gs_type_type {
	GS_TYPE_INT,
	GS_TYPE_FLOAT,
	GS_TYPE_VEC2,
	GS_TYPE_VEC3,
	GS_TYPE_ARRAY,
} gs_type_type;

typedef struct gs_type gs_type_t;
typedef struct gs_type {
	gs_type_type type;
	gs_type_t *derived_type;
} gs_type_t;
typedef map_t(gs_type_t*) map_gs_type_t;
typedef vec_t(gs_type_t*) vec_gs_type_t;

static void gs_debug_print_type(gs_type_t *type);

typedef enum gs_token_type {
    GS_TOKEN_INT,
    GS_TOKEN_FLOAT,
    GS_TOKEN_SYMBOL,
    GS_TOKEN_CHAR,
    GS_TOKEN_EOF,
} gs_token_type;

typedef struct gs_token {
    gs_token_type type;
    int line, col;
    union {
        int int_val;
        float float_val;
        const char *symbol;
        char c;
    };
} gs_token_t;
typedef vec_t(gs_token_t) vec_gs_token_t;

static bool gs_is_char_digit(char c);
static bool gs_is_char_start_of_symbol(char c);
static bool gs_is_char_part_of_symbol(char c);
static gs_token_t gs_token_eof(int line, int col);
static gs_token_t gs_token_char(char c, int line, int col);
static gs_token_t gs_token_symbol(const char *text, int *len, int line, int col);
static gs_token_t gs_token_number(const char *text, int *len, int line, int col);
static void gs_tokenize(gs_parser_t *parser, const char *src);
static void gs_debug_print_token(gs_token_t token);
static void gs_debug_print_tokens(vec_gs_token_t *tokens);

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
	GS_BINARY_OP_ASSIGNMENT,
} gs_binary_op_type;

typedef enum gs_expr_type {
    GS_EXPR_BINARY_OP,
    GS_EXPR_INT,
    GS_EXPR_FLOAT,
    GS_EXPR_SYMBOL,
    GS_EXPR_CALL,
    GS_EXPR_MEMBER_ACCESS,
    GS_EXPR_ARRAY_ACCESS,
    GS_EXPR_ARRAY_DECL,
} gs_expr_type;

typedef struct gs_expr gs_expr_t;
typedef struct gs_expr {
    gs_expr_type type;
	gs_token_t token;
    union {
		int int_val;
		float float_val;
		const char *symbol;

		struct {
			gs_binary_op_type type;
			gs_expr_t *left, *right;
		} binary_op;

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
    }; 
} gs_expr_t;
typedef vec_t(gs_expr_t*) vec_gs_expr_t;

static gs_expr_t *gs_expr_int_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_float_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_symbol_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_binary_op_new(gs_parser_t *parser, gs_token_t token, gs_binary_op_type type, gs_expr_t *left, gs_expr_t *right);
static gs_expr_t *gs_expr_call_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *fn, vec_gs_expr_t args);
static gs_expr_t *gs_expr_member_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_token_t member);
static gs_expr_t *gs_expr_array_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_expr_t *arg);
static gs_expr_t *gs_expr_array_decl_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t args);
static void gs_debug_print_expr(gs_expr_t *expr);

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
            gs_type_t *type;
            int num_ids;
            gs_token_t *tokens;
            gs_expr_t *init;
        } var_decl;

        struct {
            gs_type_t *return_type;
            gs_token_t symbol;
            int num_args;
            gs_type_t **arg_types;
            gs_token_t *arg_symbols;
            gs_stmt_t *body;
        } fn_decl;
	};
} gs_stmt_t;
typedef vec_t(gs_stmt_t*) vec_gs_stmt_t;

static gs_stmt_t *gs_stmt_if_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t conds, vec_gs_stmt_t stmts, gs_stmt_t *else_stmt);
static gs_stmt_t *gs_stmt_for_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *init, gs_expr_t *cond, gs_expr_t *inc, gs_stmt_t *body);
static gs_stmt_t *gs_stmt_return_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr);
static gs_stmt_t *gs_stmt_block_new(gs_parser_t *parser, gs_token_t token, vec_gs_stmt_t stmts);
static gs_stmt_t *gs_stmt_expr_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr);
static gs_stmt_t *gs_stmt_var_decl_new(gs_parser_t *parser, gs_type_t *type, vec_gs_token_t tokens, gs_expr_t *init);
static gs_stmt_t *gs_stmt_fn_decl_new(gs_parser_t *parser, gs_type_t *return_type, gs_token_t symbol, vec_gs_type_t arg_types, vec_gs_token_t arg_symbols, gs_stmt_t *body);
static void gs_debug_print_stmt(gs_stmt_t *stmt, int tabs);

#define MAX_ERROR_STRING_LEN 2048
typedef struct gs_parser {
    vec_gs_token_t tokens;
    int cur_token;

	map_gs_type_t type_map;

	bool error;
    char error_string[MAX_ERROR_STRING_LEN];
	gs_token_t error_token;
} gs_parser_t;

static void gs_parser_run(gs_parser_t *parser, const char *src);
static void gs_parser_error(gs_parser_t *parser, gs_token_t token, const char *fmt, ...);
static gs_token_t gs_peek(gs_parser_t *parser);
static gs_token_t gs_peek_n(gs_parser_t *parser, int n);
static bool gs_peek_eof(gs_parser_t *parser);
static bool gs_peek_char(gs_parser_t *parser, char c);
static bool gs_peek_symbol(gs_parser_t *parser, const char *symbol);
static void gs_eat(gs_parser_t *parser);
static void gs_eat_n(gs_parser_t *parser, int n);
static bool gs_eat_char(gs_parser_t *parser, char c);
static bool gs_eat_char_n(gs_parser_t *parser, int n, ...);
static bool gs_eat_symbol(gs_parser_t *parser, const char *symbol);
static bool gs_eat_symbol_n(gs_parser_t *parser, int n, ...);

static gs_type_t *gs_parse_type(gs_parser_t *parser);

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
static gs_expr_t *gs_parse_expr_member_access_or_array_access(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_call(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_primary(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_array_decl(gs_parser_t *parser);

static void gs_debug_print_type(gs_type_t *type) {
    switch (type->type) {
        case GS_TYPE_INT:
            printf("int");
            break;
        case GS_TYPE_FLOAT:
            printf("float");
            break;
        case GS_TYPE_VEC2:
            printf("vec2");
            break;
        case GS_TYPE_VEC3:
            printf("vec3");
            break;
        case GS_TYPE_ARRAY:
            gs_debug_print_type(type->derived_type);
            printf("[]");
            break;
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

    char *symbol = malloc((*len) + 1);
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

static gs_expr_t *gs_expr_int_new(gs_parser_t *parser, gs_token_t token) {
	gs_expr_t *expr = malloc(sizeof(gs_expr_t));
	expr->type = GS_EXPR_INT;
	expr->token = token;
	expr->int_val = token.int_val;
	return expr;
}

static gs_expr_t *gs_expr_float_new(gs_parser_t *parser, gs_token_t token) {
	gs_expr_t *expr = malloc(sizeof(gs_expr_t));
	expr->type = GS_EXPR_FLOAT;
	expr->token = token;
	expr->float_val = token.float_val;
	return expr;
}

static gs_expr_t *gs_expr_symbol_new(gs_parser_t *parser, gs_token_t token) {
	gs_expr_t *expr = malloc(sizeof(gs_expr_t));
	expr->type = GS_EXPR_SYMBOL;
	expr->token = token;
	expr->symbol = token.symbol;
	return expr;
}

static gs_expr_t *gs_expr_binary_op_new(gs_parser_t *parser, gs_token_t token, gs_binary_op_type type, gs_expr_t *left, gs_expr_t *right) {
	gs_expr_t *expr = malloc(sizeof(gs_expr_t));
	expr->type = GS_EXPR_BINARY_OP;
	expr->token = token;
	expr->binary_op.type = type;
	expr->binary_op.left = left;
	expr->binary_op.right = right;
	return expr;
}

static gs_expr_t *gs_expr_call_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *fn, vec_gs_expr_t args) {
    gs_expr_t *expr = malloc(sizeof(gs_expr_t));
    expr->type = GS_EXPR_CALL;
    expr->token = token;
    expr->call.fn = fn;
    expr->call.num_args = args.length;
    expr->call.args = malloc(sizeof(gs_expr_t*) * args.length);
    memcpy(expr->call.args, args.data, sizeof(gs_expr_t*) * args.length);
    return expr;
}

static gs_expr_t *gs_expr_member_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_token_t member) {
    gs_expr_t *expr = malloc(sizeof(gs_expr_t));
    expr->type = GS_EXPR_MEMBER_ACCESS;
    expr->token = token;
    expr->member_access.val = val;
    expr->member_access.member = member;
    return expr;
}

static gs_expr_t *gs_expr_array_access_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *val, gs_expr_t *arg) {
    gs_expr_t *expr = malloc(sizeof(gs_expr_t));
    expr->type = GS_EXPR_ARRAY_ACCESS;
    expr->token = token;
    expr->array_access.val = val;
    expr->array_access.arg = arg;
    return expr;
}

static gs_expr_t *gs_expr_array_decl_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t args) {
    gs_expr_t *expr = malloc(sizeof(gs_expr_t));
    expr->type = GS_EXPR_ARRAY_DECL;
    expr->token = token;
    expr->array_decl.num_args = args.length;
    expr->array_decl.args = malloc(sizeof(gs_expr_t*) * args.length);
    memcpy(expr->array_decl.args, args.data, sizeof(gs_expr_t*) * args.length);
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
                case GS_BINARY_OP_ASSIGNMENT:
                    printf("=");
                    break;
            }
            gs_debug_print_expr(expr->binary_op.right);
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
    }
}

static gs_stmt_t *gs_stmt_if_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t conds, vec_gs_stmt_t stmts, gs_stmt_t *else_stmt) {
	gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
	stmt->type = GS_STMT_IF;
	stmt->if_stmt.num_conds = conds.length;
	stmt->if_stmt.conds = malloc(sizeof(gs_expr_t*) * conds.length);
	memcpy(stmt->if_stmt.conds, conds.data, sizeof(gs_expr_t*) * conds.length);
	stmt->if_stmt.stmts = malloc(sizeof(gs_stmt_t*) * stmts.length);
	memcpy(stmt->if_stmt.stmts, stmts.data, sizeof(gs_stmt_t*) * stmts.length);
	stmt->if_stmt.else_stmt = else_stmt;
	return stmt;
}

static gs_stmt_t *gs_stmt_for_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *init, gs_expr_t *cond, gs_expr_t *inc, gs_stmt_t *body) {
	gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
	stmt->type = GS_STMT_FOR;
	stmt->for_stmt.init = init;
	stmt->for_stmt.cond = cond;
	stmt->for_stmt.inc = inc;
	stmt->for_stmt.body = body;
	return stmt;
}

static gs_stmt_t *gs_stmt_return_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr) {
    gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
    stmt->type = GS_STMT_RETURN;
    stmt->return_stmt.expr = expr;
    return stmt;
}

static gs_stmt_t *gs_stmt_block_new(gs_parser_t *parser, gs_token_t token, vec_gs_stmt_t stmts) {
	gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
	stmt->type = GS_STMT_BLOCK;
	stmt->block_stmt.num_stmts = stmts.length;
	stmt->block_stmt.stmts = malloc(sizeof(gs_stmt_t*) * stmts.length);
	memcpy(stmt->block_stmt.stmts, stmts.data, sizeof(gs_stmt_t*) * stmts.length);
	return stmt;
}

static gs_stmt_t *gs_stmt_expr_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr) {
	gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
	stmt->type = GS_STMT_EXPR;
	stmt->expr = expr;
	return stmt;
}

static gs_stmt_t *gs_stmt_var_decl_new(gs_parser_t *parser, gs_type_t *type, vec_gs_token_t tokens, gs_expr_t *init) {
    gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
    stmt->type = GS_STMT_VAR_DECL;
    stmt->var_decl.type = type;
    stmt->var_decl.num_ids = tokens.length;
    stmt->var_decl.tokens = malloc(sizeof(gs_token_t) * tokens.length);
    memcpy(stmt->var_decl.tokens, tokens.data, sizeof(gs_token_t) * tokens.length);
    stmt->var_decl.init = init;
    return stmt;
}

static gs_stmt_t *gs_stmt_fn_decl_new(gs_parser_t *parser, gs_type_t *return_type, gs_token_t symbol, vec_gs_type_t arg_types, vec_gs_token_t arg_symbols, gs_stmt_t *body) {
    gs_stmt_t *stmt = malloc(sizeof(gs_stmt_t));
    stmt->type = GS_STMT_FN_DECL;
    stmt->fn_decl.return_type = return_type;
    stmt->fn_decl.symbol = symbol;
    stmt->fn_decl.num_args = arg_types.length;
    stmt->fn_decl.arg_types = malloc(sizeof(gs_type_t*) * arg_types.length);
    memcpy(stmt->fn_decl.arg_types, arg_types.data, sizeof(gs_type_t*) * arg_types.length);
    stmt->fn_decl.arg_symbols = malloc(sizeof(gs_token_t) * arg_types.length);
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
            printf(") {\n");
            gs_debug_print_stmt(stmt->fn_decl.body, tabs + 1);
            printf("}\n");
            break;
        }
	}
}

static void gs_parser_init_base_type(gs_parser_t *parser, const char *name, gs_type_type type_type) {
	gs_type_t *type = malloc(sizeof(gs_type_t));
	type->type = type_type;
	type->derived_type = NULL;
	map_set(&parser->type_map, name, type);
}

static void gs_parser_run(gs_parser_t *parser, const char *src) {
    vec_init(&parser->tokens, "script/parser");
    parser->cur_token = 0;
	parser->error = false;

	map_init(&parser->type_map, "script/parser");
	gs_parser_init_base_type(parser, "int", GS_TYPE_INT);
	gs_parser_init_base_type(parser, "float", GS_TYPE_FLOAT);
	gs_parser_init_base_type(parser, "vec2", GS_TYPE_VEC2);
	gs_parser_init_base_type(parser, "vec3", GS_TYPE_VEC3);

    gs_tokenize(parser, src);
	if (parser->error) {
		printf("TOKENIZE ERROR: %s\n", parser->error_string);
		gs_debug_print_token(parser->error_token);
		printf("\n");
		return;
	}

    if (false) {
        gs_debug_print_tokens(&parser->tokens);
    }

    while (!gs_peek_eof(parser)) {
        gs_stmt_t *stmt = gs_parse_stmt(parser);
        if (parser->error) {
            printf("PARSE ERROR: %s\n", parser->error_string);
            gs_debug_print_token(parser->error_token);
            printf("\n");
            return;
        }

        if (true) {
            gs_debug_print_stmt(stmt, 0);
        }
    }
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

static gs_type_t *gs_parse_type(gs_parser_t *parser) {
	gs_token_t token = gs_peek(parser);
	if (token.type != GS_TOKEN_SYMBOL) {
		gs_parser_error(parser, token, "Invalid type");
		return NULL;
	}

	gs_type_t **base_type = map_get(&parser->type_map, token.symbol);
	if (!base_type) {
		gs_parser_error(parser, token, "Invalid type");
		return NULL;
	}
    gs_eat(parser);

    gs_type_t *type = *base_type;
	if (gs_eat_char(parser, '[')) {
		if (gs_eat_char(parser, ']')) {
			char full_type_name[64];
			snprintf(full_type_name, 64, "%s[]", token.symbol);

			gs_type_t **full_type = map_get(&parser->type_map, full_type_name);
			if (!full_type) {
				gs_type_t *array_type = malloc(sizeof(gs_type_t));	
				array_type->type = GS_TYPE_ARRAY;
				array_type->derived_type = type;
				map_set(&parser->type_map, full_type_name, array_type);
				full_type = map_get(&parser->type_map, full_type_name);
			}
			type = *full_type;
		}
		else {
			gs_parser_error(parser, gs_peek(parser), "Expected ']' when parsing type");
			return NULL;
		}
	}

	return type;
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
        gs_token_t token = gs_peek(parser);
        gs_type_t **base_type = NULL;
        if (token.type == GS_TOKEN_SYMBOL) {
            base_type = map_get(&parser->type_map, token.symbol);
        }

        if (base_type) {
            return gs_parse_stmt_decl(parser);
        }
        else {
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

	stmt = gs_stmt_for_new(parser, token, init, cond, inc, body);

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

    vec_gs_type_t arg_types;
    vec_init(&arg_types, "script/parser");

    vec_gs_token_t arg_symbols;
    vec_init(&arg_symbols, "script/parser");

    gs_expr_t *init = NULL;
    gs_stmt_t *stmt = NULL;

    gs_type_t *type = gs_parse_type(parser);
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
        if (!gs_eat_char(parser, ')')) {
            while (true) {
                gs_type_t *arg_type = gs_parse_type(parser);
                if (parser->error) goto cleanup;

                gs_token_t arg_symbol = gs_peek(parser);
                if (arg_symbol.type != GS_TOKEN_SYMBOL) {
                    gs_parser_error(parser, arg_symbol, "Expected symbol when parsing function declaration");
                    goto cleanup;
                }
                gs_eat(parser);

                vec_push(&arg_types, arg_type);
                vec_push(&arg_symbols, arg_symbol);

                if (!gs_eat_char(parser, ',')) {
                    if (!gs_eat_char(parser, ')')) {
                        gs_parser_error(parser, gs_peek(parser), "Expected ')' when parsing function declaration");
                        goto cleanup;
                    }
                    break;
                }
            }
        }

        if (!gs_eat_char(parser, '{')) {
            gs_parser_error(parser, gs_peek(parser), "Expected '{' when parsing function declaration");
            goto cleanup;
        }

        gs_stmt_t *body = gs_parse_stmt(parser);
        if (parser->error) goto cleanup;

        if (!gs_eat_char(parser, '}')) {
            gs_parser_error(parser, gs_peek(parser), "Expected '}' when parsing function declaration");
            goto cleanup;
        }

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

			expr = gs_expr_binary_op_new(parser, token, GS_BINARY_OP_ASSIGNMENT, expr, right);
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
	gs_expr_t *expr = gs_parse_expr_member_access_or_array_access(parser);
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

		gs_expr_t *right = gs_parse_expr_member_access_or_array_access(parser);
		if (parser->error) goto cleanup;
		expr = gs_expr_binary_op_new(parser, token, binary_op_type, expr, right);
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
    else if (gs_eat_char(parser, '[')) {
        expr = gs_parse_expr_array_decl(parser);
    }
    else if (gs_eat_char(parser, '(')) {
        expr = gs_parse_expr(parser);
        if (parser->error) goto cleanup;

        if (!gs_eat_char(parser, ')')) {
            gs_parser_error(parser, token, "Expected ')' when parsing primary");
            goto cleanup;
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

void golf_script_init(void) {
	const char *src = "[V2(1, 2), V2(3, 4), V3(5, 6), V2(7, 7)][0].x = 10; vec2[] points = [1, 2]; points[points[2].x] = 10; vec2 x = V2(100, 200)[1].x; vec2 y = V2(300, 400); vec2 z = x + y; float w = z.x + (z + z).y * 100; int i = 100 + x + z; for (i = 0; i < 10; i = i + 1) { int z = x; if (i = 0) j = i; else j = 10; x = x + 10; }"; 
    src = "int fib(int n) { if (n < 2) return n; else return fib(n - 1) + fib(n - 2); }";
    gs_parser_t parser;
    gs_parser_run(&parser, src);
}
