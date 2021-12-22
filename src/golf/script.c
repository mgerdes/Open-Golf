#include "golf/script.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

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
	GS_BINARY_OP_ASSIGNMENT,
} gs_binary_op_type;

typedef enum gs_expr_type {
    GS_EXPR_BINARY_OP,
    GS_EXPR_INT,
    GS_EXPR_FLOAT,
    GS_EXPR_SYMBOL,
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
    }; 
} gs_expr_t;
typedef vec_t(gs_expr_t*) vec_gs_expr_t;

static gs_expr_t *gs_expr_int_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_float_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_symbol_new(gs_parser_t *parser, gs_token_t token);
static gs_expr_t *gs_expr_binary_op_new(gs_parser_t *parser, gs_token_t token, gs_binary_op_type type, gs_expr_t *left, gs_expr_t *right);
static void gs_debug_print_expr(gs_expr_t *expr);

typedef enum gs_stmt_type {
    GS_STMT_IF,
    GS_STMT_FOR,
    GS_STMT_BLOCK,
    GS_STMT_EXPR,
	GS_STMT_VAR_DECL,
} gs_stmt_type;

typedef struct gs_stmt gs_stmt_t;
typedef struct gs_stmt {
    gs_stmt_type type;
	union {
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
			int num_stmts;
			gs_stmt_t **stmts;
		} block_stmt;

		gs_expr_t *expr;
	};
} gs_stmt_t;
typedef vec_t(gs_stmt_t*) vec_gs_stmt_t;

static gs_stmt_t *gs_stmt_if_new(gs_parser_t *parser, gs_token_t token, vec_gs_expr_t conds, vec_gs_stmt_t stmts, gs_stmt_t *else_stmt);
static gs_stmt_t *gs_stmt_for_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *init, gs_expr_t *cond, gs_expr_t *inc, gs_stmt_t *body);
static gs_stmt_t *gs_stmt_block_new(gs_parser_t *parser, gs_token_t token, vec_gs_stmt_t stmts);
static gs_stmt_t *gs_stmt_expr_new(gs_parser_t *parser, gs_token_t token, gs_expr_t *expr);
static void gs_debug_print_stmt(gs_stmt_t *stmt, int tabs);

typedef struct gs_parser {
    vec_gs_token_t tokens;
    int cur_token;

	map_gs_type_t type_map;

	bool error;
	gs_token_t error_token;
} gs_parser_t;

static void gs_parser_run(gs_parser_t *parser, const char *src);
static void gs_parser_error(gs_parser_t *parser, gs_token_t token, const char *fmt, ...);
static gs_token_t gs_peek(gs_parser_t *parser);
static gs_token_t gs_peek_n(gs_parser_t *parser, int n);
static bool gs_peek_char(gs_parser_t *parser, char c);
static bool gs_peek_symbol(gs_parser_t *parser, const char *symbol);
static bool gs_peek_symbol_n(gs_parser_t *parser, int n, ...);
static void gs_eat(gs_parser_t *parser);
static bool gs_eat_char(gs_parser_t *parser, char c);
static bool gs_eat_symbol(gs_parser_t *parser, const char *symbol);
static bool gs_eat_symbol_n(gs_parser_t *parser, int n, ...);

static gs_type_t *gs_parse_type(gs_parser_t *parser);

static gs_stmt_t *gs_parse_stmt(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_if(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_for(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_block(gs_parser_t *parser);
static gs_stmt_t *gs_parse_stmt_var_decl(gs_parser_t *parser);

static gs_expr_t *gs_parse_expr(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_assignment(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_comparison(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_factor(gs_parser_t *parser);
static gs_expr_t *gs_parse_expr_term(gs_parser_t *parser) ;
static gs_expr_t *gs_parse_expr_primary(gs_parser_t *parser);

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
        if (src[i] == ' ' || src[i] == '\t' || src[i] == 'r') {
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
				src[i] == ';' || src[i] == '=' || src[i] == '>' || src[i] == '<') {
            vec_push(&parser->tokens, gs_token_char(src[i], line, col));
            col++;
            i++;
        }
		else {
			gs_token_t token = gs_token_char(src[i], line, col);
			gs_parser_error(parser, token, "Unknown character");
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

static void gs_debug_print_stmt(gs_stmt_t *stmt, int tabs) {
	for (int i = 0; i < tabs; i++) { 
		printf("  ");
	}

	switch (stmt->type) {
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
		case GS_STMT_EXPR: {
			gs_debug_print_expr(stmt->expr);
			printf(";\n");
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
		printf("TOKENIZE ERROR: \n");
		gs_debug_print_token(parser->error_token);
		printf("\n");
		return;
	}

    gs_debug_print_tokens(&parser->tokens);
    gs_stmt_t *stmt = gs_parse_stmt(parser);
	if (parser->error) {
		printf("PARSE ERROR:\n");
		gs_debug_print_token(parser->error_token);
		printf("\n");
		return;
	}
	gs_debug_print_stmt(stmt, 0);
	printf("\n");
}

static void gs_parser_error(gs_parser_t *parser, gs_token_t token, const char *fmt, ...) {
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

static bool gs_peek_char(gs_parser_t *parser, char c) {
	gs_token_t token = gs_peek(parser);
	return token.type == GS_TOKEN_CHAR && token.c == c;
}

static bool gs_peek_symbol(gs_parser_t *parser, const char *symbol) {
	gs_token_t token = gs_peek(parser);
	return token.type == GS_TOKEN_SYMBOL && strcmp(token.symbol, symbol) == 0;
}

static bool gs_peek_symbol_n(gs_parser_t *parser, int n, ...) {
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

    return match;
}

static void gs_eat(gs_parser_t *parser) {
	if (parser->cur_token < parser->tokens.length) {
		parser->cur_token++;
	}
}

static bool gs_eat_char(gs_parser_t *parser, char c) {
	if (gs_peek_char(parser, c)) {
		gs_eat(parser);
		return true;
	}
	return false;
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
		for (int i = 0; i < n; i++) {
			gs_eat(parser);
		}
	}

    return match;
}

static gs_type_t *gs_parse_type(gs_parser_t *parser) {
	gs_token_t token = gs_peek(parser);
	if (token.type != GS_TOKEN_SYMBOL) {
		gs_parser_error(parser, token, "Invalid type");
		return NULL;
	}

	gs_type_t **type = map_get(&parser->type_map, token.symbol);
	if (!type) {
		gs_parser_error(parser, token, "Invalid type");
		return NULL;
	}

	if (gs_eat_char(parser, '[')) {
		if (gs_eat_char(parser, ']')) {
			char full_type_name[64];
			snprintf(full_type_name, 64, "%s[]", token.symbol);

			gs_type_t **full_type = map_get(&parser->type_map, full_type_name);
			if (!full_type) {
				gs_type_t *array_type = malloc(sizeof(gs_type_t));	
				array_type->type = GS_TYPE_ARRAY;
				array_type->derived_type = *type;
				map_set(&parser->type_map, full_type_name, array_type);
				full_type = map_get(&parser->type_map, full_type_name);
			}
			*type = *full_type;
		}
		else {
			gs_parser_error(parser, gs_peek(parser), "Expected ']'");
			return NULL;
		}
	}

	return *type;
}

static gs_stmt_t *gs_parse_stmt(gs_parser_t *parser) {
	if (gs_eat_symbol(parser, "if")) {
		return gs_parse_stmt_if(parser);
	}
	else if (gs_eat_symbol(parser, "for")) {
		return gs_parse_stmt_for(parser);
	}
	else if (gs_eat_char(parser, '{')) {
		return gs_parse_stmt_block(parser);
	}
	else {
        gs_token_t token = gs_peek(parser);
        gs_type_t **base_type = NULL;
        if (token.base_type == GS_TOKEN_SYMBOL) {
            base_type = map_get(&parser->type_map, token.symbol);
        }

        if (base_type) {
            gs_type *type = gs_parse_type(parser);
            if (parser->error) return NULL;
        }
        else {
            gs_expr_t *expr = gs_parse_expr(parser);
            if (parser->error) return NULL;
            if (!gs_eat_char(parser, ';')) {
                gs_parser_error(parser, gs_peek(parser), "Expected ';'");
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
			gs_parser_error(parser, token, "Expected '('");
			goto cleanup;
		}

		gs_expr_t *cond = gs_parse_expr(parser);
		if (parser->error) goto cleanup;

		if (!gs_eat_char(parser, ')')) {
			gs_parser_error(parser, token, "Expected ')'");
			goto cleanup;
		}

		gs_stmt_t *stmt = gs_parse_stmt(parser);
		if (parser->error) goto cleanup;

		vec_push(&conds, cond);
		vec_push(&stmts, stmt);
	}

	while (gs_eat_symbol_n(parser, 2, "else", "if")) {
		if (!gs_eat_char(parser, '(')) {
			gs_parser_error(parser, token, "Expected '('");
			goto cleanup;
		}

		gs_expr_t *cond = gs_parse_expr(parser);
		if (parser->error) goto cleanup;

		if (!gs_eat_char(parser, ')')) {
			gs_parser_error(parser, token, "Expected ')'");
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
		gs_parser_error(parser, gs_peek(parser), "Expected '('");
		goto cleanup;
	}

	init = gs_parse_expr(parser);
	if (parser->error) goto cleanup;

	if (!gs_eat_char(parser, ';')) {
		gs_parser_error(parser, gs_peek(parser), "Expected ';'");
		goto cleanup;
	}

	cond = gs_parse_expr(parser);
	if (parser->error) goto cleanup;

	if (!gs_eat_char(parser, ';')) {
		gs_parser_error(parser, gs_peek(parser), "Expected ';'");
		goto cleanup;
	}

	inc = gs_parse_expr(parser);
	if (parser->error) goto cleanup;

	if (!gs_eat_char(parser, ')')) {
		gs_parser_error(parser, gs_peek(parser), "Expected ')'");
		goto cleanup;
	}

	body = gs_parse_stmt(parser);
	if (parser->error) goto cleanup;

	stmt = gs_stmt_for_new(parser, token, init, cond, inc, body);

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

static gs_stmt_t *gs_parse_stmt_var_decl(gs_parser_t *parser) {
	return NULL;
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
		if (gs_eat_char(parser, '>')) {
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
	gs_expr_t *expr = gs_parse_expr_primary(parser);
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

		gs_expr_t *right = gs_parse_expr_primary(parser);
		if (parser->error) goto cleanup;
		expr = gs_expr_binary_op_new(parser, token, binary_op_type, expr, right);
	}

cleanup:
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
	else {
		gs_parser_error(parser, token, "Unknown token");
		goto cleanup;
	}

cleanup:
	return expr;
}

void golf_script_init(void) {
    const char *src = "if (i) i + i; else if (i + 1) x + y; else x + y + z * w;";
	src = "for (i = 0; i < 10; i = i + 1) { if (i = 0) j = i; else j = 10; x = x + 10; }"; 
    gs_parser_t parser;
    gs_parser_run(&parser, src);
}
