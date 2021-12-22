#include "golf/script.h"

#include <stdio.h>

#include "golf/map.h"
#include "golf/vec.h"

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

static gs_token_t gs_token_eof(int line, int col);
static gs_token_t gs_token_char(char c, int line, int col);
static gs_token_t gs_token_symbol(const char *text, int *len, int line, int col);
static gs_token_t gs_token_number(const char *text, int *len, int line, int col);
static void gs_tokenize(const char *src, vec_gs_token_t *tokens);
static void gs_debug_print_tokens(vec_gs_token_t *tokens);

typedef enum gs_expr_type {
    GS_EXPR_UNARY_OP,
    GS_EXPR_BINARY_OP,
    GS_EXPR_CALL,
    GS_EXPR_ASSIGNMENT,
    GS_EXPR_CONST,
    GS_EXPR_SYMBOL,
    GS_EXPR_CAST,
} gs_expr_type;

typedef struct gs_expr {
    gs_expr_type type;
    union {
    }; 
} gs_expr_t;

typedef enum gs_stmt_type {
    GS_STMT_IF,
    GS_STMT_RETURN,
    GS_STMT_BLOCK,
    GS_STMT_FUNCTION_DECL,
    GS_STMT_VARIABLE_DECL,
    GS_STMT_EXPR,
    GS_STMT_FOR,
} gs_stmt_type;

typedef struct gs_stmt {
    gs_stmt_type type;
} gs_stmt_t;

typedef struct gs_parser {
    vec_gs_token_t tokens;
    int cur_token;
} gs_parser_t;

static void gs_parser_run(gs_parser_t *parser, const char *src);
static gs_expr_t *gs_parse_expr(gs_parser_t *parser);

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

static void gs_tokenize(const char *src, vec_gs_token_t *tokens) {
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
            vec_push(tokens, gs_token_eof(line, col));
            break;
        }
        else if (src[i] == '/' && src[i + 1] == '/') {
            while (src[i] && src[i] != '\n') i++;
        }
        else if (gs_is_char_start_of_symbol(src[i])) {
            int len = 0;
            vec_push(tokens, gs_token_symbol(src + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (gs_is_char_digit(src[i])) {
            int len;
            vec_push(tokens, gs_token_number(src + i, &len, line, col));
            i += len;
            col += len;
        }
        else if (src[i] == '+' ||
                src[i] == '-' ||
                src[i] == '*' ||
                src[i] == '/') {
            vec_push(tokens, gs_token_char(src[i], line, col));
            col++;
            i++;
        }
    }
}

static void gs_debug_print_tokens(vec_gs_token_t *tokens) {
    for (int i = 0; i < tokens->length; i++) {
        gs_token_t token = tokens->data[i];
        switch (token.type) {
            case GS_TOKEN_INT:
                printf("[INT %d]", token.int_val);
                break;
            case GS_TOKEN_FLOAT:
                printf("[FLOAT %f]", token.float_val);
                break;
            case GS_TOKEN_SYMBOL:
                printf("[SYM %s]", token.symbol);
                break;
            case GS_TOKEN_CHAR:
                printf("[CHAR %c]", token.c);
                break;
            case GS_TOKEN_EOF:
                printf("[EOF]");
                break;
        }
        printf("\n");
    }
}

static void gs_parser_run(gs_parser_t *parser, const char *src) {
    vec_init(&parser->tokens, "script/parser");
    parser->cur_token = 0;
    gs_tokenize("10 + 10 + x / y", &parser->tokens);
    gs_debug_print_tokens(&parser->tokens);
    gs_parse_expr(parser);
}

static gs_expr_t *gs_parse_expr(gs_parser_t *parser) {
}

void golf_script_init(void) {
    const char *src = "10 + 10 + x / y";
    gs_parser_t parser;
    gs_parser_run(&parser, src);
}
