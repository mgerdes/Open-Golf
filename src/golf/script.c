#include "golf/script.h"

#include "golf/map.h"
#include "golf/vec.h"

typedef enum gs_constant_type {
    GS_CONSTANT_INT,
    GS_CONSTANT_FLOAT,
    GS_CONSTANT_STRING,
} gs_constant_type;

typedef struct gs_constant {
    gs_constant_type type;
    union {
        int int_val;
        float float_val;
        const char *string_val;
    };  
} gs_constant_t;

typedef enum gs_token_type {
    GS_TOKEN_CONST,
    GS_TOKEN_SYMBOL,
    GS_TOKEN_CHAR,
    GS_TOKEN_EOF,
} gs_token_type;

typedef struct gs_token {
    gs_token_type type;
    union {
        gs_constant_t constant;
        const char *symbol;
        char c;
    };
} gs_token_t;
typedef vec_t(gs_token_t) vec_gs_token_t;

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

static void gs_tokenize(const char *src, vec_gs_token_t *tokens);

static void gs_tokenize(const char *src, vec_gs_token_t *tokens) {
   int line = 1; 
   int col = 1;
   int i = 0;

   /*
   while (true) {
       if (text[i] == ' ' || text[i] == '\t' || text[i] == '\r') {
           col++;
           i++;
       }
       else if (text[i] == '\n') {
           col = 1;
           line++;
           i++;
       }
       else if (text[i] == 0) {
           vec_push(&program->tokens, _ms_token_eof(line, col));
           break;
       }
       else if ((text[i] == '/') && (text[i + 1] == '/')) {
           while (text[i] && (text[i] != '\n')) {
               i++;
           }
       }
       else if (text[i] == '"') {
           i++;
           col++;
           int len = 0;
           vec_push(&program->tokens, _ms_token_string(program, text + i, &len, line, col));
           if (program->error) return;
           i += (len + 1);
           col += (len + 1);
       }
       else if (_ms_is_char_start_of_symbol(text[i])) {
           int len = 0;
           vec_push(&program->tokens, _ms_token_symbol(text + i, &len, line, col));
           i += len;
           col += len;
       }
       else if (_ms_is_char_digit(text[i])) {
           int len = 0;
           vec_push(&program->tokens, _ms_token_number(text + i, &len, line, col));
           i += len;
           col += len;
       }
       else if (_ms_is_char(text[i])) {
           vec_push(&program->tokens, _ms_token_char(text[i], line, col));
           i++;
           col++;
       }
       else {
           _ms_token_t tok = _ms_token_char(text[i], line, col);
           _ms_program_error(program, tok, "Unknown character: %c", text[i]);
           return;
       }
   }
   */
}

void golf_script_init(void) {

}
