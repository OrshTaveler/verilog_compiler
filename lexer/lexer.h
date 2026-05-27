#include "../general/general.h"

#define MAX_WORD_LEN 100
#define TOKEN_STD_LEN 128

typedef enum 
{
    // literals
    NUMBER,
    ID,
    STRING,

    // keywords
    MODULE,
    ENDMODULE,
    WIRE,
    REG,
    ASSIGN,
    ALWAYS,
    BEGIN,
    END,
    IF,
    ELSE,
    CASE,
    ENDCASE,
    INITIAL,
    INTEGER,
    WHILE,

    INPUT,
    OUTPUT,

    // operators
    PLUS, MINUS, STAR, SLASH,
    EQ,        // =
    LT, LTEQ,      // <= (non-blocking assignment)
    BT, BTEQ,
    EQEQ, NEQ,
    AND, OR,
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT,
    SHARP, //#

    // punctuation
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    SEMICOLON,
    COMMA,
    DOT,
    COLON,

    NONE

} token_type;

typedef struct 
{   
    int str_number;
    int position;
    char* value;
    token_type type;
} Token;

typedef struct
{
   char word[128];
   token_type key;
} Lexeme;

Token* tokenize(const char* input, unsigned int* tokens_len, unsigned int len);
const char* token_to_string(Token token);
