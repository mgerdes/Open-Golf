#define _CRT_SECURE_NO_WARNINGS

#include "sl.h"

#include "log.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

static int num_allocs = 0;

void *sl_alloc(size_t num_bytes) {
    num_allocs++;
    return malloc(num_bytes);
}

void sl_free(void *ptr) {
    num_allocs--;
    free(ptr);
}

//
// EXPR 
//

static bool sl_expr_is_symbol(struct sl_expr *expr, const char *symbol) {
    return (expr->type == SL_EXPR_SYMBOL) && (strcmp(expr->symbol, symbol) == 0);
}

static void sl_expr_delete(struct sl_expr *expr) {
    switch (expr->type) {
        case SL_EXPR_ERROR:
            sl_free(expr->error_message);
            break;
        case SL_EXPR_NUMBER:
            break;
        case SL_EXPR_SYMBOL:
            sl_free(expr->symbol);
            break;
        case SL_EXPR_LIST:
            for (int i = 0; i < expr->list.length; i++) {
                sl_expr_delete(expr->list.data[i]);
            }
            array_deinit(&expr->list);
            break;
    }
    sl_free(expr);
}

static struct sl_expr *sl_expr_number_create(float number) {
    struct sl_expr *expr = sl_alloc(sizeof(struct sl_expr));
    expr->type = SL_EXPR_NUMBER;
    expr->number = number;
    return expr;
}

static struct sl_expr *sl_expr_symbol_create(const char *symbol) {
    struct sl_expr *expr = sl_alloc(sizeof(struct sl_expr));
    expr->type = SL_EXPR_SYMBOL;
    expr->symbol = sl_alloc(SL_SYMBOL_MAX_LEN + 1);
    strncpy(expr->symbol, symbol, SL_SYMBOL_MAX_LEN);
    expr->symbol[SL_SYMBOL_MAX_LEN] = 0;
    return expr;
}

static struct sl_expr *sl_expr_list_create(struct array_sl_expr_ptr items) {
    struct sl_expr *expr = sl_alloc(sizeof(struct sl_expr));
    expr->type = SL_EXPR_LIST;
    expr->list = items;
    return expr;
}

static struct sl_expr *sl_expr_error_create(const char *message, ...) {
    struct sl_expr *expr = sl_alloc(sizeof(struct sl_expr));
    expr->type = SL_EXPR_ERROR;
    expr->error_message = sl_alloc(SL_ERROR_MESSAGE_MAX_LEN + 1);
    va_list arg;
    va_start(arg, message);
    vsnprintf(expr->error_message, SL_ERROR_MESSAGE_MAX_LEN, message, arg);
    expr->error_message[SL_ERROR_MESSAGE_MAX_LEN] = 0;
    va_end(arg);
    return expr;
}

void sl_expr_print(struct sl_expr *expr) {
    if (expr->type == SL_EXPR_LIST) {
        m_logf("(");
        for (int i = 0; i < expr->list.length; i++) {
            sl_expr_print(expr->list.data[i]);
            if (i != expr->list.length - 1) {
                m_logf(" ");
            }
        }
        m_logf(")");
    }
    else if (expr->type == SL_EXPR_NUMBER) {
        m_logf("%f", expr->number);
    }
    else if (expr->type == SL_EXPR_SYMBOL) {
        m_logf("%s", expr->symbol); 
    }
    else if (expr->type == SL_EXPR_ERROR) {
        m_logf("ERROR: %s", expr->error_message);
    }
}

//
// VAL 
//

static bool sl_val_is_true(struct sl_val *val) {
    return val->type != SL_VAL_NULL;
}

static struct sl_val sl_val_boolean(bool boolean) {
    struct sl_val val;
    val.type = SL_VAL_BOOLEAN;
    val.boolean = boolean;
    return val;
}

static struct sl_val sl_val_null(void) {
    struct sl_val val;
    val.type = SL_VAL_NULL;
    return val;
}

static struct sl_val sl_val_number(float number) {
    struct sl_val val;
    val.type = SL_VAL_NUMBER;
    val.number = number;
    return val;
}

static struct sl_val sl_val_function(int fn_pos) {
    struct sl_val val;
    val.type = SL_VAL_FUNCTION;
    val.fn_pos = fn_pos;
    return val;
}

static struct sl_val sl_val_primitive_function(void) {
    struct sl_val val;
    val.type = SL_VAL_PRIMITIVE_FUNCTION;
    return val;
}

static struct sl_val sl_val_array(void) {
    struct sl_val val;
    val.type = SL_VAL_ARRAY;
    array_init(&(val.array)); 
    return val;
}

static struct sl_val sl_val_vec2(vec2 v2) {
    struct sl_val val;
    val.type = SL_VAL_VEC2;
    val.v2 = v2;
    return val;
}

static struct sl_val sl_val_vec3(vec3 v3) {
    struct sl_val val;
    val.type = SL_VAL_VEC3;
    val.v3 = v3;
    return val;
}

void sl_val_print(struct sl_val *val) {
    switch (val->type) {
        case SL_VAL_NULL:
            m_logf("NULL");
            break;
        case SL_VAL_NUMBER:
            m_logf("%f", val->number);
            break;
        case SL_VAL_FUNCTION:
            m_logf("<FUNCTION: %d>", val->fn_pos);
            break;
        case SL_VAL_PRIMITIVE_FUNCTION:
            m_logf("<PRIMITIVE_FUNCTION>");
            break;
        case SL_VAL_ARRAY:
            m_logf("[");
            for (int i = 0; i < val->array.length; i++) {
                sl_val_print(val->array.data[i]);
                if (i != val->array.length - 1) {
                    m_logf(", ");
                }
            }
            m_logf("]");
            break;
        case SL_VAL_VEC2:
            m_logf("<%f, %f>", val->v2.x, val->v2.y);
            break;
        case SL_VAL_VEC3:
            m_logf("<%f, %f, %f>", val->v3.x, val->v3.y, val->v3.z);
            break;
        case SL_VAL_ERROR:
            m_logf("ERROR: %s", val->error_message);
            break;
    }
}

//
// ENV 
//

struct sl_env {
    int num_symbols;
    struct sl_env *global_env;
    map_int_t map;
};

static void sl_env_init(struct sl_env *env, struct sl_env *global_env) {
    env->num_symbols = 0;
    env->global_env = global_env;
    map_init(&env->map);
}

static void sl_env_deinit(struct sl_env *env) {
    map_deinit(&env->map);
}

static int sl_env_add_if_missing(struct sl_env *env, const char *symbol) {
    int *pos = map_get(&env->map, symbol);
    if (pos) {
        return *pos; 
    }
    else {
        map_set(&env->map, symbol, env->num_symbols);
        return env->num_symbols++;
    }
}

static int sl_env_get(struct sl_env *env, const char *symbol) {
    int *pos = map_get(&env->map, symbol);
    if (pos) {
        return *pos;
    }
    return -1;
}

//
// TOKENIZE
//

static bool sl_is_start_of_symbol(char c) {
    return (c >= 'a' && c <= 'z') || 
        (c >= 'A' && c <= 'Z') ||
        (c == '=') || 
        (c == '+') ||
        (c == '-') ||
        (c == '<') ||
        (c == '>') 
        ;
}

static bool sl_is_part_of_symbol(char c) {
    return (c >= 'a' && c <= 'z') || 
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '=') || 
        (c == '+') ||
        (c == '-') ||
        (c == '<') ||
        (c == '>') 
        ;
}

static bool sl_is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static bool sl_is_start_of_number(char c) {
    return sl_is_digit(c);
}

enum sl_token_type {
    SL_TOKEN_SYMBOL,
    SL_TOKEN_NUMBER,
    SL_TOKEN_CHARACTER,
    SL_TOKEN_END_OF_FILE,
};

struct sl_token {
    int line, col;
    enum sl_token_type type;  
    union {
        char *symbol;
        float number;
        char character;
    };
};
array_t(struct sl_token, array_sl_token)

static bool sl_token_is_character(struct sl_token token, char c) {
    return token.type == SL_TOKEN_CHARACTER &&
        token.character == c;
}

static bool sl_token_is_number(struct sl_token token) {
    return token.type == SL_TOKEN_NUMBER;
}

static bool sl_token_is_symbol(struct sl_token token) {
    return token.type == SL_TOKEN_SYMBOL;
}

static bool sl_token_is_eof(struct sl_token token) {
    return token.type == SL_TOKEN_END_OF_FILE;
}

static struct sl_token sl_token_end_of_file_create(int line, int col) {
    struct sl_token token;
    token.line = line;
    token.col = col;
    token.type = SL_TOKEN_END_OF_FILE;
    return token;
}

static struct sl_token sl_token_character_create(char c, int line, int col) {
    struct sl_token token;
    token.line = line;
    token.col = col;
    token.type = SL_TOKEN_CHARACTER;
    token.character = c;
    return token;
}

static struct sl_token sl_token_number_create(const char *text, int *len, int line, int col) {
    *len = 0;
    float number = 0.0f;
    while (sl_is_digit(text[*len])) {
        number = 10.0f*number + (text[*len] - '0');
        (*len)++;
    }

    struct sl_token token;
    token.line = line;
    token.col = col;
    token.type = SL_TOKEN_NUMBER;
    token.number = number;
    return token;
}

static struct sl_token sl_token_symbol_create(const char *text, int *len, int line, int col) {
    *len = 0;
    while (sl_is_part_of_symbol(text[*len])) {
        (*len)++;
    }

    char *symbol = malloc((*len) + 1);
    for (int i = 0; i < *len; i++) {
        symbol[i] = text[i];
    }
    symbol[(*len)] = 0;

    struct sl_token token;
    token.line = line;
    token.col = col;
    token.type = SL_TOKEN_SYMBOL;
    token.symbol = symbol;
    return token;
}

static void sl_tokens_print(struct sl_token *tokens) {
    while (!sl_token_is_eof(tokens[0])) {
        struct sl_token token = tokens[0];
        switch (token.type) {
            case SL_TOKEN_NUMBER:
                m_logf("[NUM: %f]\n", token.number);
                break;
            case SL_TOKEN_SYMBOL:
                m_logf("[SYM: %s]\n", token.symbol);
                break;
            case SL_TOKEN_CHARACTER:
                m_logf("[CHAR: %c]\n", token.character);
                break;
            case SL_TOKEN_END_OF_FILE:
                m_logf("[EOF]\n");
                break;
        }
        tokens++;
    }
}

static bool sl_tokenize_program(const char *text, struct array_sl_token *tokens) {
    int line = 1;
    int col = 1;
    int pos = 0;

    while (true) {
        if (text[pos] == 0) {
            struct sl_token token = sl_token_end_of_file_create(line, col);
            array_push(tokens, token);
            break;        
        }
        else if (text[pos] == ' ' || text[pos] == '\t') {
            col++;
            pos++;
        }
        else if (text[pos] == '\n') {
            col = 1;
            line++;
            pos++;
        }
        else if (text[pos] == '(' || text[pos] == ')') {
            struct sl_token token = sl_token_character_create(text[pos], line, col);
            array_push(tokens, token);
            col++;
            pos++;
        }
        else if (text[pos] == ';') {
            while ((text[pos] != 0) && (text[pos] != '\n')) {
                pos++; 
                col++;
            }
        }
        else if (sl_is_start_of_number(text[pos])) {
            int len;
            struct sl_token token = sl_token_number_create(text + pos, &len, line, col);
            array_push(tokens, token);
            pos += len;
            col += len;
        }
        else if (sl_is_start_of_symbol(text[pos])) {
            int len;
            struct sl_token token = sl_token_symbol_create(text + pos, &len, line, col);
            array_push(tokens, token);
            pos += len;
            col += len;
        }
        else {
            m_logf("Unknown character found: %c\n", text[pos]);
            m_logf("Line: %d\n", line);
            m_logf("Col: %d\n", col);
            return false;
        }
    }

    return true;
}

//
// PARSE
//

static struct sl_expr *sl_expr_parse(struct sl_token **tokens);
static struct sl_expr *sl_expr_list_parse(struct sl_token **tokens);

static struct sl_expr *sl_expr_parse(struct sl_token **tokens) {
    if (sl_token_is_character((*tokens)[0], '(')) {
        (*tokens)++;
        return sl_expr_list_parse(tokens);
    }
    else if (sl_token_is_number((*tokens)[0])) {
        return sl_expr_number_create(((*tokens)++)->number);
    }
    else if (sl_token_is_symbol((*tokens)[0])) {
        return sl_expr_symbol_create(((*tokens)++)->symbol);
    }
    else if (sl_token_is_eof((*tokens)[0])) {
        return sl_expr_error_create("Unexpected end of file");
    }

    assert(false);
    return NULL;
}


static struct sl_expr *sl_expr_list_parse(struct sl_token **tokens) {
    struct array_sl_expr_ptr list_items;
    array_init(&list_items);

    while (!sl_token_is_character((*tokens)[0], ')')) {
        struct sl_expr *expr = sl_expr_parse(tokens);
        if (expr->type == SL_EXPR_ERROR) {
            for (int i = 0; i < list_items.length; i++) {
                sl_expr_delete(list_items.data[i]);
            }
            array_deinit(&list_items);
            return expr;
        }
        array_push(&list_items, expr);
    }
    (*tokens)++;

    return sl_expr_list_create(list_items);
}

//
// EVAL
//

/*
static struct sl_val *sl_expr_eval(struct sl_expr *expr, struct sl_env *env);
static struct sl_val *sl_expr_list_eval(struct sl_expr *expr, struct sl_env *env);
static struct sl_val *sl_expr_number_eval(struct sl_expr *expr, struct sl_env *env);
static struct sl_val *sl_expr_symbol_eval(struct sl_expr *expr, struct sl_env *env);

static struct sl_val *sl_expr_list_eval(struct sl_expr *expr, struct sl_env *env) {
    assert(expr->type == SL_EXPR_LIST);

    if (expr->list.length == 0) {
        return sl_val_null_create();
    }

    if (sl_expr_is_symbol(expr->list.data[0], "define") ||
            sl_expr_is_symbol(expr->list.data[0], "=")) {
        // (define name body)
        struct sl_expr *name_expr = expr->list.data[1];

        if (name_expr->type == SL_EXPR_LIST) {
            if (expr->list.length < 3) {
                return sl_val_error_create("Expect atleast 3 arguments to define");
            }

            struct array_sl_expr_ptr body_exprs;
            array_init(&body_exprs);
            for (int i = 2; i < expr->list.length; i++) {
                array_push(&body_exprs, expr->list.data[i]);
            }

            if (name_expr->list.length == 0) {
                return sl_val_error_create("Invalid define expression");
            }
            for (int i = 0; i < name_expr->list.length; i++) {
                if (name_expr->list.data[i]->type != SL_EXPR_SYMBOL) {
                    return sl_val_error_create("Invalid define expression");
                }
            }

            struct array_char_ptr arg_names;
            array_init(&arg_names);
            for (int i = 1; i < name_expr->list.length; i++) {
                array_push(&arg_names, name_expr->list.data[i]->symbol);
            }
            struct sl_val *val = sl_val_function_create(env, &arg_names, body_exprs);
            sl_env_set(env, name_expr->list.data[0]->symbol, val);
            return val;
        }
        else if (name_expr->type == SL_EXPR_SYMBOL) {
            if (expr->list.length != 3) {
                return sl_val_error_create("Expect 3 arguments to define");
            }

            struct sl_expr *body_expr = expr->list.data[2];

            struct sl_val *val = sl_expr_eval(body_expr, env);
            if (val->type == SL_VAL_ERROR) {
                return val;
            }
            sl_env_set(env, name_expr->symbol, val);
            return val;
        }
        else {
            return sl_val_error_create("Invalid define expression");
        }
    }
    else if (sl_expr_is_symbol(expr->list.data[0], "if")) {
        // (if cond expr1 expr2)
        if (expr->list.length != 4) {
            return sl_val_error_create("Invalid if expression");
        }

        struct sl_expr *cond_expr = expr->list.data[1];
        struct sl_expr *expr1 = expr->list.data[2];
        struct sl_expr *expr2 = expr->list.data[3];

        struct sl_val *cond_val = sl_expr_eval(cond_expr, env);
        if (cond_val->type == SL_VAL_ERROR) {
            return cond_val;
        }

        if (sl_val_is_true(cond_val)) {
            return sl_expr_eval(expr1, env);
        }
        else {
            return sl_expr_eval(expr2, env);
        }
    }
    else if (sl_expr_is_symbol(expr->list.data[0], "for")) {
        // (for init condition increment body) 
        if (expr->list.length < 5) {
            return sl_val_error_create("Expect atleast 5 arguments to for");
        }

        struct sl_expr *init_expr = expr->list.data[1];
        struct sl_expr *condition_expr = expr->list.data[2];
        struct sl_expr *increment_expr = expr->list.data[3];

        struct sl_val *init_val = sl_expr_eval(init_expr, env);
        struct sl_val *condition_val = sl_expr_eval(condition_expr, env);
        struct sl_val *increment_val, *body_val = sl_val_null_create();

        while (sl_val_is_true(condition_val)) {
            for (int i = 4; i < expr->list.length; i++) {
                struct sl_expr *body_expr = expr->list.data[i];
                body_val = sl_expr_eval(body_expr, env);
            }
            struct sl_val *increment_val = sl_expr_eval(increment_expr, env);
            condition_val = sl_expr_eval(condition_expr, env);
        }

        return body_val;
    }
    else {
        struct sl_val *fn_val = sl_expr_eval(expr->list.data[0], env);
        if (fn_val->type == SL_VAL_ERROR) {
            return fn_val;
        }

        if (fn_val->type == SL_VAL_PRIMITIVE_FUNCTION) {
            struct array_sl_val_ptr arg_vals;
            array_init(&arg_vals);
            for (int i = 1; i < expr->list.length; i++) {
                struct sl_val *arg_val = sl_expr_eval(expr->list.data[i], env);
                if (arg_val->type == SL_VAL_ERROR) {
                    return arg_val;
                }
                array_push(&arg_vals, arg_val);
            }

            struct sl_val *val = fn_val->primitive_function(&arg_vals);
            return val;
        }
        else if (fn_val->type == SL_VAL_FUNCTION) {
            if (fn_val->fn.arg_names.length != expr->list.length - 1) {
                return sl_val_error_create("Incorrect number of arguments"); 
            }

            struct sl_env *new_env = sl_alloc_env();
            new_env->parent_env = fn_val->fn.env;
            for (int i = 1; i < expr->list.length; i++) {
                struct sl_val *arg_val = sl_expr_eval(expr->list.data[i], env);
                if (arg_val->type == SL_VAL_ERROR) {
                    return arg_val;
                }
                sl_env_set(new_env, fn_val->fn.arg_names.data[i - 1], arg_val);
            }

            struct sl_val *val = NULL;
            for (int i = 0; i < fn_val->fn.body.length; i++) {
                val = sl_expr_eval(fn_val->fn.body.data[i], new_env);
            }
            assert(val);
            return val;
        }
        else {
            return sl_val_error_create("Invalid function"); 
        }
    }

    assert(false);
    return NULL;
}

static struct sl_val *sl_expr_number_eval(struct sl_expr *expr, struct sl_env *env) {
    assert(expr->type == SL_EXPR_NUMBER);
    return sl_val_number_create(expr->number);
}

static struct sl_val *sl_expr_symbol_eval(struct sl_expr *expr, struct sl_env *env) {
    assert(expr->type == SL_EXPR_SYMBOL);
    while (env) {
        struct sl_val **val = map_get(&env->map, expr->symbol);
        if (val) {
            return *val;
        }
        env = env->parent_env;
    }
    return sl_val_error_create("Unknown symbol: %s", expr->symbol);
}

static struct sl_val *sl_expr_eval(struct sl_expr *expr, struct sl_env *env) {
    switch (expr->type) {
        case SL_EXPR_NUMBER:
            return sl_expr_number_eval(expr, env);
            break;
        case SL_EXPR_SYMBOL:
            return sl_expr_symbol_eval(expr, env);
            break;
        case SL_EXPR_LIST:
            return sl_expr_list_eval(expr, env);
            break;
        case SL_EXPR_ERROR:
            assert(false);
            break;
    }

    assert(false);
    return NULL;
}
*/

//
// COMPILER
//

static void sl_expr_compile(struct sl_expr *expr, struct array_sl_opcode *ops, struct sl_env *env);
static void sl_expr_number_compile(struct sl_expr *expr, struct array_sl_opcode *ops);
static void sl_expr_symbol_compile(struct sl_expr *expr, struct array_sl_opcode *ops, struct sl_env *env);
static void sl_expr_list_compile(struct sl_expr *expr, struct array_sl_opcode *ops, struct sl_env *env);

enum sl_opcode_type {
    SL_OPCODE_PUSH,
    SL_OPCODE_POP,
    SL_OPCODE_JT,
    SL_OPCODE_JF,
    SL_OPCODE_JMP,
    SL_OPCODE_RETURN,
    SL_OPCODE_CALL,
    SL_OPCODE_CALL_PRIMITIVE,
    SL_OPCODE_SUB,
    SL_OPCODE_ADD,
    SL_OPCODE_READ_GLOBAL,
    SL_OPCODE_READ_STACK,
    SL_OPCODE_SET,
};

struct sl_opcode {
    enum sl_opcode_type type;

    union {
        int pos;
        struct sl_val val;
    };
};
array_t(struct sl_opcode, array_sl_opcode)

static void sl_opcodes_print(struct array_sl_opcode *ops) {
    for (int i = 0; i < ops->length; i++) {
        m_logf("%d: ", i);
        struct sl_opcode op = ops->data[i];
        switch (op.type) {
            case SL_OPCODE_PUSH:
                m_logf("[PUSH: ");
                sl_val_print(&op.val);
                m_logf("]");
                break;
            case SL_OPCODE_POP:
                m_logf("[POP]");
                break;
            case SL_OPCODE_JT:
                m_logf("[JT: %d]", op.pos);
                break;
            case SL_OPCODE_JF:
                m_logf("[JF: %d]", op.pos);
                break;
            case SL_OPCODE_JMP:
                m_logf("[JMP: %d]", op.pos);
                break;
            case SL_OPCODE_RETURN:
                m_logf("[RETURN]");
                break;
            case SL_OPCODE_CALL:
                m_logf("[CALL: %d]", op.pos);
                break;
            case SL_OPCODE_CALL_PRIMITIVE:
                m_logf("[CALL_PRIMTIVE]");
                break;
            case SL_OPCODE_SUB:
                m_logf("[SUB]");
                break;
            case SL_OPCODE_ADD:
                m_logf("[ADD]");
                break;
            case SL_OPCODE_READ_GLOBAL:
                m_logf("[READ_GLOBAL: %d]", op.pos);
                break;
            case SL_OPCODE_READ_STACK:
                m_logf("[READ_STACK: %d]", op.pos);
                break;
            case SL_OPCODE_SET:
                m_logf("[SET: %d]", op.pos);
                break;
        }
        m_logf("\n");
    }
}

static struct sl_opcode sl_opcode_push(struct sl_val val) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_PUSH;
    opcode.val = val;
    return opcode;
}

static struct sl_opcode sl_opcode_pop(void) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_POP;
    return opcode;
}

static struct sl_opcode sl_opcode_jt(int pos) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_JT;
    opcode.pos = pos;
    return opcode;
}

static struct sl_opcode sl_opcode_jf(int pos) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_JF;
    opcode.pos = pos;
    return opcode;
}

static struct sl_opcode sl_opcode_jmp(int pos) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_JMP;
    opcode.pos = pos;
    return opcode;
}

static struct sl_opcode sl_opcode_return(void) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_RETURN;
    return opcode;
}

static struct sl_opcode sl_opcode_call(int num_args) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_CALL;
    opcode.pos = num_args;
    return opcode;
}

static struct sl_opcode sl_opcode_call_primitive(void) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_CALL_PRIMITIVE;
    return opcode;
}

static struct sl_opcode sl_opcode_sub(void) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_SUB;
    return opcode;
}

static struct sl_opcode sl_opcode_add(void) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_ADD;
    return opcode;
}

static struct sl_opcode sl_opcode_read_global(int pos) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_READ_GLOBAL;
    opcode.pos = pos;
    return opcode;
}

static struct sl_opcode sl_opcode_read_stack(int pos) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_READ_STACK;
    opcode.pos = pos;
    return opcode;
}

static struct sl_opcode sl_opcode_set(int pos) {
    struct sl_opcode opcode;
    opcode.type = SL_OPCODE_SET;
    opcode.pos = pos;
    return opcode;
}

static void sl_expr_number_compile(struct sl_expr *expr, struct array_sl_opcode *ops) {
    assert(expr->type == SL_EXPR_NUMBER);

    array_push(ops, sl_opcode_push(sl_val_number(expr->number)));
}

static void sl_expr_symbol_compile(struct sl_expr *expr, struct array_sl_opcode *ops, struct sl_env *env) {
    assert(expr->type == SL_EXPR_SYMBOL);

    int pos = sl_env_get(env, expr->symbol);
    if (pos < 0) {
        if (env->global_env) {
            pos = sl_env_get(env->global_env, expr->symbol);
            if (pos < 0) {
                array_push(ops, sl_opcode_push(sl_val_null()));
            }
            else {
                array_push(ops, sl_opcode_read_global(pos));
            }
        }
        else {
            array_push(ops, sl_opcode_push(sl_val_null()));
        }
    }
    else {
        if (env->global_env) {
            array_push(ops, sl_opcode_read_stack(pos));
        }
        else {
            array_push(ops, sl_opcode_read_global(pos));
        }
    }
}

static void sl_expr_list_compile(struct sl_expr *expr, struct array_sl_opcode *ops, struct sl_env *env) {
    assert(expr->type == SL_EXPR_LIST);

    if (expr->list.length == 0) {
        array_push(ops, sl_opcode_push(sl_val_null()));
    }
    else if (sl_expr_is_symbol(expr->list.data[0], "define") ||
            sl_expr_is_symbol(expr->list.data[0], "=")) {
        // (define name body)
        if (expr->list.length != 3) {
            // ERROR: Expect 3 arguments to define
            assert(false);
        }

        struct sl_expr *name_expr = expr->list.data[1];
        struct sl_expr *body_expr = expr->list.data[2];

        if (name_expr->type != SL_EXPR_SYMBOL) {
            // ERROR: Expect first argument to define to be a symbol
            assert(false);
        }

        sl_expr_compile(body_expr, ops, env);
        array_push(ops, sl_opcode_set(sl_env_add_if_missing(env, name_expr->symbol)));
    }
    else if (sl_expr_is_symbol(expr->list.data[0], "lambda")) {
        // (lambda (args) body)
        if (expr->list.length < 3) {
            // ERROR: Expect atleast 3 arguments to lambda
            assert(false);
        }
        if (env->global_env) {
            // ERROR: Can only create lambdas in the global environment
            assert(false);
        }

        struct sl_expr *args_expr = expr->list.data[1];
        if (args_expr->type != SL_EXPR_LIST) {
            // ERROR: Expect second arg to lambda to be a list
            assert(false);
        }
        for (int i = 0; i < args_expr->list.length; i++) {
            if (args_expr->list.data[i]->type != SL_EXPR_SYMBOL) {
                // ERROR: Expect all args to lambda to be a symbol
                assert(false);
            }
        }

        int jmp_pos = ops->length;
        array_push(ops, sl_opcode_jmp(0));

        {
            struct sl_env fn_env;
            sl_env_init(&fn_env, env);
            for (int i = 0; i < args_expr->list.length; i++) {
                sl_env_add_if_missing(&fn_env, args_expr->list.data[i]->symbol); 
            }
            for (int i = 2; i < expr->list.length; i++) {
                sl_expr_compile(expr->list.data[i], ops, &fn_env);
                if (i != expr->list.length - 1) {
                    array_push(ops, sl_opcode_pop());
                }
            }
            array_push(ops, sl_opcode_return());
            sl_env_deinit(&fn_env);
        }

        ops->data[jmp_pos].pos = ops->length;
        array_push(ops, sl_opcode_push(sl_val_function(jmp_pos + 1)));
    }
    else if (sl_expr_is_symbol(expr->list.data[0], "if")) {
        // (if cond expr1 expr2)
        if (expr->list.length != 4) {
            assert(false);
        }

        struct sl_expr *cond_expr = expr->list.data[1];
        struct sl_expr *expr1 = expr->list.data[2];
        struct sl_expr *expr2 = expr->list.data[3];

        int jf_op_pos, jmp_op_pos;

        sl_expr_compile(cond_expr, ops, env);
        jf_op_pos = ops->length;
        array_push(ops, sl_opcode_jf(0));
        sl_expr_compile(expr1, ops, env);
        ops->data[jf_op_pos].pos = ops->length + 2;
        jmp_op_pos = ops->length;
        array_push(ops, sl_opcode_jmp(0));
        sl_expr_compile(expr2, ops, env);
        ops->data[jmp_op_pos].pos = ops->length + 1;
    }
    else if (sl_expr_is_symbol(expr->list.data[0], "for")) {
        // (for init cond inc body)
        if (expr->list.length < 5) {
            // ERROR: Expect atleast 5 arguments to for
            assert(false);
        }

        struct sl_expr *init_expr = expr->list.data[1];
        struct sl_expr *cond_expr = expr->list.data[2];
        struct sl_expr *inc_expr = expr->list.data[3];

        int cond_pos, jt_pos;

        sl_expr_compile(init_expr, ops, env);
        array_push(ops, sl_opcode_pop());
        cond_pos = ops->length;
        sl_expr_compile(cond_expr, ops, env);
        jt_pos = ops->length;
        array_push(ops, sl_opcode_jf(0));
        for (int i = 4; i < expr->list.length; i++) {
            sl_expr_compile(expr->list.data[i], ops, env);
            array_push(ops, sl_opcode_pop());
        }
        sl_expr_compile(inc_expr, ops, env);
        array_push(ops, sl_opcode_pop());
        array_push(ops, sl_opcode_jmp(cond_pos));
        ops->data[jt_pos].pos = ops->length;
        array_push(ops, sl_opcode_push(sl_val_null()));
    }
    else {
        struct sl_expr *fn_expr = expr->list.data[0];
        sl_expr_compile(fn_expr, ops, env);
        for (int i = 1; i < expr->list.length; i++) {
            sl_expr_compile(expr->list.data[i], ops, env);
        }
        array_push(ops, sl_opcode_call(expr->list.length - 1));
    }
}

static void sl_expr_compile(struct sl_expr *expr, struct array_sl_opcode *ops, struct sl_env *env) {
    switch (expr->type) {
        case SL_EXPR_NUMBER: 
            sl_expr_number_compile(expr, ops);
            break;
        case SL_EXPR_SYMBOL:
            sl_expr_symbol_compile(expr, ops, env);
            break;
        case SL_EXPR_LIST:
            sl_expr_list_compile(expr, ops, env);
            break;
        case SL_EXPR_ERROR:
            assert(false);
            break;
    }
}

//
// PRIMITIVES
//

static void sl_primitive_add(struct sl_vm *vm, int num_args) {
    float number = 0;
    for (int i = 0; i < num_args; i++) {
        number += sl_val_to_float(val);
    }
    sl_vm_return(vm, sl_val_number(number));
}

static void sl_primtive_sub(struct sl_vm *vm, int num_args) {
    float number = 0;
    if (num_args == 1) {
        number = -sl_val_to_float(vm->call_stack.data[vm->call_stack_top]);
    }
    else if (num_args > 1) {
        number = sl_val_to_float(vm->call_stack.data[vm->call_stack_top]);
        for (int i = 1; i < num_args; i++) {
            number -= sl_val_to_float(vm->call_stack.data[vm->call_stack_top + i]);
        }
    }
    sl_vm_return(vm, sl_val_number(number));
}

static void sl_primitive_less_then(struct sl_vm *vm, int num_args) {
    if (num_args < 2) {
        sl_vm_return(vm, sl_val_false());
        return;
    }
    else {
        for (int i = 0; i < num_args - 1; i++) {
            float num0 = sl_val_to_float(sl_vm_stack_get(vm, i));
            float num1 = sl_val_to_float(sl_vm_stack_get(vm, i + 1));
            if (!(num0 < num1)) {
                sl_vm_return(vm, sl_val_false());
                return;
            }
        }
    }
    sl_vm_return(vm, sl_val_true());
}

static void sl_primitive_array_create(struct sl_vm *vm, int num_args) {
    if (num_args == 0) {
        struct array_sl_val *array = malloc(sizeof(struct array_sl_val));
        array_init(array);
        sl_vm_return(vm, sl_val_array(array));
    }
    else {
        sl_vm_return(vm, sl_val_null());
    }
}

static void sl_primitive_array_push(struct sl_vm *vm, int num_args) {
    if (num_args < 2) {
    }

    struct sl_val array_val = sl_vm_stack_get(vm, 0);
    if (array_val.type != SL_VAL_ARRAY) {
    }

    for (int i = 1; i < num_args; i++) {
        array_push(array_val.array, sl_vm_stack_get(vm, i));
    }
    sl_vm_return(vm, sl_val_null());
}

static void sl_primitive_array_get(struct sl_vm *vm, int num_args) {
    if (num_args != 2) {
    }

    struct sl_val array_val = sl_vm_stack_get(vm, 0);
    if (array_val.type != SL_VAL_ARRAY) {
    }

    struct sl_val idx_val = sl_vm_stack_get(vm, 1);
    if (idx_val.type != SL_VAL_NUMBER) {
    }

    sl_vm_return(vm, array_val.array->data[(int)idx_val.number]);
}

//
// VM
//
struct sl_vm {
    int stack_ptr;
    struct array_sl_val stack;

    int opcode_ptr;
    struct array_int opcode_ptr_stack;
    struct array_opcode opcodes;
};

static void sl_vm_init(struct sl_vm *vm) {
    vm->stack_ptr = 0;
    array_init(&vm->stack);

    vm->opcode_ptr = 0;
    array_init(&vm->opcode_ptr_stack);
    array_init(&vm->opcodes);
}

static void sl_vm_call(struct sl_vm *vm, int fn_opcode_ptr) {
    array_push(&vm->opcode_ptr_stack, vm->opcode_ptr);
    vm->opcode_ptr = fn_opcode_ptr;
}

static void sl_vm_return(struct sl_vm *vm, struct sl_val val) {
    vm->stack.length = vm->stack_ptr - 1;
    array_push(&vm->stack, val);
    vm->opcode_ptr = array_pop(&vm->opcode_ptr_stack);
}

static struct sl_val sl_vm_stack_get(struct sl_vm *vm, int idx) {
    return vm->stack.data[vm->stack_ptr + idx];
}

//
// PROG
//

struct sl_prog *sl_prog_create(const char *prog_text) {
    struct sl_prog *prog = sl_alloc(sizeof(struct sl_prog));
    prog->parse_idx = 0;
    prog->text = prog_text;
    prog->error = NULL;
    array_init(&prog->expr_list);

    struct array_sl_token tokens_array;
    array_init(&tokens_array);
    sl_tokenize_program(prog_text, &tokens_array);
    sl_tokens_print(tokens_array.data);

    struct array_sl_opcode ops;
    array_init(&ops);

    struct sl_env env;
    sl_env_init(&env, NULL);

    struct sl_token *tokens = tokens_array.data;
    while (!sl_token_is_eof(*tokens)) {
        struct sl_expr *expr = sl_expr_parse(&tokens);
        sl_expr_print(expr);
        m_logf("\n");
        if (expr->type == SL_EXPR_ERROR) {
            prog->error = expr;
            break;
        }

        sl_expr_compile(expr, &ops, &env);
        array_push(&ops, sl_opcode_pop());

        array_push(&prog->expr_list, expr);
    }

    sl_opcodes_print(&ops);

    return prog;
}

void sl_prog_delete(struct sl_prog *prog) {
    for (int i = 0; i < prog->expr_list.length; i++) {
        sl_expr_delete(prog->expr_list.data[i]);
    }
    if (prog->error) {
        sl_expr_delete(prog->error);
    }
    array_deinit(&prog->expr_list);
    sl_free(prog);

    m_logf("%d\n", num_allocs);
}

void sl_prog_run(struct sl_prog *prog) {
    //for (int i = 0; i < prog->expr_list.length; i++) {
        //struct sl_val *val = sl_expr_eval(prog->expr_list.data[i], prog->env);
        //sl_val_print(val);
        //printf("\n");
    //}
}
