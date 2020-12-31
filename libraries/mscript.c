#include "mscript.h"

#include <assert.h>
#include <stdarg.h>

#include "array.h"
#include "log.h"
#include "map.h"

//
// TOKENIZER
//

enum mscript_token_type {
    MSCRIPT_TOKEN_INT,
    MSCRIPT_TOKEN_FLOAT,
    MSCRIPT_TOKEN_SYMBOL,
    MSCRIPT_TOKEN_CHAR,
    MSCRIPT_TOKEN_EOF,
};

struct mscript_token {
    enum mscript_token_type type;
    union {
        char *symbol; 
        int number_int; 
        float number_float;
        char character;
    };
};
array_t(struct mscript_token, array_mscript_token);

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

static bool is_character_char(char c) {
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

static struct mscript_token mscript_token_number_create(const char *text, int *len) {
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
        struct mscript_token token;
        token.type = MSCRIPT_TOKEN_FLOAT;
        token.number_float = (float)int_part + float_part;
        return token;
    }
    else {
        struct mscript_token token;
        token.type = MSCRIPT_TOKEN_INT;
        token.number_int = int_part;
        return token;
    }
}

static struct mscript_token mscript_token_char_create(char c) {
    struct mscript_token token;
    token.type = MSCRIPT_TOKEN_CHAR;
    token.character = c;
    return token;
}

static struct mscript_token mscript_token_symbol_create(const char *text, int *len) {
    *len = 0;
    while (is_char_part_of_symbol(text[*len])) {
        (*len)++;
    }

    char *symbol = malloc((*len) + 1);
    for (int i = 0; i < *len; i++) {
        symbol[i] = text[i];
    }
    symbol[*len] = 0;

    struct mscript_token token;
    token.type = MSCRIPT_TOKEN_SYMBOL;
    token.symbol = symbol;
    return token;
}

static struct mscript_token mscript_token_eof_create(void) {
    struct mscript_token token;
    token.type = MSCRIPT_TOKEN_EOF;
    return token;
}

static void mscript_tokens_debug_log(struct mscript_token *tokens) {
    while (true) {
        struct mscript_token token = *tokens;
        switch (token.type) {
            case MSCRIPT_TOKEN_INT:
                m_logf("[INT: %d]\n", token.number_int);
                break;
            case MSCRIPT_TOKEN_FLOAT:
                m_logf("[FLOAT: %d]\n", token.number_float);
                break;
            case MSCRIPT_TOKEN_SYMBOL:
                m_logf("[SYMBOL: %s]\n", token.symbol);
                break;
            case MSCRIPT_TOKEN_CHAR:
                m_logf("[CHAR: %c]\n", token.character);
                break;
            case MSCRIPT_TOKEN_EOF:
                m_logf("[EOF]\n");
                return;
                break;
        }
        tokens++;
    }
}

static bool mscript_tokenize(const char *prog, struct array_mscript_token *tokens) {
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
            array_push(tokens, mscript_token_eof_create());
            break;
        }
        else if ((prog[i] == '/') && (prog[i + 1] == '/')) {
            while (prog[i] && (prog[i] != '\n')) {
                i++;
            }
        }
        else if (is_char_start_of_symbol(prog[i])) {
            int len;
            array_push(tokens, mscript_token_symbol_create(prog + i, &len));
            i += len;
        }
        else if (is_char_digit(prog[i])) {
            int len;
            array_push(tokens, mscript_token_number_create(prog + i, &len));
            i += len;
        }
        else if (is_character_char(prog[i])) {
            array_push(tokens, mscript_token_char_create(prog[i]));
            i++;
        }
        else {
            m_logf("TOKENIZE ERROR: Unknown character: %c,%d\n", prog[i], prog[i]);
            m_logf("Line: %d\n", line);
            m_logf("Col: %d\n", col);
            return false;
        }
    }

    return true;
}

//
// PARSER
//

array_t(struct mscript_stmt *, array_mscript_stmt_ptr);
array_t(struct mscript_expr *, array_mscript_expr_ptr)

struct mscript_parser {
    const char *prog_text;
    int token_idx;
    struct array_mscript_token tokens_array; 
    struct array_mscript_stmt_ptr stmts_array;
};

static void mscript_parser_init(struct mscript_parser *parser, const char *prog_text);
static void mscript_parser_deinit(struct mscript_parser *parser);
static void mscript_parser_run(struct mscript_parser *parser);
static struct mscript_token mscript_parser_peek(struct mscript_parser *parser);
static struct mscript_token mscript_parser_peek_n(struct mscript_parser *parser, int n);
static void mscript_parser_eat(struct mscript_parser *parser);
static bool mscript_parser_match_char(struct mscript_parser *parser, char c);
static bool mscript_parser_match_char_n(struct mscript_parser *parser, int n, ...);
static bool mscript_parser_match_symbol(struct mscript_parser *parser, const char *symbol);
static bool mscript_parser_match_symbol_n(struct mscript_parser *parser, int n, ...);
static bool mscript_parser_check_symbol(struct mscript_parser *parser, const char *symbol);
static bool mscript_parser_check_type_n(struct mscript_parser *parser, int n, ...);
static bool mscript_parser_check_type_declaration(struct mscript_parser *parser);

enum mscript_type_type {
    MSCRIPT_TYPE_VOID, 
    MSCRIPT_TYPE_INT, 
    MSCRIPT_TYPE_FLOAT, 
    MSCRIPT_TYPE_STRUCT, 
    MSCRIPT_TYPE_ARRAY, 
};

struct mscript_type {
    enum mscript_type_type type, array_type;
    const char *struct_name;
};
array_t(struct mscript_type, array_mscript_type)

enum mscript_stmt_type {
    MSCRIPT_STMT_IF,
    MSCRIPT_STMT_RETURN,
    MSCRIPT_STMT_BLOCK,
    MSCRIPT_STMT_FUNCTION_DECLARATION,
    MSCRIPT_STMT_VARIABLE_DECLARATION,
    MSCRIPT_STMT_TYPE_DECLARATION,
    MSCRIPT_STMT_EXPR,
    MSCRIPT_STMT_FOR,
};

struct mscript_stmt {
    enum mscript_stmt_type type;
    union {
        struct {
            struct array_mscript_expr_ptr conds;
            struct array_mscript_stmt_ptr stmts;
            struct mscript_stmt *else_stmt;
        } if_stmt;

        struct {
            struct mscript_expr *expr;
        } return_stmt;

        struct {
            struct array_mscript_stmt_ptr stmts;
        } block;

        struct {
            struct mscript_type return_type;
            const char *name;
            struct array_mscript_type arg_types;
            struct array_char_ptr arg_names;
            struct mscript_stmt *body;
        } function_declaration;

        struct {
            struct mscript_type type;
            const char *name;
            struct mscript_expr *expr;
        } variable_declaration;

        struct {
            const char *name;
            struct array_mscript_type member_types;
            struct array_char_ptr member_names;
        } type_declaration;

        struct {
            struct mscript_expr *init, *cond, *inc;
            struct mscript_stmt *body;
        } for_stmt;

        struct mscript_expr *expr;
    };
};
typedef map_t(struct mscript_stmt *) map_mscript_stmt_ptr_t;

static struct mscript_stmt *mscript_stmt_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_if_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_block_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_for_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_return_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_declaration_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_variable_declaration_parse(struct mscript_parser *parser);
static struct mscript_stmt *mscript_stmt_function_declaration_parse(struct mscript_parser *parser);
static void mscript_stmt_debug_log(struct mscript_stmt *stmt) ;

enum mscript_expr_type {
    MSCRIPT_EXPR_UNARY_OP,
    MSCRIPT_EXPR_BINARY_OP,
    MSCRIPT_EXPR_CALL,
    MSCRIPT_EXPR_ARRAY_ACCESS,
    MSCRIPT_EXPR_MEMBER_ACCESS,
    MSCRIPT_EXPR_ASSIGNMENT,
    MSCRIPT_EXPR_INT,
    MSCRIPT_EXPR_FLOAT,
    MSCRIPT_EXPR_SYMBOL,
};

enum mscript_unary_op_type {
    MSCRIPT_UNARY_OP_POST_INC,
};

enum mscript_binary_op_type {
    MSCRIPT_BINARY_OP_ADD,
    MSCRIPT_BINARY_OP_SUB,
    MSCRIPT_BINARY_OP_MUL,
    MSCRIPT_BINARY_OP_DIV,
    MSCRIPT_BINARY_OP_LTE,
    MSCRIPT_BINARY_OP_LT,
    MSCRIPT_BINARY_OP_GTE,
    MSCRIPT_BINARY_OP_GT,
    MSCRIPT_BINARY_OP_EQ,
    MSCRIPT_BINARY_OP_NEQ,
};

struct mscript_expr {
    enum mscript_expr_type type;

    union {
        struct {
            enum mscript_unary_op_type type;
            struct mscript_expr *operand;
        } unary_op;

        struct {
            enum mscript_binary_op_type type;
            struct mscript_expr *left, *right;
        } binary_op;

        struct {
            struct mscript_expr *left, *right;
        } assignment;

        struct {
            struct mscript_expr *left, *right;
        } array_access;

        struct {
            struct mscript_expr *left;
            const char *member_name;
        } member_access;

        struct {
            const char *function_name;
            struct array_mscript_expr_ptr args;
        } call;

        int int_value;
        float float_value;
        const char *symbol;
    };
};

static struct mscript_expr *mscript_expr_parse(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_assignment(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_comparison(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_term(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_factor(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_unary(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_call(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_member_access(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_array_access(struct mscript_parser *parser);
static struct mscript_expr *mscript_expr_parse_primary(struct mscript_parser *parser);
static void mscript_expr_debug_log(struct mscript_expr *expr);

static void mscript_parser_init(struct mscript_parser *parser, const char *prog_text) {
    parser->prog_text = prog_text;
    parser->token_idx = 0;
    array_init(&parser->tokens_array);
    array_init(&parser->stmts_array);
}

static void mscript_parser_deinit(struct mscript_parser *parser) {
    array_deinit(&parser->tokens_array);
}

static void mscript_parser_run(struct mscript_parser *parser) {
    if (!mscript_tokenize(parser->prog_text, &(parser->tokens_array))) {
        return;
    }
    //mscript_tokens_debug_log(parser->tokens_array.data);

    while (mscript_parser_peek(parser).type != MSCRIPT_TOKEN_EOF) {
        struct mscript_stmt *stmt = mscript_stmt_declaration_parse(parser);
        array_push(&(parser->stmts_array), stmt);
        mscript_stmt_debug_log(stmt);
    }
}

static struct mscript_token mscript_parser_peek(struct mscript_parser *parser) {
    if (parser->token_idx >= parser->tokens_array.length) {
        // Return EOF
        return parser->tokens_array.data[parser->tokens_array.length - 1];
    }
    else {
        return parser->tokens_array.data[parser->token_idx];
    }
}

static struct mscript_token mscript_parser_peek_n(struct mscript_parser *parser, int n) {
    if (parser->token_idx + n >= parser->tokens_array.length) {
        // Return EOF
        return parser->tokens_array.data[parser->tokens_array.length - 1];
    }
    else {
        return parser->tokens_array.data[parser->token_idx + n];
    }
}

static void mscript_parser_eat(struct mscript_parser *parser) {
    parser->token_idx++;
}

static bool mscript_parser_match_char(struct mscript_parser *parser, char c) {
    struct mscript_token peek = mscript_parser_peek(parser);
    if (peek.type == MSCRIPT_TOKEN_CHAR && peek.character == c) {
        mscript_parser_eat(parser);
        return true;
    }
    else {
        return false;
    }
}

static bool mscript_parser_match_char_n(struct mscript_parser *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        char c = va_arg(ap, char);
        struct mscript_token peek = mscript_parser_peek_n(parser, i);
        if (peek.type != MSCRIPT_TOKEN_CHAR || peek.character != c) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        for (int i = 0; i < n; i++) {
            mscript_parser_eat(parser);
        }
    }

    return match;
}

static bool mscript_parser_match_symbol(struct mscript_parser *parser, const char *symbol) {
    struct mscript_token peek = mscript_parser_peek(parser);
    if (peek.type == MSCRIPT_TOKEN_SYMBOL && (strcmp(symbol, peek.symbol) == 0)) {
        mscript_parser_eat(parser);
        return true;
    }
    else {
        return false;
    }
}

static bool mscript_parser_match_symbol_n(struct mscript_parser *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *symbol = va_arg(ap, const char *);
        struct mscript_token peek = mscript_parser_peek_n(parser, i);
        if (peek.type != MSCRIPT_TOKEN_SYMBOL || (strcmp(symbol, peek.symbol) != 0)) {
            match = false;
        }
    }
    va_end(ap);

    if (match) {
        for (int i = 0; i < n; i++) {
            mscript_parser_eat(parser);
        }
    }

    return match;
}

static bool mscript_parser_check_symbol(struct mscript_parser *parser, const char *symbol) {
    struct mscript_token peek = mscript_parser_peek(parser);
    if (peek.type == MSCRIPT_TOKEN_SYMBOL && (strcmp(symbol, peek.symbol) == 0)) {
        return true;
    }
    else {
        return false;
    }
}

static bool mscript_parser_check_type_n(struct mscript_parser *parser, int n, ...) {
    bool match = true;

    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        enum mscript_token_type type = va_arg(ap, enum mscript_token_type);
        struct mscript_token peek = mscript_parser_peek_n(parser, i);
        if (peek.type != type) {
            match = false;
        }
    }
    va_end(ap);

    return match;
}

static bool mscript_parser_check_type_declaration(struct mscript_parser *parser) {
    // Type declaration's begin with 2 symbols or 1 symbol followed by [] for an array.
    return (mscript_parser_check_type_n(parser, 2, MSCRIPT_TOKEN_SYMBOL, MSCRIPT_TOKEN_SYMBOL) ||
            ((mscript_parser_peek_n(parser, 0).type == MSCRIPT_TOKEN_SYMBOL) &&
             (mscript_parser_peek_n(parser, 1).type == MSCRIPT_TOKEN_CHAR) &&
             (mscript_parser_peek_n(parser, 1).character == '[') &&
             (mscript_parser_peek_n(parser, 2).type == MSCRIPT_TOKEN_CHAR) &&
             (mscript_parser_peek_n(parser, 2).character == ']')));
}


static struct mscript_expr *mscript_expr_unary_op_new(enum mscript_unary_op_type type, struct mscript_expr *operand) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_UNARY_OP;
    expr->unary_op.type = type;
    expr->unary_op.operand = operand;
    return expr;
}

static struct mscript_expr *mscript_expr_binary_op_new(enum mscript_binary_op_type type, struct mscript_expr *left, struct mscript_expr *right) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_BINARY_OP;
    expr->binary_op.type = type;
    expr->binary_op.left = left;
    expr->binary_op.right = right;
    return expr;
}

static struct mscript_expr *mscript_expr_float_new(float float_value) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_FLOAT;
    expr->float_value = float_value;
    return expr;
}

static struct mscript_expr *mscript_expr_int_new(int int_value) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_INT;
    expr->int_value = int_value;
    return expr;
}

static struct mscript_expr *mscript_expr_symbol_new(const char *symbol) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_SYMBOL;
    expr->symbol = symbol;
    return expr;
}

static struct mscript_expr *mscript_expr_call_new(const char *function_name, struct array_mscript_expr_ptr args) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_CALL;
    expr->call.function_name = function_name;
    expr->call.args = args;
    return expr;
}

static struct mscript_expr *mscript_expr_member_access_new(struct mscript_expr *left, const char *member_name) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_MEMBER_ACCESS;
    expr->member_access.left = left;
    expr->member_access.member_name = member_name;
    return expr;
}

static struct mscript_expr *mscript_expr_array_access_new(struct mscript_expr *left, struct mscript_expr *right) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_ARRAY_ACCESS;
    expr->array_access.left = left;
    expr->array_access.right = right;
    return expr;
}

static struct mscript_expr *mscript_expr_assignment_new(struct mscript_expr *left, struct mscript_expr *right) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_ASSIGNMENT;
    expr->assignment.left = left;
    expr->assignment.right = right;
    return expr;
}

static void mscript_expr_debug_log(struct mscript_expr *expr) {
    switch (expr->type) {
        case MSCRIPT_EXPR_ARRAY_ACCESS:
            m_log("(");
            mscript_expr_debug_log(expr->array_access.left);
            m_log("[");
            mscript_expr_debug_log(expr->array_access.right);
            m_log("]");
            m_log(")");
            break;
        case MSCRIPT_EXPR_MEMBER_ACCESS:
            m_log("(");
            mscript_expr_debug_log(expr->member_access.left);
            m_logf(".%s)", expr->member_access.member_name);
            m_log(")");
            break;
        case MSCRIPT_EXPR_ASSIGNMENT:
            m_log("(");
            mscript_expr_debug_log(expr->assignment.left);
            m_log("=");
            mscript_expr_debug_log(expr->assignment.right);
            m_log(")");
            break;
        case MSCRIPT_EXPR_UNARY_OP:
            switch (expr->unary_op.type) {
                case MSCRIPT_UNARY_OP_POST_INC:
                    m_log("(");
                    mscript_expr_debug_log(expr->unary_op.operand);
                    m_log(")++");
                    break;
            }
            break;
        case MSCRIPT_EXPR_BINARY_OP:
            m_logf("(");
            mscript_expr_debug_log(expr->binary_op.left);
            switch (expr->binary_op.type) {
                case MSCRIPT_BINARY_OP_ADD:
                    m_log("+");
                    break;
                case MSCRIPT_BINARY_OP_SUB:
                    m_log("-");
                    break;
                case MSCRIPT_BINARY_OP_MUL:
                    m_log("*");
                    break;
                case MSCRIPT_BINARY_OP_DIV:
                    m_log("/");
                    break;
                case MSCRIPT_BINARY_OP_LTE:
                    m_log("<=");
                    break;
                case MSCRIPT_BINARY_OP_LT:
                    m_log("<");
                    break;
                case MSCRIPT_BINARY_OP_GTE:
                    m_log(">=");
                    break;
                case MSCRIPT_BINARY_OP_GT:
                    m_log(">");
                    break;
                case MSCRIPT_BINARY_OP_EQ:
                    m_log("==");
                    break;
                case MSCRIPT_BINARY_OP_NEQ:
                    m_log("!=");
                    break;
            }
            mscript_expr_debug_log(expr->binary_op.right);
            m_logf(")");
            break;
        case MSCRIPT_EXPR_INT:
            m_logf("%d", expr->int_value);
            break;
            break;
        case MSCRIPT_EXPR_FLOAT:
            m_logf("%f", expr->float_value);
            break;
        case MSCRIPT_EXPR_SYMBOL:
            m_logf("%s", expr->symbol);
            break;
        case MSCRIPT_EXPR_CALL:
            m_logf("%s(", expr->call.function_name);
            for (int i = 0; i < expr->call.args.length; i++) {
                mscript_expr_debug_log(expr->call.args.data[i]);
                if (i != expr->call.args.length - 1) {
                    m_logf(",");
                }
            }
            m_logf(")");
            break;
    }
}

static struct mscript_expr *mscript_expr_parse_primary(struct mscript_parser *parser) {
    struct mscript_token token = mscript_parser_peek(parser);
    if (token.type == MSCRIPT_TOKEN_INT) {
        mscript_parser_eat(parser);
        return mscript_expr_int_new(token.number_int);
    }
    else if (token.type == MSCRIPT_TOKEN_FLOAT) {
        mscript_parser_eat(parser);
        return mscript_expr_float_new(token.number_float);
    }
    else if (token.type == MSCRIPT_TOKEN_SYMBOL) {
        mscript_parser_eat(parser);
        return mscript_expr_symbol_new(token.symbol);
    }
    else if (mscript_parser_match_char(parser, '(')) {
        struct mscript_expr *expr = mscript_expr_parse(parser);
        if (!mscript_parser_match_char(parser, ')')) {
            assert(false);
        }
        return expr;
    }
    else {
        assert(false);
        return NULL;
    }
}

static struct mscript_expr *mscript_expr_parse_array_access(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_primary(parser);

    if (mscript_parser_match_char(parser, '[')) {
        struct mscript_expr *right = mscript_expr_parse(parser);
        expr = mscript_expr_array_access_new(expr, right);

        if (!mscript_parser_match_char(parser, ']')) {
            assert(false);
        }
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_member_access(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_array_access(parser);

    while (mscript_parser_match_char(parser, '.')) {
        struct mscript_expr *right = mscript_expr_parse_array_access(parser);
        if (right->type != MSCRIPT_EXPR_SYMBOL) {
            assert(false);
        }
        expr = mscript_expr_member_access_new(expr, right->symbol);
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_call(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_member_access(parser);

    if (mscript_parser_match_char(parser, '(')) {
        struct array_mscript_expr_ptr args;
        array_init(&args);

        if (!mscript_parser_match_char(parser, ')')) {
            while (true) {
                struct mscript_expr *arg = mscript_expr_parse(parser);
                array_push(&args, arg);

                if (!mscript_parser_match_char(parser, ',')) {
                    if (!mscript_parser_match_char(parser, ')')) {
                        assert(false);
                    }
                    break;
                }
            }
        }

        if (expr->type != MSCRIPT_EXPR_SYMBOL) {
            assert(false);
        }
        expr = mscript_expr_call_new(expr->symbol, args);
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_unary(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_call(parser);

    if (mscript_parser_match_char_n(parser, 2, '+', '+')) {
        expr = mscript_expr_unary_op_new(MSCRIPT_UNARY_OP_POST_INC, expr);
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_factor(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_unary(parser);

    while (true) {
        if (mscript_parser_match_char(parser, '*')) {
            struct mscript_expr *right = mscript_expr_parse_unary(parser);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_MUL, expr, right); 
        }
        else if (mscript_parser_match_char(parser, '/')) {
            struct mscript_expr *right = mscript_expr_parse_unary(parser);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_DIV, expr, right); 
        }
        else {
            break;
        }
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_term(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_factor(parser); 

    while (true) {
        if (mscript_parser_match_char(parser, '+')) {
            struct mscript_expr *right = mscript_expr_parse_factor(parser);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_ADD, expr, right); 
        }
        else if (mscript_parser_match_char(parser, '-')) {
            struct mscript_expr *right = mscript_expr_parse_factor(parser);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_SUB, expr, right); 
        }
        else {
            break;
        }
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_comparison(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_term(parser);

    while (true) {
        if (mscript_parser_match_char(parser, '<')) {
            if (mscript_parser_match_char(parser, '=')) {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_LTE, expr, right);
            }
            else {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_LT, expr, right);
            }
        }
        else if (mscript_parser_match_char(parser, '>')) {
            if (mscript_parser_match_char(parser, '=')) {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_GTE, expr, right);
            }
            else {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_GT, expr, right);
            }
        }
        else if (mscript_parser_match_char_n(parser, 2, '=', '=')) {
            struct mscript_expr *right = mscript_expr_parse_term(parser);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_EQ, expr, right);
        }
        else if (mscript_parser_match_char_n(parser, 2, '!', '=')) {
            struct mscript_expr *right = mscript_expr_parse_term(parser);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_NEQ, expr, right);
        }
        else {
            break;
        }
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse_assignment(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_comparison(parser);

    while (true) {
        if (mscript_parser_match_char(parser, '=')) {
            struct mscript_expr *right = mscript_expr_parse_assignment(parser);
            expr = mscript_expr_assignment_new(expr, right);
        }
        else {
            break;
        }
    }

    return expr;
}

static struct mscript_expr *mscript_expr_parse(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse_assignment(parser);
    return expr;
}

static void mscript_type_debug_log(struct mscript_type type) {
    enum mscript_type_type t = type.type;
    if (type.type == MSCRIPT_TYPE_ARRAY) {
        t = type.array_type;
    }

    switch (t) {
        case MSCRIPT_TYPE_VOID:
            m_logf("void");
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
    }

    if (type.type == MSCRIPT_TYPE_ARRAY) {
        m_logf("[]");
    }
}

static struct mscript_type mscript_parse_type(struct mscript_parser *parser) {
    struct mscript_type type;

    if (mscript_parser_match_symbol(parser, "void")) {
        type.type = MSCRIPT_TYPE_VOID;
    }
    else if (mscript_parser_match_symbol(parser, "int")) {
        type.type = MSCRIPT_TYPE_INT;
    }
    else if (mscript_parser_match_symbol(parser, "float")) {
        type.type = MSCRIPT_TYPE_FLOAT;
    }
    else {
        struct mscript_token token = mscript_parser_peek(parser);
        if (token.type != MSCRIPT_TOKEN_SYMBOL) {
            assert(false);
        }
        mscript_parser_eat(parser);

        type.type = MSCRIPT_TYPE_STRUCT;
        type.struct_name = token.symbol;
    }

    if (mscript_parser_match_char_n(parser, 2, '[', ']')) {
        type.array_type = type.type;
        type.type = MSCRIPT_TYPE_ARRAY;
    }

    return type;
}

static struct mscript_stmt *mscript_stmt_if_new(struct array_mscript_expr_ptr conds, struct array_mscript_stmt_ptr stmts, struct mscript_stmt *else_stmt) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_IF;
    stmt->if_stmt.conds = conds;
    stmt->if_stmt.stmts = stmts;
    stmt->if_stmt.else_stmt = else_stmt;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_block_new(struct array_mscript_stmt_ptr stmts) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_BLOCK;
    stmt->block.stmts = stmts;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_return_new(struct mscript_expr *expr) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_RETURN;
    stmt->return_stmt.expr = expr;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_for_new(struct mscript_expr *init, struct mscript_expr *cond, struct mscript_expr *inc, 
        struct mscript_stmt *body) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_FOR;
    stmt->for_stmt.init = init;
    stmt->for_stmt.cond = cond;
    stmt->for_stmt.inc = inc;
    stmt->for_stmt.body = body;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_function_declaration_new(struct mscript_type return_type, const char *name,
        struct array_mscript_type arg_types, struct array_char_ptr arg_names, struct mscript_stmt *body_stmt) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_FUNCTION_DECLARATION;
    stmt->function_declaration.return_type = return_type;
    stmt->function_declaration.name = name;
    stmt->function_declaration.arg_types = arg_types;
    stmt->function_declaration.arg_names = arg_names;
    stmt->function_declaration.body = body_stmt;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_expr_new(struct mscript_expr *expr) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_EXPR;
    stmt->expr = expr;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_variable_declaration_new(struct mscript_type type, const char *name, struct mscript_expr *expr) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_VARIABLE_DECLARATION;
    stmt->variable_declaration.type = type;
    stmt->variable_declaration.name = name;
    stmt->variable_declaration.expr = expr;
    return stmt;
}

static struct mscript_stmt *mscript_stmt_type_declaration_new(const char *name, 
        struct array_mscript_type member_types, struct array_char_ptr member_names) {
    struct mscript_stmt *stmt = malloc(sizeof(struct mscript_stmt));
    stmt->type = MSCRIPT_STMT_TYPE_DECLARATION;
    stmt->type_declaration.name = name;
    stmt->type_declaration.member_types = member_types;
    stmt->type_declaration.member_names = member_names;
    return stmt;
}

static void mscript_stmt_debug_log(struct mscript_stmt *stmt)  {
    switch (stmt->type) {
        case MSCRIPT_STMT_IF:
            m_logf("if (");
            mscript_expr_debug_log(stmt->if_stmt.conds.data[0]);
            m_logf(") ");
            mscript_stmt_debug_log(stmt->if_stmt.stmts.data[0]);
            for (int i = 1; i < stmt->if_stmt.conds.length; i++) {
                m_logf("else if (");
                mscript_expr_debug_log(stmt->if_stmt.conds.data[i]);
                m_logf(") ");
                mscript_stmt_debug_log(stmt->if_stmt.stmts.data[i]);
            }
            if (stmt->if_stmt.else_stmt) {
                m_logf("else ");
                mscript_stmt_debug_log(stmt->if_stmt.else_stmt);
            }
            break;
        case MSCRIPT_STMT_FOR:
            m_logf("for (");
            mscript_expr_debug_log(stmt->for_stmt.init);
            m_logf(";");
            mscript_expr_debug_log(stmt->for_stmt.cond);
            m_logf(";");
            mscript_expr_debug_log(stmt->for_stmt.inc);
            m_logf(") ");
            mscript_stmt_debug_log(stmt->for_stmt.body);
            break;
        case MSCRIPT_STMT_RETURN:
            m_logf("return ");
            mscript_expr_debug_log(stmt->return_stmt.expr);
            m_logf(";\n");
            break;
        case MSCRIPT_STMT_BLOCK:
            m_logf("{\n");
            for (int i = 0; i < stmt->block.stmts.length; i++) {
                mscript_stmt_debug_log(stmt->block.stmts.data[i]);
            }
            m_logf("}\n");
            break;
        case MSCRIPT_STMT_FUNCTION_DECLARATION:
            mscript_type_debug_log(stmt->function_declaration.return_type);
            m_logf(" %s(", stmt->function_declaration.name);
            for (int i = 0; i < stmt->function_declaration.arg_types.length; i++) {
                mscript_type_debug_log(stmt->function_declaration.arg_types.data[i]);
                m_logf(" %s", stmt->function_declaration.arg_names.data[i]);
                if (i != stmt->function_declaration.arg_types.length - 1) {
                    m_logf(",");
                }
            }
            m_logf(")");
            mscript_stmt_debug_log(stmt->function_declaration.body);
            break;
        case MSCRIPT_STMT_VARIABLE_DECLARATION:
            mscript_type_debug_log(stmt->variable_declaration.type);
            m_logf(" %s", stmt->variable_declaration.name);
            if (stmt->variable_declaration.expr) {
                m_logf(" = ");
                mscript_expr_debug_log(stmt->variable_declaration.expr);
            }
            m_logf(";\n");
            break;
        case MSCRIPT_STMT_EXPR:
            mscript_expr_debug_log(stmt->expr);
            m_logf(";\n");
            break;
        case MSCRIPT_STMT_TYPE_DECLARATION:
            m_logf("struct %s {\n", stmt->type_declaration.name);
            for (int i = 0; i < stmt->type_declaration.member_types.length; i++) {
                mscript_type_debug_log(stmt->type_declaration.member_types.data[i]);
                m_logf(" %s;\n", stmt->type_declaration.member_names.data[i]);
            }
            m_logf("}\n");
            break;
    }
}

static struct mscript_stmt *mscript_stmt_if_parse(struct mscript_parser *parser) {
    if (!mscript_parser_match_char(parser, '(')) {
        assert(false);
    }

    struct array_mscript_expr_ptr conds;
    array_init(&conds);
    struct array_mscript_stmt_ptr stmts;
    array_init(&stmts);
    struct mscript_stmt *else_stmt = NULL;

    {
        array_push(&conds, mscript_expr_parse(parser));

        if (!mscript_parser_match_char(parser, ')')) {
            assert(false);
        }

        array_push(&stmts, mscript_stmt_parse(parser));
    }

    while (true) {
        if (mscript_parser_match_symbol_n(parser, 2, "else", "if")) {
            if (!mscript_parser_match_char(parser, '(')) {
                assert(false);
            }

            array_push(&conds, mscript_expr_parse(parser));

            if (!mscript_parser_match_char(parser, ')')) {
                assert(false);
            }

            array_push(&stmts, mscript_stmt_parse(parser));
        }
        else if (mscript_parser_match_symbol(parser, "else")) {
            else_stmt = mscript_stmt_parse(parser);
            break;
        }
        else {
            break;
        }
    }

    return mscript_stmt_if_new(conds, stmts, else_stmt);
}

static struct mscript_stmt *mscript_stmt_block_parse(struct mscript_parser *parser) {
    struct array_mscript_stmt_ptr stmts;
    array_init(&stmts);

    while (true) {
        if (mscript_parser_match_char(parser, '}')) {
            break;
        }

        array_push(&stmts, mscript_stmt_parse(parser));
    }

    return mscript_stmt_block_new(stmts);
}

static struct mscript_stmt *mscript_stmt_for_parse(struct mscript_parser *parser) {
    if (!mscript_parser_match_char(parser, '(')) {
        assert(false);
    }

    struct mscript_expr *init = mscript_expr_parse(parser);
    if (!mscript_parser_match_char(parser, ';')) {
        assert(false);
    }

    struct mscript_expr *cond = mscript_expr_parse(parser);
    if (!mscript_parser_match_char(parser, ';')) {
        assert(false);
    }

    struct mscript_expr *inc = mscript_expr_parse(parser);
    if (!mscript_parser_match_char(parser, ')')) {
        assert(false);
    }

    struct mscript_stmt *body = mscript_stmt_parse(parser);
    return mscript_stmt_for_new(init, cond, inc, body);
}

static struct mscript_stmt *mscript_stmt_return_parse(struct mscript_parser *parser) {
    struct mscript_expr *expr = mscript_expr_parse(parser);
    if (!mscript_parser_match_char(parser, ';')) {
        assert(false);
    }

    return mscript_stmt_return_new(expr);
}

static struct mscript_stmt *mscript_stmt_function_declaration_parse(struct mscript_parser *parser) {
    struct mscript_type return_type = mscript_parse_type(parser);
    struct mscript_token name = mscript_parser_peek(parser);
    mscript_parser_eat(parser);

    if (name.type != MSCRIPT_TOKEN_SYMBOL) {
        assert(false);
    }
    if (!mscript_parser_match_char(parser, '(')) {
        assert(false);
    }

    struct array_mscript_type arg_types;
    struct array_char_ptr arg_names;
    array_init(&arg_types);
    array_init(&arg_names);

    if (!mscript_parser_match_char(parser, ')')) {
        while (true) {
            struct mscript_type arg_type = mscript_parse_type(parser);
            struct mscript_token arg_name = mscript_parser_peek(parser);
            mscript_parser_eat(parser);

            if (arg_name.type != MSCRIPT_TOKEN_SYMBOL) {
                assert(false);
            }

            array_push(&arg_types, arg_type);
            array_push(&arg_names, arg_name.symbol);

            if (!mscript_parser_match_char(parser, ',')) {
                if (!mscript_parser_match_char(parser, ')')) {
                    assert(false);
                }
                break;
            }
        }
    }

    if (!mscript_parser_match_char(parser, '{')) {
        assert(false);
    }

    struct mscript_stmt *body_stmt = mscript_stmt_block_parse(parser);
    return mscript_stmt_function_declaration_new(return_type, name.symbol, arg_types, arg_names, body_stmt); 
}

static struct mscript_stmt *mscript_stmt_variable_declaration_parse(struct mscript_parser *parser) {
    struct mscript_type type = mscript_parse_type(parser);
    struct mscript_token name = mscript_parser_peek(parser);
    mscript_parser_eat(parser);

    assert(name.type == MSCRIPT_TOKEN_SYMBOL);

    struct mscript_expr *expr = NULL;
    if (mscript_parser_match_char(parser, '=')) {
        expr = mscript_expr_parse(parser);
    }

    if (!mscript_parser_match_char(parser, ';')) {
        assert(false);
    }

    return mscript_stmt_variable_declaration_new(type, name.symbol, expr);
}

static struct mscript_stmt *mscript_stmt_type_declaration_parse(struct mscript_parser *parser) {
    struct mscript_token name_token = mscript_parser_peek(parser);
    if (name_token.type != MSCRIPT_TOKEN_SYMBOL) {
        assert(false);
    }
    const char *name = name_token.symbol;

    mscript_parser_eat(parser);
    if (!mscript_parser_match_char(parser, '{')) {
        assert(false);
    }

    struct array_mscript_type member_types; 
    array_init(&member_types);

    struct array_char_ptr member_names;
    array_init(&member_names);

    while (true) {
        if (mscript_parser_match_char(parser, '}')) {
            break;
        }

        struct mscript_type member_type = mscript_parse_type(parser);
        struct mscript_token member_name_token = mscript_parser_peek(parser);
        if (member_name_token.type != MSCRIPT_TOKEN_SYMBOL) {
            assert(false);
        }
        mscript_parser_eat(parser);
        char *member_name = member_name_token.symbol;
        array_push(&member_types, member_type);
        array_push(&member_names, member_name);
        if (!mscript_parser_match_char(parser, ';')) {
            assert(false);
        }
    }

    return mscript_stmt_type_declaration_new(name, member_types, member_names);
}

static struct mscript_stmt *mscript_stmt_declaration_parse(struct mscript_parser *parser) {
    if (mscript_parser_match_symbol(parser, "struct")) {
        return mscript_stmt_type_declaration_parse(parser); 
    }
    else if (mscript_parser_check_type_declaration(parser)) {
        return mscript_stmt_function_declaration_parse(parser);
    }
    else {
        assert(false);
        return NULL;
    }
}

static struct mscript_stmt *mscript_stmt_parse(struct mscript_parser *parser) {
    if (mscript_parser_match_symbol(parser, "if")) {
        return mscript_stmt_if_parse(parser); 
    }
    else if (mscript_parser_match_symbol(parser, "for")) {
        return mscript_stmt_for_parse(parser);
    }
    else if (mscript_parser_match_symbol(parser, "return")) {
        return mscript_stmt_return_parse(parser);
    }
    else if (mscript_parser_check_type_declaration(parser)) {
        return mscript_stmt_variable_declaration_parse(parser);
    }
    else if (mscript_parser_match_char(parser, '{')) {
        return mscript_stmt_block_parse(parser);
    }
    else {
        struct mscript_expr *expr = mscript_expr_parse(parser);
        if (!mscript_parser_match_char(parser, ';')) {
            assert(false);
        }
        return mscript_stmt_expr_new(expr);
    }
}

//
// VM
//

enum mscript_opcode_type {
    MSCRIPT_OPCODE_ADD,
    MSCRIPT_OPCODE_SUB,
    MSCRIPT_OPCODE_MUL,
    MSCRIPT_OPCODE_DIV,
    MSCRIPT_OPCODE_LTE,
    MSCRIPT_OPCODE_LT,
    MSCRIPT_OPCODE_GTE,
    MSCRIPT_OPCODE_GT,
    MSCRIPT_OPCODE_EQ,
    MSCRIPT_OPCODE_NEQ,
    MSCRIPT_OPCODE_INC,
    MSCRIPT_OPCODE_INT_CONSTANT,
    MSCRIPT_OPCODE_FLOAT_CONSTANT,
    MSCRIPT_OPCODE_SET_LOCAL,
    MSCRIPT_OPCODE_GET_LOCAL,
    MSCRIPT_OPCODE_JF_LABEL,
    MSCRIPT_OPCODE_JMP_LABEL,
    MSCRIPT_OPCODE_CALL_LABEL,
    MSCRIPT_OPCODE_LABEL,
    MSCRIPT_OPCODE_RETURN,
    MSCRIPT_OPCODE_POP,
    MSCRIPT_OPCODE_PUSH,
};

struct mscript_opcode {
    enum mscript_opcode_type type;
    union {
        int int_constant;
        float float_constant;
        int label;

        struct {
            int idx;
        } inc;

        struct {
            int size;
        } push_pop;

        struct {
            int args_size, label;
        } call;

        struct {
            int size;
        } ret;

        struct {
            int idx, size;
        } get_set;
    };
};
array_t(struct mscript_opcode, array_mscript_opcode);

struct mscript_vm {
    int ip;
    struct array_int ip_stack;
    struct array_mscript_opcode opcodes; 

    int fp;
    struct array_int fp_stack;
    struct array_char stack; 
};

static void mscript_vm_init(struct mscript_vm *vm, struct array_mscript_opcode opcodes);
static void mscript_vm_run(struct mscript_vm *vm);
static void mscript_vm_push(struct mscript_vm *vm, char *v, int n);
static char *mscript_vm_pop(struct mscript_vm *vm, int n);
static int mscript_vm_get_label_idx(struct mscript_vm *vm, int label);

static void mscript_vm_init(struct mscript_vm *vm, struct array_mscript_opcode opcodes) {
    vm->ip = 0;
    vm->opcodes = opcodes;
    array_init(&vm->ip_stack);

    vm->fp = 0;
    array_init(&vm->fp_stack);
    array_init(&vm->stack);

    array_push(&vm->ip_stack, -1);
    array_push(&vm->fp_stack, 0);
}

static void mscript_vm_run(struct mscript_vm *vm) {
    vm->ip = mscript_vm_get_label_idx(vm, 7);

    while (true) {
        if (vm->ip == -1) {
            break;
        }

        struct mscript_opcode op = vm->opcodes.data[vm->ip];
        switch (op.type) {
            case MSCRIPT_OPCODE_ADD:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 + *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_SUB:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 - *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_MUL:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 * *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_DIV:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 / *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_LTE:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 <= *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_LT:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 < *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_GTE:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 >= *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_GT:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 > *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_EQ:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 == *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_NEQ:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 != *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_INC:
                {
                    assert(vm->fp + op.inc.idx < vm->stack.length);
                    int *i = (int*) (vm->stack.data + vm->fp + op.inc.idx);
                    (*i)++;
                    mscript_vm_push(vm, (char*) (&i), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_INT_CONSTANT:
                {
                    mscript_vm_push(vm, (char*) (&op.int_constant), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FLOAT_CONSTANT:
                {
                    mscript_vm_push(vm, (char*) (&op.float_constant), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_SET_LOCAL:
                {
                    int idx = op.get_set.idx;
                    int size = op.get_set.size;
                    assert(vm->fp + idx < vm->stack.length);
                    char *src = vm->stack.data + vm->stack.length - size;
                    char *dest = vm->stack.data + vm->fp + idx;
                    memcpy(dest, src, size); 
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_GET_LOCAL:
                {
                    assert(vm->fp + op.get_set.idx < vm->stack.length);
                    char *data = &(vm->stack.data[vm->fp + op.get_set.idx]);
                    mscript_vm_push(vm, data, op.get_set.size);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_JF_LABEL:
                {
                    int *v = (int*) mscript_vm_pop(vm, 4);
                    if (!(*v)) {
                        vm->ip = mscript_vm_get_label_idx(vm, op.label);
                    }
                    else {
                        (vm->ip)++;
                    }
                }
                break;
            case MSCRIPT_OPCODE_JMP_LABEL:
                {
                    vm->ip = mscript_vm_get_label_idx(vm, op.label);
                }
                break;
            case MSCRIPT_OPCODE_CALL_LABEL:
                {
                    array_push(&vm->fp_stack, vm->fp);
                    array_push(&vm->ip_stack, vm->ip + 1);
                    vm->fp = vm->stack.length - op.call.args_size;
                    vm->ip = mscript_vm_get_label_idx(vm, op.call.label);
                }
                break;
            case MSCRIPT_OPCODE_LABEL:
                {
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_RETURN:
                {
                    assert(vm->fp_stack.length > 0);
                    assert(vm->ip_stack.length > 0);
                    int size = op.ret.size;
                    char *ret = vm->stack.data + vm->stack.length - size;
                    vm->stack.length = vm->fp;
                    vm->fp = array_pop(&vm->fp_stack);
                    vm->ip = array_pop(&vm->ip_stack);
                    array_pusharr(&vm->stack, ret, size);
                }
                break;
            case MSCRIPT_OPCODE_POP:
                {
                    int size = op.push_pop.size;
                    vm->stack.length -= size;
                    assert(vm->stack.length >= 0);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_PUSH:
                {
                    int size = op.push_pop.size;
                    for (int i = 0; i < size; i++) {
                        array_push(&vm->stack, 0);
                    }
                    (vm->ip)++;
                }
                break;
        }
    }
}

static void mscript_vm_push(struct mscript_vm *vm, char *v, int n) {
    array_pusharr(&vm->stack, v, n);
}

static char *mscript_vm_pop(struct mscript_vm *vm, int n) {
    if (vm->stack.length < n) {
        assert(false);
    }

    vm->stack.length -= n;
    return vm->stack.data + vm->stack.length;
}

static int mscript_vm_get_label_idx(struct mscript_vm *vm, int label) {
    for (int i = 0; i < vm->opcodes.length; i++) {
        if ((vm->opcodes.data[i].type == MSCRIPT_OPCODE_LABEL) && (vm->opcodes.data[i].label == label)) {
            return i;
        }
    }
    assert(false);
    return -1;
}

//
// COMPILER
//

struct mscript_symbol_declaration {
    const char *name;
    struct mscript_type type;
    int stack_offset;
};
typedef map_t(struct mscript_symbol_declaration) map_mscript_symbol_declaration_t;

struct mscript_type_declaration {
    const char *name; 
    struct array_mscript_type member_types;
    struct array_char_ptr member_names;
};
typedef map_t(struct mscript_type_declaration) map_mscript_type_declaration_t;

struct mscript_env_block {
    int stack_size;
    map_mscript_symbol_declaration_t symbols_map;
};
array_t(struct mscript_env_block, array_mscript_env_block);

struct mscript_function_declaration {
    struct mscript_type return_type;
    const char *name;
    struct array_mscript_type arg_types;
    struct array_char_ptr arg_names;
    int label;
};
typedef map_t(struct mscript_function_declaration) map_mscript_function_declaration_t;

struct mscript_env {
    struct array_mscript_env_block blocks;
    map_mscript_function_declaration_t functions_map;
    map_mscript_type_declaration_t types_map; 
};

static void mscript_env_init(struct mscript_env *env) {
    array_init(&env->blocks);
    map_init(&env->functions_map);
    map_init(&env->types_map);
}

static void mscript_env_push_block(struct mscript_env *env) {
    struct mscript_env_block block;
    block.stack_size = 0;
    map_init(&block.symbols_map);
    array_push(&env->blocks, block);
}

static void mscript_env_pop_block(struct mscript_env *env) {
    struct mscript_env_block block = array_pop(&env->blocks); 
    map_deinit(&block.symbols_map);
}

static void mscript_env_add_symbol_declaration(struct mscript_env *env, const char *name, struct mscript_type type, int type_size) {
    struct mscript_env_block *block = &(env->blocks.data[env->blocks.length - 1]);

    struct mscript_symbol_declaration decl;
    decl.name = name;
    decl.type = type;
    decl.stack_offset = block->stack_size;
    map_set(&(block->symbols_map), name, decl);

    block->stack_size += type_size;
}

static struct mscript_symbol_declaration *mscript_env_get_symbol_declaration(struct mscript_env *env, const char *name) {
    for (int i = env->blocks.length - 1; i >= 0; i--) {
        struct mscript_env_block *block = &(env->blocks.data[i]);  
        struct mscript_symbol_declaration *decl = map_get(&(block->symbols_map), name);
        if (decl) {
            return decl;
        }
    }
    return NULL;
}

static void mscript_env_add_type_declaration(struct mscript_env *env, const char *name,
        struct array_mscript_type member_types, struct array_char_ptr member_names) {
    struct mscript_type_declaration decl;
    decl.name = name;
    decl.member_types = member_types;
    decl.member_names = member_names;
    map_set(&env->types_map, name, decl);
}

static struct mscript_type_declaration *mscript_env_get_type_declaration(struct mscript_env *env, const char *name) {
    return map_get(&env->types_map, name);
}

static struct mscript_function_declaration *mscript_env_get_function_declaration(struct mscript_env *env, const char *name) {
    return map_get(&env->functions_map, name);
}

static void mscript_env_add_function_declaration(struct mscript_env *env, struct mscript_type return_type, const char *name,
        struct array_mscript_type arg_types, struct array_char_ptr arg_names, int label) {
    struct mscript_function_declaration decl;
    decl.return_type = return_type;
    decl.name = name;
    decl.arg_types = arg_types;
    decl.arg_names = arg_names;
    decl.label = label;
    map_set(&env->functions_map, name, decl);
}

struct mscript_compiler {
    struct mscript_function_declaration *cur_function;
    struct mscript_parser *parser;
    struct mscript_env env;
    int cur_label;
    struct array_mscript_opcode opcodes;
};

static void mscript_compiler_init(struct mscript_compiler *compiler, struct mscript_parser *parser);
static void mscript_compiler_run(struct mscript_compiler *compiler);
static int mscript_compiler_create_label(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_add(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_sub(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_mul(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_div(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_lte(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_lt(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_gte(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_gt(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_eq(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_neq(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_inc(struct mscript_compiler *compiler, int idx);
static void mscript_compiler_push_opcode_inc_sp(struct mscript_compiler *compiler, int idx);
static void mscript_compiler_push_opcode_int_constant(struct mscript_compiler *compiler, int int_constant);
static void mscript_compiler_push_opcode_float_constant(struct mscript_compiler *compiler, float float_constant);
static void mscript_compiler_push_opcode_set_local(struct mscript_compiler *compiler, int idx, int size);
static void mscript_compiler_push_opcode_get_local(struct mscript_compiler *compiler, int idx, int size);
static void mscript_compiler_push_opcode_jf_label(struct mscript_compiler *compiler, int label);
static void mscript_compiler_push_opcode_jmp_label(struct mscript_compiler *compiler, int label);
static void mscript_compiler_push_opcode_label(struct mscript_compiler *compiler, int label);
static void mscript_compiler_push_opcode_call_label(struct mscript_compiler *compiler, int label, int args_size);
static void mscript_compiler_push_opcode_call(struct mscript_compiler *compiler, int idx);
static void mscript_compiler_push_opcode_return(struct mscript_compiler *compiler, int size);
static void mscript_compiler_push_opcode_push(struct mscript_compiler *compiler, int size);
static void mscript_compiler_push_opcode_pop(struct mscript_compiler *compiler, int size);
static int mscript_compiler_get_type_size(struct mscript_compiler *compiler, struct mscript_type type);
static int mscript_compiler_get_l_value_stack_offset(struct mscript_compiler *compiler, struct mscript_expr *expr);
static struct mscript_type mscript_compiler_expr_get_type(struct mscript_compiler *compiler, struct mscript_expr *expr);
static void mscript_compiler_debug_print_opcodes(struct mscript_compiler *compiler);
static void mscript_compile_stmt(struct mscript_compiler *compiler, struct mscript_stmt *stmt);
static void mscript_compile_expr(struct mscript_compiler *compiler, struct mscript_expr *expr);

static void mscript_compiler_init(struct mscript_compiler *compiler, struct mscript_parser *parser) {
    compiler->cur_function = NULL;
    compiler->parser = parser;
    mscript_env_init(&compiler->env);
    mscript_env_push_block(&compiler->env);
    compiler->cur_label = 0;
    array_init(&compiler->opcodes);
}

static void mscript_compiler_run(struct mscript_compiler *compiler) {
    for (int i = 0; i < compiler->parser->stmts_array.length; i++) {
        struct mscript_stmt *stmt = compiler->parser->stmts_array.data[i]; 

        if (stmt->type == MSCRIPT_STMT_FUNCTION_DECLARATION) {
            struct mscript_type return_type = stmt->function_declaration.return_type;
            const char *name = stmt->function_declaration.name;
            struct array_mscript_type arg_types = stmt->function_declaration.arg_types;
            struct array_char_ptr arg_names = stmt->function_declaration.arg_names;
            int label = mscript_compiler_create_label(compiler);
            mscript_compiler_push_opcode_label(compiler, label);
            mscript_env_add_function_declaration(&compiler->env, return_type, name, arg_types, arg_names, label);
            struct mscript_function_declaration *function_declaration = mscript_env_get_function_declaration(&compiler->env, name);

            mscript_env_push_block(&compiler->env);
            for (int i = 0; i < stmt->function_declaration.arg_names.length; i++) {
                const char *name = stmt->function_declaration.arg_names.data[i];
                struct mscript_type type = stmt->function_declaration.arg_types.data[i];
                int size = mscript_compiler_get_type_size(compiler, type);
                mscript_env_add_symbol_declaration(&compiler->env, name, type, size);
            }

            if (compiler->cur_function) {
                assert(false);
            }
            compiler->cur_function = function_declaration;
            mscript_compile_stmt(compiler, stmt->function_declaration.body);
            compiler->cur_function = NULL;

            mscript_env_pop_block(&compiler->env);
        }
        else if (stmt->type == MSCRIPT_STMT_VARIABLE_DECLARATION) {

        }
        else if (stmt->type == MSCRIPT_STMT_TYPE_DECLARATION) {
            mscript_env_add_type_declaration(&compiler->env, stmt->type_declaration.name,
                    stmt->type_declaration.member_types, stmt->type_declaration.member_names);
        }
        else {
            assert(false);
        }
    }

    mscript_compiler_debug_print_opcodes(compiler);
}

static int mscript_compiler_create_label(struct mscript_compiler *compiler) {
    return compiler->cur_label++;
}

static void mscript_compiler_push_opcode_add(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_ADD;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_sub(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_SUB;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_mul(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_MUL;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_div(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_DIV;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_lte(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_LTE;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_lt(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_LT;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_gte(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_GTE;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_gt(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_GT;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_eq(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_EQ;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_neq(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_NEQ;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_inc(struct mscript_compiler *compiler, int idx) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_INC;
    op.inc.idx = idx;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_int_constant(struct mscript_compiler *compiler, int int_constant) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_INT_CONSTANT;
    op.int_constant = int_constant;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_float_constant(struct mscript_compiler *compiler, float float_constant) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FLOAT_CONSTANT;
    op.float_constant = float_constant;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_set_local(struct mscript_compiler *compiler, int idx, int size) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_SET_LOCAL;
    op.get_set.idx = idx;
    op.get_set.size = size;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_get_local(struct mscript_compiler *compiler, int idx, int size) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_GET_LOCAL;
    op.get_set.idx = idx;
    op.get_set.size = size;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_jf_label(struct mscript_compiler *compiler, int label) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_JF_LABEL;
    op.label = label;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_jmp_label(struct mscript_compiler *compiler, int label) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_JMP_LABEL;
    op.label = label;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_label(struct mscript_compiler *compiler, int label) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_LABEL;
    op.label = label;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_call_label(struct mscript_compiler *compiler, int label, int args_size) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_CALL_LABEL;
    op.call.label = label;
    op.call.args_size = args_size;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_return(struct mscript_compiler *compiler, int size) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_RETURN;
    op.ret.size = size;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_push(struct mscript_compiler *compiler, int size)  {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_PUSH;
    op.push_pop.size = size;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_pop(struct mscript_compiler *compiler, int size)  {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_POP;
    op.push_pop.size = size;
    array_push(&compiler->opcodes, op);
}

static int mscript_compiler_get_l_value_stack_offset(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    switch (expr->type) {
        case MSCRIPT_EXPR_UNARY_OP:
        case MSCRIPT_EXPR_BINARY_OP:
        case MSCRIPT_EXPR_CALL:
        case MSCRIPT_EXPR_ASSIGNMENT:
        case MSCRIPT_EXPR_INT:
        case MSCRIPT_EXPR_FLOAT:
            assert(false);
            break;
        case MSCRIPT_EXPR_MEMBER_ACCESS:
            break;
        case MSCRIPT_EXPR_ARRAY_ACCESS:
            break;
        case MSCRIPT_EXPR_SYMBOL:
            {
                struct mscript_symbol_declaration *decl = mscript_env_get_symbol_declaration(&compiler->env, expr->symbol);
                if (!decl) {
                    assert(false);
                }
                return decl->stack_offset;
            }
            break;
    }

    assert(false);
    return 0;
}


static int mscript_compiler_get_type_size(struct mscript_compiler *compiler, struct mscript_type type) {
    switch (type.type) {
        case MSCRIPT_TYPE_VOID:
            return 0;
            break;
        case MSCRIPT_TYPE_INT:
            return 4;
            break;
        case MSCRIPT_TYPE_FLOAT:
            return 4;
            break;
        case MSCRIPT_TYPE_STRUCT:
            {
                int size = 0;
                struct mscript_type_declaration *type_decl = mscript_env_get_type_declaration(&(compiler->env), type.struct_name);
                assert(type_decl);
                for (int i = 0; i < type_decl->member_types.length; i++) {
                    size += mscript_compiler_get_type_size(compiler, type_decl->member_types.data[i]);
                }
                return size;
            }
            break;
        case MSCRIPT_TYPE_ARRAY:
            return 4;
            break;
    }

    assert(false);
    return 0;
}

static struct mscript_type mscript_compiler_expr_get_type(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    switch (expr->type) {
        case MSCRIPT_EXPR_UNARY_OP:
            {
                return mscript_compiler_expr_get_type(compiler, expr->unary_op.operand);
            }
            break;
        case MSCRIPT_EXPR_BINARY_OP:
            {
                return mscript_compiler_expr_get_type(compiler, expr->binary_op.left);
            }
            break;
        case MSCRIPT_EXPR_CALL:
            {
                struct mscript_function_declaration *decl = mscript_env_get_function_declaration(&compiler->env, expr->call.function_name);
                if (!decl) {
                    assert(false);
                }
                return decl->return_type;
            }
            break;
        case MSCRIPT_EXPR_ARRAY_ACCESS:
            assert(false);
            break;
        case MSCRIPT_EXPR_MEMBER_ACCESS:
            assert(false);
            break;
        case MSCRIPT_EXPR_ASSIGNMENT:
            {
                return mscript_compiler_expr_get_type(compiler, expr->assignment.left);
            }
            break;
        case MSCRIPT_EXPR_INT:
            {
                struct mscript_type type;
                type.type = MSCRIPT_TYPE_INT;
                return type;
            }
            break;
        case MSCRIPT_EXPR_FLOAT:
            {
                struct mscript_type type;
                type.type = MSCRIPT_TYPE_FLOAT;
                return type;
            }
            break;
        case MSCRIPT_EXPR_SYMBOL:
            {
                struct mscript_symbol_declaration *decl = mscript_env_get_symbol_declaration(&compiler->env, expr->symbol);
                if (!decl) {
                    assert(false);
                }
                return decl->type;
            }
            break;
    }

    assert(false);
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_INT;
    return type;
}

static void mscript_compiler_debug_print_opcodes(struct mscript_compiler *compiler) {
    for (int i = 0; i < compiler->opcodes.length; i++) {
        m_logf("%d: ", i);
        struct mscript_opcode op = compiler->opcodes.data[i];
        switch (op.type) {
            case MSCRIPT_OPCODE_ADD:
                m_logf("[ADD]");
                break;
            case MSCRIPT_OPCODE_SUB:
                m_logf("[SUB]");
                break;
            case MSCRIPT_OPCODE_MUL:
                m_logf("[MUL]");
                break;
            case MSCRIPT_OPCODE_DIV:
                m_logf("[DIV]");
                break;
            case MSCRIPT_OPCODE_LTE:
                m_logf("[LTE]");
                break;
            case MSCRIPT_OPCODE_LT:
                m_logf("[LT]");
                break;
            case MSCRIPT_OPCODE_GTE:
                m_logf("[GTE]");
                break;
            case MSCRIPT_OPCODE_GT:
                m_logf("[GT]");
                break;
            case MSCRIPT_OPCODE_EQ:
                m_logf("[EQ]");
                break;
            case MSCRIPT_OPCODE_NEQ:
                m_logf("[NEQ]");
                break;
            case MSCRIPT_OPCODE_INC:
                m_logf("[INC: %d]", op.inc.idx);
                break;
            case MSCRIPT_OPCODE_INT_CONSTANT:
                m_logf("[INT_CONSTANT: %d]", op.int_constant);
                break;
            case MSCRIPT_OPCODE_FLOAT_CONSTANT:
                m_logf("[FLOAT_CONSTANT: %d]", op.float_constant);
                break;
            case MSCRIPT_OPCODE_SET_LOCAL:
                m_logf("[SET_LOCAL: %d, %d]", op.get_set.idx, op.get_set.size);
                break;
            case MSCRIPT_OPCODE_GET_LOCAL:
                m_logf("[GET_LOCAL: %d, %d]", op.get_set.idx, op.get_set.size);
                break;
            case MSCRIPT_OPCODE_JF_LABEL:
                m_logf("[JF_LABEL: %d]", op.label);
                break;
            case MSCRIPT_OPCODE_JMP_LABEL:
                m_logf("[JMP_LABEL: %d]", op.label);
                break;
            case MSCRIPT_OPCODE_LABEL:
                m_logf("[LABEL: %d]", op.label);
                break;
            case MSCRIPT_OPCODE_CALL_LABEL:
                m_logf("[CALL_LABEL: %d, %d]", op.call.label, op.call.args_size);
                break;
            case MSCRIPT_OPCODE_RETURN:
                m_logf("[RETURN: %d]", op.ret.size);
                break;
            case MSCRIPT_OPCODE_POP:
                m_logf("[POP: %d]", op.push_pop.size);
                break;
            case MSCRIPT_OPCODE_PUSH:
                m_logf("[PUSH: %d]", op.push_pop.size);
                break;
        }
        m_logf("\n");
    }
}

static void mscript_compile_stmt(struct mscript_compiler *compiler, struct mscript_stmt *stmt) {
    switch (stmt->type) {
        case MSCRIPT_STMT_IF:
            {
                struct array_mscript_expr_ptr conds = stmt->if_stmt.conds;
                struct array_mscript_stmt_ptr stmts = stmt->if_stmt.stmts;
                struct mscript_stmt *else_stmt = stmt->if_stmt.else_stmt;

                int else_if_label = -1;
                int final_label = mscript_compiler_create_label(compiler);
                for (int i = 0; i < conds.length; i++) {
                    mscript_compile_expr(compiler, conds.data[i]);
                    else_if_label = mscript_compiler_create_label(compiler);
                    mscript_compiler_push_opcode_jf_label(compiler, else_if_label);
                    mscript_compile_stmt(compiler, stmts.data[i]);
                    mscript_compiler_push_opcode_jmp_label(compiler, final_label);
                    mscript_compiler_push_opcode_label(compiler, else_if_label);
                }
                if (else_stmt) {
                    mscript_compile_stmt(compiler, else_stmt);
                }
                mscript_compiler_push_opcode_label(compiler, final_label);
            }
            break;
        case MSCRIPT_STMT_RETURN:
            {
                mscript_compile_expr(compiler, stmt->return_stmt.expr);
                assert(compiler->cur_function);
                int size = mscript_compiler_get_type_size(compiler, compiler->cur_function->return_type);
                mscript_compiler_push_opcode_return(compiler, size);
            }
            break;
        case MSCRIPT_STMT_BLOCK:
            {
                for (int i = 0; i < stmt->block.stmts.length; i++) {
                    mscript_compile_stmt(compiler, stmt->block.stmts.data[i]);
                }
            }
            break;
        case MSCRIPT_STMT_EXPR:
            {
                mscript_compile_expr(compiler, stmt->expr);
                struct mscript_type type = mscript_compiler_expr_get_type(compiler, stmt->expr);
                int size = mscript_compiler_get_type_size(compiler, type);
                mscript_compiler_push_opcode_pop(compiler, size);
            }
            break;
        case MSCRIPT_STMT_FOR:
            {
                struct mscript_expr *init = stmt->for_stmt.init;
                struct mscript_expr *cond = stmt->for_stmt.cond;
                struct mscript_expr *inc = stmt->for_stmt.inc;
                struct mscript_stmt *body = stmt->for_stmt.body;

                int init_size = mscript_compiler_get_type_size(compiler, mscript_compiler_expr_get_type(compiler, init));
                int cond_size = mscript_compiler_get_type_size(compiler, mscript_compiler_expr_get_type(compiler, cond));
                int inc_size = mscript_compiler_get_type_size(compiler, mscript_compiler_expr_get_type(compiler, inc));

                int cond_label = mscript_compiler_create_label(compiler);
                int end_label = mscript_compiler_create_label(compiler);

                mscript_compile_expr(compiler, init);
                mscript_compiler_push_opcode_pop(compiler, init_size);
                mscript_compiler_push_opcode_label(compiler, cond_label);
                mscript_compile_expr(compiler, cond);
                mscript_compiler_push_opcode_jf_label(compiler, end_label);
                mscript_compile_stmt(compiler, body);
                mscript_compile_expr(compiler, inc);
                mscript_compiler_push_opcode_pop(compiler, inc_size);
                mscript_compiler_push_opcode_jmp_label(compiler, cond_label);
                mscript_compiler_push_opcode_label(compiler, end_label);
            }
            break;
        case MSCRIPT_STMT_VARIABLE_DECLARATION:
            {
                struct mscript_type type = stmt->variable_declaration.type;
                const char *name = stmt->variable_declaration.name;
                struct mscript_expr *expr = stmt->variable_declaration.expr;
                int size = mscript_compiler_get_type_size(compiler, type);

                mscript_env_add_symbol_declaration(&compiler->env, name, type, size);
                mscript_compiler_push_opcode_push(compiler, size);

                if (expr) {
                    struct mscript_symbol_declaration *decl = mscript_env_get_symbol_declaration(&compiler->env, name);
                    if (!decl) {
                        assert(false);
                    }

                    mscript_compile_expr(compiler, expr);
                    mscript_compiler_push_opcode_set_local(compiler, decl->stack_offset, size);
                    mscript_compiler_push_opcode_pop(compiler, size);
                }
            }
            break;
        case MSCRIPT_STMT_FUNCTION_DECLARATION:
            assert(false);
            break;
    }
}

static void mscript_compile_expr_unary_op(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    switch (expr->unary_op.type) {
        case MSCRIPT_UNARY_OP_POST_INC:
            int l_value_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->unary_op.operand);
            mscript_compiler_push_opcode_inc(compiler, l_value_offset);
            break;
    }
}

static void mscript_compile_expr_binary_op(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    mscript_compile_expr(compiler, expr->binary_op.left);
    mscript_compile_expr(compiler, expr->binary_op.right);

    switch (expr->binary_op.type) {
        case MSCRIPT_BINARY_OP_ADD:
            mscript_compiler_push_opcode_add(compiler);
            break;
        case MSCRIPT_BINARY_OP_SUB:
            mscript_compiler_push_opcode_sub(compiler);
            break;
        case MSCRIPT_BINARY_OP_MUL:
            mscript_compiler_push_opcode_mul(compiler);
            break;
        case MSCRIPT_BINARY_OP_DIV:
            mscript_compiler_push_opcode_div(compiler);
            break;
        case MSCRIPT_BINARY_OP_LTE:
            mscript_compiler_push_opcode_lte(compiler);
            break;
        case MSCRIPT_BINARY_OP_LT:
            mscript_compiler_push_opcode_lt(compiler);
            break;
        case MSCRIPT_BINARY_OP_GTE:
            mscript_compiler_push_opcode_gte(compiler);
            break;
        case MSCRIPT_BINARY_OP_GT:
            mscript_compiler_push_opcode_gt(compiler);
            break;
        case MSCRIPT_BINARY_OP_EQ:
            mscript_compiler_push_opcode_eq(compiler);
            break;
        case MSCRIPT_BINARY_OP_NEQ:
            mscript_compiler_push_opcode_neq(compiler);
            break;
    }
}

static void mscript_compile_expr(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    switch (expr->type) {
        case MSCRIPT_EXPR_UNARY_OP:
            {
                mscript_compile_expr_unary_op(compiler, expr);
            }
            break;
        case MSCRIPT_EXPR_BINARY_OP:
            {
                mscript_compile_expr_binary_op(compiler, expr);
            }
            break;
        case MSCRIPT_EXPR_CALL:
            {
                const char *function_name = expr->call.function_name;
                struct mscript_function_declaration *decl = mscript_env_get_function_declaration(&compiler->env, function_name);
                if (!decl) {
                    assert(false);
                }
                if (decl->arg_types.length != expr->call.args.length) {
                    assert(false);
                }

                int args_size = 0;
                for (int i = 0; i < expr->call.args.length; i++) {
                    args_size += mscript_compiler_get_type_size(compiler, decl->arg_types.data[i]);
                    mscript_compile_expr(compiler, expr->call.args.data[i]);
                }

                mscript_compiler_push_opcode_call_label(compiler, decl->label, args_size);
            }
            break;
        case MSCRIPT_EXPR_ASSIGNMENT:
            {
                struct mscript_type l_type = mscript_compiler_expr_get_type(compiler, expr->assignment.left);
                int l_size = mscript_compiler_get_type_size(compiler, l_type);
                int l_value_stack_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->assignment.left);
                mscript_compile_expr(compiler, expr->assignment.right);
                mscript_compiler_push_opcode_set_local(compiler, l_value_stack_offset, l_size);
            }
            break;
        case MSCRIPT_EXPR_MEMBER_ACCESS:
            {
                int l_value_stack_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->member_access.left);
                struct mscript_type struct_type = mscript_compiler_expr_get_type(compiler, expr->member_access.left);
                if (struct_type.type != MSCRIPT_TYPE_STRUCT) {
                    assert(false);
                }
                const char *member_name = expr->member_access.member_name;

                struct mscript_type_declaration *type_decl = mscript_env_get_type_declaration(&compiler->env, struct_type.struct_name);
                if (!type_decl) {
                    assert(false);
                }

                int member_size = 0;
                int member_offset = 0;
                bool found = false;
                for (int i = 0; i < type_decl->member_names.length; i++) {
                    int type_size = mscript_compiler_get_type_size(compiler, type_decl->member_types.data[i]);
                    if (strcmp(type_decl->member_names.data[i], member_name) == 0) {
                        member_size = type_size; 
                        found = true;
                        break;
                    }
                    member_offset += type_size; 
                }

                if (!found) {
                    assert(false);
                }

                mscript_compiler_push_opcode_get_local(compiler, member_size, 4);
            }
            break;
        case MSCRIPT_EXPR_ARRAY_ACCESS:
            {
            }
            break;
        case MSCRIPT_EXPR_INT:
            {
                mscript_compiler_push_opcode_int_constant(compiler, expr->int_value);
            }
            break;
        case MSCRIPT_EXPR_FLOAT:
            {
                mscript_compiler_push_opcode_float_constant(compiler, expr->float_value);
            }
            break;
        case MSCRIPT_EXPR_SYMBOL:
            {
                struct mscript_symbol_declaration *decl = mscript_env_get_symbol_declaration(&compiler->env, expr->symbol);
                if (!decl) {
                    assert(false);
                }

                int type_size = mscript_compiler_get_type_size(compiler, decl->type);
                mscript_compiler_push_opcode_get_local(compiler, decl->stack_offset, type_size);
            }
            break;
    }
}

void mscript_compile(const char *prog_text) {
    struct mscript_parser parser;
    mscript_parser_init(&parser, prog_text);
    mscript_parser_run(&parser);

    struct mscript_compiler compiler;
    mscript_compiler_init(&compiler, &parser);
    mscript_compiler_run(&compiler);

    struct mscript_vm vm;
    mscript_vm_init(&vm, compiler.opcodes);
    mscript_vm_run(&vm);
}
