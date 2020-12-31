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
        else if (is_char(prog[i])) {
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
typedef map_t(struct mscript_stmt *) map_mscript_stmt_ptr_t;
array_t(struct mscript_expr *, array_mscript_expr_ptr)

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
array_t(struct mscript_type, array_mscript_type);
typedef map_t(struct mscript_type) map_mscript_type_t;

static struct mscript_type mscript_type_void(void);
static struct mscript_type mscript_type_int(void);
static struct mscript_type mscript_type_float(void);
static struct mscript_type mscript_type_binary_op_result(enum mscript_binary_op_type binary_op_type,
        struct mscript_expr *left, struct mscript_expr *right);

struct mscript_parser_env {
    map_mscript_type_t symbols;
};
array_t(struct mscript_parser_env, array_mscript_parser_env);

struct mscript_parser {
    const char *prog_text;
    int token_idx;
    struct array_mscript_token tokens_array; 
    struct array_mscript_stmt_ptr stmts_array;
    struct array_mscript_parser_env env_array;
    map_mscript_stmt_ptr_t type_declarations_map;
};

static void mscript_parser_init(struct mscript_parser *parser, const char *prog_text);
static void mscript_parser_deinit(struct mscript_parser *parser);
static void mscript_parser_run(struct mscript_parser *parser);
static void mscript_parser_env_push(struct mscript_parser *parser);
static void mscript_parser_env_pop(struct mscript_parser *parser);
static void mscript_parser_env_set_symbol_type(struct mscript_parser *parser, const char *symbol, struct mscript_type type);
static struct mscript_type *mscript_parser_env_get_symbol_type(struct mscript_parser *parser, const char *symbol);
static void mscript_parser_env_add_type_declaration(struct mscript_parser *parser, const char *name, struct mscript_stmt *stmt);
static struct mscript_stmt *mscript_parser_env_get_type_declaration(struct mscript_parser *parser, const char *name);
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
    struct mscript_type result_type;

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

static struct mscript_type mscript_type_void(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_VOID;
    return type;
}

static struct mscript_type mscript_type_int(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_INT;
    return type;
}

static struct mscript_type mscript_type_float(void) {
    struct mscript_type type;
    type.type = MSCRIPT_TYPE_FLOAT;
    return type;
}

static struct mscript_type mscript_type_binary_op_result(enum mscript_binary_op_type binary_op_type,
        struct mscript_expr *left, struct mscript_expr *right) {
    switch (binary_op_type) {
        case MSCRIPT_BINARY_OP_ADD:
        case MSCRIPT_BINARY_OP_SUB:
        case MSCRIPT_BINARY_OP_MUL:
        case MSCRIPT_BINARY_OP_DIV:
            {
                if (left->result_type.type == MSCRIPT_TYPE_INT && right->result_type.type == MSCRIPT_TYPE_INT) {
                    return mscript_type_int();
                }
                else if ((left->result_type.type == MSCRIPT_TYPE_FLOAT && right->result_type.type == MSCRIPT_TYPE_FLOAT) ||
                        (left->result_type.type == MSCRIPT_TYPE_FLOAT && right->result_type.type == MSCRIPT_TYPE_INT) ||
                        (left->result_type.type == MSCRIPT_TYPE_INT && right->result_type.type == MSCRIPT_TYPE_FLOAT)) {
                    return mscript_type_float();
                }
                else {
                    assert(false);
                }
            }
            break;

        case MSCRIPT_BINARY_OP_LTE:
        case MSCRIPT_BINARY_OP_LT:
        case MSCRIPT_BINARY_OP_GTE:
        case MSCRIPT_BINARY_OP_GT:
        case MSCRIPT_BINARY_OP_EQ:
        case MSCRIPT_BINARY_OP_NEQ:
            {
                return mscript_type_int();
            }
            break;
    }

    assert(false);
    return mscript_type_void();
}

static void mscript_parser_init(struct mscript_parser *parser, const char *prog_text) {
    parser->prog_text = prog_text;
    parser->token_idx = 0;
    array_init(&parser->tokens_array);
    array_init(&parser->stmts_array);
    array_init(&parser->env_array);
    mscript_parser_env_push(parser);
    map_init(&parser->type_declarations_map);
}

static void mscript_parser_deinit(struct mscript_parser *parser) {
    array_deinit(&parser->tokens_array);
    array_deinit(&parser->stmts_array);
    array_deinit(&parser->env_array);
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

static void mscript_parser_env_push(struct mscript_parser *parser) {
    struct mscript_parser_env env;
    map_init(&env.symbols);
    array_push(&parser->env_array, env);
}

static void mscript_parser_env_pop(struct mscript_parser *parser) {
    struct mscript_parser_env env = array_pop(&parser->env_array);
    map_deinit(&env.symbols);
}

static void mscript_parser_env_set_symbol_type(struct mscript_parser *parser, const char *symbol, struct mscript_type type) {
    assert(parser->env_array.length > 0);
    map_set(&(parser->env_array.data[parser->env_array.length - 1].symbols), symbol, type);
}

static struct mscript_type *mscript_parser_env_get_symbol_type(struct mscript_parser *parser, const char *symbol) {
    for (int i = parser->env_array.length - 1; i >= 0; i--) {
        struct mscript_type *type = map_get(&(parser->env_array.data[i].symbols), symbol);
        if (type) {
            return type;
        }
    }
    return NULL;
}

static void mscript_parser_env_add_type_declaration(struct mscript_parser *parser, const char *name, struct mscript_stmt *stmt) {
    map_set(&parser->type_declarations_map, name, stmt);
}

static struct mscript_stmt *mscript_parser_env_get_type_declaration(struct mscript_parser *parser, const char *name) {
    struct mscript_stmt **stmt = map_get(&parser->type_declarations_map, name);
    if (!stmt) {
        assert(false);
    }
    return *stmt;
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
    expr->result_type = operand->result_type;
    expr->unary_op.type = type;
    expr->unary_op.operand = operand;
    return expr;
}

static struct mscript_expr *mscript_expr_binary_op_new(enum mscript_binary_op_type type, struct mscript_expr *left, struct mscript_expr *right,
        struct mscript_type result_type) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_BINARY_OP;
    expr->result_type = result_type;
    expr->binary_op.type = type;
    expr->binary_op.left = left;
    expr->binary_op.right = right;
    return expr;
}

static struct mscript_expr *mscript_expr_float_new(float float_value) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_FLOAT;
    expr->result_type = mscript_type_float();
    expr->float_value = float_value;
    return expr;
}

static struct mscript_expr *mscript_expr_int_new(int int_value) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_INT;
    expr->result_type = mscript_type_int();
    expr->int_value = int_value;
    return expr;
}

static struct mscript_expr *mscript_expr_symbol_new(const char *symbol, struct mscript_type type) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_SYMBOL;
    expr->result_type = type;
    expr->symbol = symbol;
    return expr;
}

static struct mscript_expr *mscript_expr_call_new(const char *function_name, struct array_mscript_expr_ptr args, struct mscript_type type) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_CALL;
    expr->result_type = type;
    expr->call.function_name = function_name;
    expr->call.args = args;
    return expr;
}

static struct mscript_expr *mscript_expr_member_access_new(struct mscript_expr *left, const char *member_name, struct mscript_type type) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_MEMBER_ACCESS;
    expr->result_type = type;
    expr->member_access.left = left;
    expr->member_access.member_name = member_name;
    return expr;
}

static struct mscript_expr *mscript_expr_array_access_new(struct mscript_expr *left, struct mscript_expr *right) {
    assert(false);
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_ARRAY_ACCESS;
    expr->array_access.left = left;
    expr->array_access.right = right;
    return expr;
}

static struct mscript_expr *mscript_expr_assignment_new(struct mscript_expr *left, struct mscript_expr *right) {
    struct mscript_expr *expr = malloc(sizeof(struct mscript_expr));
    expr->type = MSCRIPT_EXPR_ASSIGNMENT;
    expr->result_type = left->result_type;
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
        struct mscript_type *type = mscript_parser_env_get_symbol_type(parser, token.symbol);
        assert(type);
        return mscript_expr_symbol_new(token.symbol, *type);
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
        struct mscript_token token = mscript_parser_peek(parser);
        if (token.type != MSCRIPT_TOKEN_SYMBOL) {
            assert(false);
        }
        mscript_parser_eat(parser);

        if (expr->result_type.type != MSCRIPT_TYPE_STRUCT) {
            assert(false);
        }

        struct mscript_stmt *type_declaration = mscript_parser_env_get_type_declaration(parser, expr->result_type.struct_name);
        assert(type_declaration->type == MSCRIPT_STMT_TYPE_DECLARATION);

        struct array_mscript_type *member_types = &(type_declaration->type_declaration.member_types);
        struct array_char_ptr *member_names = &(type_declaration->type_declaration.member_names);
        assert(member_types->length = member_names->length);

        bool found_member = false;
        struct mscript_type member_type;
        for (int i = 0; i < member_names->length; i++) {
            if (strcmp(member_names->data[i], token.symbol) == 0) {
                found_member = true;
                member_type = member_types->data[i];
                break;
            }
        }
        if (!found_member) {
            assert(false);
        }

        expr = mscript_expr_member_access_new(expr, token.symbol, member_type);
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

        struct mscript_type *type = mscript_parser_env_get_symbol_type(parser, expr->symbol);
        assert(type);
        expr = mscript_expr_call_new(expr->symbol, args, *type);
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
            struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_MUL, expr, right);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_MUL, expr, right, result_type); 
        }
        else if (mscript_parser_match_char(parser, '/')) {
            struct mscript_expr *right = mscript_expr_parse_unary(parser);
            struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_DIV, expr, right);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_DIV, expr, right, result_type); 
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
            struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_ADD, expr, right);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_ADD, expr, right, result_type); 
        }
        else if (mscript_parser_match_char(parser, '-')) {
            struct mscript_expr *right = mscript_expr_parse_factor(parser);
            struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_SUB, expr, right);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_SUB, expr, right, result_type); 
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
                struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_LTE, expr, right);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_LTE, expr, right, result_type);
            }
            else {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_LT, expr, right);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_LT, expr, right, result_type);
            }
        }
        else if (mscript_parser_match_char(parser, '>')) {
            if (mscript_parser_match_char(parser, '=')) {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_GTE, expr, right);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_GTE, expr, right, result_type);
            }
            else {
                struct mscript_expr *right = mscript_expr_parse_term(parser);
                struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_GT, expr, right);
                expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_GT, expr, right, result_type);
            }
        }
        else if (mscript_parser_match_char_n(parser, 2, '=', '=')) {
            struct mscript_expr *right = mscript_expr_parse_term(parser);
            struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_EQ, expr, right);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_EQ, expr, right, result_type);
        }
        else if (mscript_parser_match_char_n(parser, 2, '!', '=')) {
            struct mscript_expr *right = mscript_expr_parse_term(parser);
            struct mscript_type result_type = mscript_type_binary_op_result(MSCRIPT_BINARY_OP_NEQ, expr, right);
            expr = mscript_expr_binary_op_new(MSCRIPT_BINARY_OP_NEQ, expr, right, result_type);
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

    mscript_parser_env_push(parser);
    while (true) {
        if (mscript_parser_match_char(parser, '}')) {
            break;
        }

        array_push(&stmts, mscript_stmt_parse(parser));
    }
    mscript_parser_env_pop(parser);

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

    mscript_parser_env_set_symbol_type(parser, name.symbol, return_type);
    mscript_parser_env_push(parser);
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
            mscript_parser_env_set_symbol_type(parser, arg_name.symbol, arg_type);

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
    mscript_parser_env_pop(parser);
    return mscript_stmt_function_declaration_new(return_type, name.symbol, arg_types, arg_names, body_stmt); 
}

static struct mscript_stmt *mscript_stmt_variable_declaration_parse(struct mscript_parser *parser) {
    struct mscript_type type = mscript_parse_type(parser);
    struct mscript_token name = mscript_parser_peek(parser);
    mscript_parser_eat(parser);

    assert(name.type == MSCRIPT_TOKEN_SYMBOL);
    mscript_parser_env_set_symbol_type(parser, name.symbol, type);

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

    struct mscript_stmt *type_declaration = mscript_stmt_type_declaration_new(name, member_types, member_names);
    mscript_parser_env_add_type_declaration(parser, name, type_declaration);
    return type_declaration;
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
    MSCRIPT_OPCODE_IADD,
    MSCRIPT_OPCODE_FADD,
    MSCRIPT_OPCODE_ISUB,
    MSCRIPT_OPCODE_FSUB,
    MSCRIPT_OPCODE_IMUL,
    MSCRIPT_OPCODE_FMUL,
    MSCRIPT_OPCODE_IDIV,
    MSCRIPT_OPCODE_FDIV,
    MSCRIPT_OPCODE_ILTE,
    MSCRIPT_OPCODE_FLTE,
    MSCRIPT_OPCODE_ILT,
    MSCRIPT_OPCODE_FLT,
    MSCRIPT_OPCODE_IGTE,
    MSCRIPT_OPCODE_FGTE,
    MSCRIPT_OPCODE_IGT,
    MSCRIPT_OPCODE_FGT,
    MSCRIPT_OPCODE_IEQ,
    MSCRIPT_OPCODE_FEQ,
    MSCRIPT_OPCODE_INEQ,
    MSCRIPT_OPCODE_FNEQ,
    MSCRIPT_OPCODE_IINC,
    MSCRIPT_OPCODE_F2I,
    MSCRIPT_OPCODE_I2F,
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
    vm->ip = mscript_vm_get_label_idx(vm, 4);

    while (true) {
        if (vm->ip == -1) {
            break;
        }

        struct mscript_opcode op = vm->opcodes.data[vm->ip];
        switch (op.type) {
            case MSCRIPT_OPCODE_IADD:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 + *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FADD:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    float v = *i0 + *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_ISUB:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 - *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FSUB:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    float v = *i0 - *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_IMUL:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 * *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FMUL:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    float v = *i0 * *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_IDIV:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 / *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FDIV:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    float v = *i0 / *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_ILTE:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 <= *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FLTE:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    int v = *i0 <= *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_ILT:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 < *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FLT:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    int v = *i0 < *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_IGTE:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 >= *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FGTE:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    int v = *i0 >= *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_IGT:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 > *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FGT:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    int v = *i0 > *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_IEQ:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 == *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FEQ:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    int v = *i0 == *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_INEQ:
                {
                    int *i1 = (int*) mscript_vm_pop(vm, 4);
                    int *i0 = (int*) mscript_vm_pop(vm, 4);
                    int v = *i0 != *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_FNEQ:
                {
                    float *i1 = (float*) mscript_vm_pop(vm, 4);
                    float *i0 = (float*) mscript_vm_pop(vm, 4);
                    int v = *i0 != *i1;
                    mscript_vm_push(vm, (char*) (&v), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_IINC:
                {
                    assert(vm->fp + op.inc.idx < vm->stack.length);
                    int *i = (int*) (vm->stack.data + vm->fp + op.inc.idx);
                    (*i)++;
                    mscript_vm_push(vm, (char*) (&i), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_F2I:
                {
                    float *f = (float*) mscript_vm_pop(vm, 4);
                    int i = (int)(*f);
                    mscript_vm_push(vm, (char*) (&i), 4);
                    (vm->ip)++;
                }
                break;
            case MSCRIPT_OPCODE_I2F:
                {
                    int *i = (int*) mscript_vm_pop(vm, 4);
                    float f = (float)(*i);
                    mscript_vm_push(vm, (char*) (&f), 4);
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
    int stack_size = 0;
    if (env->blocks.length > 0) {
        struct mscript_env_block top_block = env->blocks.data[env->blocks.length - 1];
        stack_size = top_block.stack_size;
    }

    struct mscript_env_block block;
    block.stack_size = stack_size;
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
static void mscript_compiler_push_opcode_iadd(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fadd(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_isub(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fsub(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_imul(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fmul(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_idiv(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fdiv(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_ilte(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_flte(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_ilt(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_flt(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_igte(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fgte(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_igt(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fgt(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_ieq(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_feq(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_ineq(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_fneq(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_iinc(struct mscript_compiler *compiler, int idx);
static void mscript_compiler_push_opcode_i2f(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_f2i(struct mscript_compiler *compiler);
static void mscript_compiler_push_opcode_int_constant(struct mscript_compiler *compiler, int int_constant);
static void mscript_compiler_push_opcode_float_constant(struct mscript_compiler *compiler, float float_constant);
static void mscript_compiler_push_opcode_set_local(struct mscript_compiler *compiler, int idx, int size);
static void mscript_compiler_push_opcode_get_local(struct mscript_compiler *compiler, int idx, int size);
static void mscript_compiler_push_opcode_jf_label(struct mscript_compiler *compiler, int label);
static void mscript_compiler_push_opcode_jmp_label(struct mscript_compiler *compiler, int label);
static void mscript_compiler_push_opcode_label(struct mscript_compiler *compiler, int label);
static void mscript_compiler_push_opcode_call_label(struct mscript_compiler *compiler, int label, int args_size);
static void mscript_compiler_push_opcode_return(struct mscript_compiler *compiler, int size);
static void mscript_compiler_push_opcode_push(struct mscript_compiler *compiler, int size);
static void mscript_compiler_push_opcode_pop(struct mscript_compiler *compiler, int size);
static int mscript_compiler_get_type_size(struct mscript_compiler *compiler, struct mscript_type type);
static int mscript_compiler_get_l_value_stack_offset(struct mscript_compiler *compiler, struct mscript_expr *expr);
static void mscript_compiler_debug_print_opcodes(struct mscript_compiler *compiler);
static void mscript_compile_stmt(struct mscript_compiler *compiler, struct mscript_stmt *stmt);
static void mscript_compile_expr(struct mscript_compiler *compiler, struct mscript_expr *expr);
static void mscript_compile_expr_with_cast(struct mscript_compiler *compiler, struct mscript_expr *expr, struct mscript_type type);

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

static void mscript_compiler_push_opcode_iadd(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IADD;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fadd(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FADD;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_isub(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_ISUB;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fsub(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FSUB;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_imul(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IMUL;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fmul(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FMUL;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_idiv(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IDIV;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fdiv(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FDIV;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_ilte(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_ILTE;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_flte(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FLTE;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_ilt(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_ILT;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_flt(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FLT;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_igte(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IGTE;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fgte(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FGTE;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_igt(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IGT;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fgt(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FGT;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_ieq(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IEQ;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_feq(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FEQ;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_ineq(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_INEQ;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_fneq(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_FNEQ;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_iinc(struct mscript_compiler *compiler, int idx) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_IINC;
    op.inc.idx = idx;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_f2i(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_F2I;
    array_push(&compiler->opcodes, op);
}

static void mscript_compiler_push_opcode_i2f(struct mscript_compiler *compiler) {
    struct mscript_opcode op;
    op.type = MSCRIPT_OPCODE_I2F;
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
            {
                struct mscript_type struct_type = expr->member_access.left->result_type;
                if (struct_type.type != MSCRIPT_TYPE_STRUCT) {
                    assert(false);
                }

                struct mscript_type_declaration *type_decl = mscript_env_get_type_declaration(&compiler->env, struct_type.struct_name);
                if (!type_decl) {
                    assert(false);
                }

                int stack_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->member_access.left);
                bool found_member = false;
                for (int i = 0; i < type_decl->member_names.length; i++) {
                    if (strcmp(expr->member_access.member_name, type_decl->member_names.data[i]) == 0) {
                        found_member = true;
                        break;
                    }
                    stack_offset += mscript_compiler_get_type_size(compiler, type_decl->member_types.data[i]);
                }
                assert(found_member);
                return stack_offset;
            }
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

static void mscript_compiler_debug_print_opcodes(struct mscript_compiler *compiler) {
    for (int i = 0; i < compiler->opcodes.length; i++) {
        m_logf("%d: ", i);
        struct mscript_opcode op = compiler->opcodes.data[i];
        switch (op.type) {
            case MSCRIPT_OPCODE_IADD:
                m_logf("[IADD]");
                break;
            case MSCRIPT_OPCODE_FADD:
                m_logf("[FADD]");
                break;
            case MSCRIPT_OPCODE_ISUB:
                m_logf("[ISUB]");
                break;
            case MSCRIPT_OPCODE_FSUB:
                m_logf("[FSUB]");
                break;
            case MSCRIPT_OPCODE_IMUL:
                m_logf("[IMUL]");
                break;
            case MSCRIPT_OPCODE_FMUL:
                m_logf("[FMUL]");
                break;
            case MSCRIPT_OPCODE_IDIV:
                m_logf("[IDIV]");
                break;
            case MSCRIPT_OPCODE_FDIV:
                m_logf("[FDIV]");
                break;
            case MSCRIPT_OPCODE_ILTE:
                m_logf("[ILTE]");
                break;
            case MSCRIPT_OPCODE_FLTE:
                m_logf("[FLTE]");
                break;
            case MSCRIPT_OPCODE_ILT:
                m_logf("[ILT]");
                break;
            case MSCRIPT_OPCODE_FLT:
                m_logf("[FLT]");
                break;
            case MSCRIPT_OPCODE_IGTE:
                m_logf("[IGTE]");
                break;
            case MSCRIPT_OPCODE_FGTE:
                m_logf("[FGTE]");
                break;
            case MSCRIPT_OPCODE_IGT:
                m_logf("[IGT]");
                break;
            case MSCRIPT_OPCODE_FGT:
                m_logf("[FGT]");
                break;
            case MSCRIPT_OPCODE_IEQ:
                m_logf("[IEQ]");
                break;
            case MSCRIPT_OPCODE_FEQ:
                m_logf("[FEQ]");
                break;
            case MSCRIPT_OPCODE_INEQ:
                m_logf("[INEQ]");
                break;
            case MSCRIPT_OPCODE_FNEQ:
                m_logf("[FNEQ]");
                break;
            case MSCRIPT_OPCODE_IINC:
                m_logf("[IINC: %d]", op.inc.idx);
                break;
            case MSCRIPT_OPCODE_F2I:
                m_logf("[F2I]");
                break;
            case MSCRIPT_OPCODE_I2F:
                m_logf("[I2F]");
                break;
            case MSCRIPT_OPCODE_INT_CONSTANT:
                m_logf("[INT_CONSTANT: %d]", op.int_constant);
                break;
            case MSCRIPT_OPCODE_FLOAT_CONSTANT:
                m_logf("[FLOAT_CONSTANT: %f]", op.float_constant);
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
                assert(compiler->cur_function);
                mscript_compile_expr_with_cast(compiler, stmt->return_stmt.expr, compiler->cur_function->return_type);
                int size = mscript_compiler_get_type_size(compiler, compiler->cur_function->return_type);
                mscript_compiler_push_opcode_return(compiler, size);
            }
            break;
        case MSCRIPT_STMT_BLOCK:
            {
                mscript_env_push_block(&compiler->env);
                for (int i = 0; i < stmt->block.stmts.length; i++) {
                    mscript_compile_stmt(compiler, stmt->block.stmts.data[i]);
                }
                mscript_env_pop_block(&compiler->env);
            }
            break;
        case MSCRIPT_STMT_EXPR:
            {
                mscript_compile_expr(compiler, stmt->expr);
                int size = mscript_compiler_get_type_size(compiler, stmt->expr->result_type);
                mscript_compiler_push_opcode_pop(compiler, size);
            }
            break;
        case MSCRIPT_STMT_FOR:
            {
                struct mscript_expr *init = stmt->for_stmt.init;
                struct mscript_expr *cond = stmt->for_stmt.cond;
                struct mscript_expr *inc = stmt->for_stmt.inc;
                struct mscript_stmt *body = stmt->for_stmt.body;

                int init_size = mscript_compiler_get_type_size(compiler, init->result_type);
                int cond_size = mscript_compiler_get_type_size(compiler, cond->result_type);
                int inc_size = mscript_compiler_get_type_size(compiler, inc->result_type);

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

                    mscript_compile_expr_with_cast(compiler, expr, type);
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
            {
                int l_value_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->unary_op.operand);
                mscript_compiler_push_opcode_iinc(compiler, l_value_offset);
            }
            break;
    }
}

static void mscript_compile_expr_binary_op_term_factor(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    assert(expr->type == MSCRIPT_EXPR_BINARY_OP);

    mscript_compile_expr_with_cast(compiler, expr->binary_op.left, expr->result_type);
    mscript_compile_expr_with_cast(compiler, expr->binary_op.right, expr->result_type);

    if (expr->result_type.type == MSCRIPT_TYPE_INT) {
        if (expr->binary_op.type == MSCRIPT_BINARY_OP_ADD) {
            mscript_compiler_push_opcode_iadd(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_SUB) {
            mscript_compiler_push_opcode_isub(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_MUL) {
            mscript_compiler_push_opcode_imul(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_DIV) {
            mscript_compiler_push_opcode_idiv(compiler);
        }
        else {
            assert(false);
        }
    }
    else if (expr->result_type.type == MSCRIPT_TYPE_FLOAT) {
        if (expr->binary_op.type == MSCRIPT_BINARY_OP_ADD) {
            mscript_compiler_push_opcode_fadd(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_SUB) {
            mscript_compiler_push_opcode_fsub(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_MUL) {
            mscript_compiler_push_opcode_fmul(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_DIV) {
            mscript_compiler_push_opcode_fdiv(compiler);
        }
        else {
            assert(false);
        }
    }
    else {
        assert(false);
    }
}

static void mscript_compile_expr_binary_op_comparison(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    assert(expr->type == MSCRIPT_EXPR_BINARY_OP);
    assert(expr->result_type.type == MSCRIPT_TYPE_INT);

    struct mscript_type left_type = expr->binary_op.left->result_type;
    struct mscript_type right_type = expr->binary_op.right->result_type;
    struct mscript_type cast_type;

    if (left_type.type == MSCRIPT_TYPE_INT && right_type.type == MSCRIPT_TYPE_INT) {
        cast_type = mscript_type_int();
    }
    else if ((left_type.type == MSCRIPT_TYPE_INT && right_type.type == MSCRIPT_TYPE_FLOAT) ||
            (left_type.type == MSCRIPT_TYPE_FLOAT && right_type.type == MSCRIPT_TYPE_INT) ||
            (left_type.type == MSCRIPT_TYPE_FLOAT && right_type.type == MSCRIPT_TYPE_FLOAT)) {
        cast_type = mscript_type_float();
    }
    else {
        assert(false);
    }

    mscript_compile_expr_with_cast(compiler, expr->binary_op.left, cast_type);
    mscript_compile_expr_with_cast(compiler, expr->binary_op.right, cast_type);

    if (cast_type.type == MSCRIPT_TYPE_INT) {
        if (expr->binary_op.type == MSCRIPT_BINARY_OP_LTE) {
            mscript_compiler_push_opcode_ilte(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_LT) {
            mscript_compiler_push_opcode_ilt(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_GTE) {
            mscript_compiler_push_opcode_igte(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_GT) {
            mscript_compiler_push_opcode_igt(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_EQ) {
            mscript_compiler_push_opcode_ieq(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_NEQ) {
            mscript_compiler_push_opcode_ineq(compiler);
        }
        else {
            assert(false);
        }
    }
    else if (cast_type.type == MSCRIPT_TYPE_FLOAT) {
        if (expr->binary_op.type == MSCRIPT_BINARY_OP_LTE) {
            mscript_compiler_push_opcode_flte(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_LT) {
            mscript_compiler_push_opcode_flt(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_GTE) {
            mscript_compiler_push_opcode_fgte(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_GT) {
            mscript_compiler_push_opcode_fgt(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_EQ) {
            mscript_compiler_push_opcode_feq(compiler);
        }
        else if (expr->binary_op.type == MSCRIPT_BINARY_OP_NEQ) {
            mscript_compiler_push_opcode_fneq(compiler);
        }
        else {
            assert(false);
        }
    }
    else {
        assert(false);
    }
}

static void mscript_compile_expr_binary_op(struct mscript_compiler *compiler, struct mscript_expr *expr) {
    assert(expr->type == MSCRIPT_EXPR_BINARY_OP);

    switch (expr->binary_op.type) {
        case MSCRIPT_BINARY_OP_ADD:
        case MSCRIPT_BINARY_OP_SUB:
        case MSCRIPT_BINARY_OP_MUL:
        case MSCRIPT_BINARY_OP_DIV:
            {
                mscript_compile_expr_binary_op_term_factor(compiler, expr);
            }
            break;
        case MSCRIPT_BINARY_OP_LTE:
        case MSCRIPT_BINARY_OP_LT:
        case MSCRIPT_BINARY_OP_GTE:
        case MSCRIPT_BINARY_OP_GT:
        case MSCRIPT_BINARY_OP_EQ:
        case MSCRIPT_BINARY_OP_NEQ:
            {
                mscript_compile_expr_binary_op_comparison(compiler, expr);
            }
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
                    struct mscript_type arg_type = decl->arg_types.data[i];
                    args_size += mscript_compiler_get_type_size(compiler, arg_type);
                    mscript_compile_expr_with_cast(compiler, expr->call.args.data[i], arg_type);
                }

                mscript_compiler_push_opcode_call_label(compiler, decl->label, args_size);
            }
            break;
        case MSCRIPT_EXPR_ASSIGNMENT:
            {
                int l_value_stack_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->assignment.left);
                int l_size = mscript_compiler_get_type_size(compiler, expr->assignment.left->result_type);
                mscript_compile_expr_with_cast(compiler, expr->assignment.right, expr->assignment.left->result_type);
                mscript_compiler_push_opcode_set_local(compiler, l_value_stack_offset, l_size);
            }
            break;
        case MSCRIPT_EXPR_MEMBER_ACCESS:
            {
                struct mscript_type struct_type = expr->member_access.left->result_type;
                if (struct_type.type != MSCRIPT_TYPE_STRUCT) {
                    assert(false);
                }
                const char *member_name = expr->member_access.member_name;

                struct mscript_type_declaration *type_decl = mscript_env_get_type_declaration(&compiler->env, struct_type.struct_name);
                if (!type_decl) {
                    assert(false);
                }

                int stack_offset = mscript_compiler_get_l_value_stack_offset(compiler, expr->member_access.left);
                int member_size = 0;
                bool found = false;
                for (int i = 0; i < type_decl->member_names.length; i++) {
                    int type_size = mscript_compiler_get_type_size(compiler, type_decl->member_types.data[i]);
                    if (strcmp(type_decl->member_names.data[i], member_name) == 0) {
                        member_size = type_size; 
                        found = true;
                        break;
                    }
                    stack_offset += type_size; 
                }

                if (!found) {
                    assert(false);
                }

                mscript_compiler_push_opcode_get_local(compiler, stack_offset, member_size);
            }
            break;
        case MSCRIPT_EXPR_ARRAY_ACCESS:
            {
                assert(false);
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

static void mscript_compile_expr_with_cast(struct mscript_compiler *compiler, struct mscript_expr *expr, struct mscript_type type) {
    mscript_compile_expr(compiler, expr);
    if (type.type != expr->result_type.type) {
        if ((type.type == MSCRIPT_TYPE_INT) && (expr->result_type.type == MSCRIPT_TYPE_FLOAT)) {
            mscript_compiler_push_opcode_f2i(compiler);
        }
        else if ((type.type == MSCRIPT_TYPE_FLOAT) && (expr->result_type.type == MSCRIPT_TYPE_INT)) {
            mscript_compiler_push_opcode_i2f(compiler);
        }
        else {
            assert(false);
        }
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
