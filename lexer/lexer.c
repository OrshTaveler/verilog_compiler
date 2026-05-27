#include "stdio.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lexer.h"

#define TABLE_OP_START 17
#define TABLE_START 0

const int MAX_RESERVED_WORD_LEN = 128;
const Lexeme lexemes[] = {
    // keywords
    {"module", MODULE},
    {"endmodule", ENDMODULE},
    {"wire", WIRE},
    {"reg", REG},
    {"assign", ASSIGN},
    {"always", ALWAYS},
    {"begin", BEGIN},
    {"end", END},
    {"if", IF},
    {"else", ELSE},
    {"case", CASE},
    {"endcase", ENDCASE},
    {"initial",INITIAL},
    {"input",INPUT},
    {"output",OUTPUT},
    {"integer",INTEGER},
    {"while",WHILE},

    // operators
    {"+", PLUS},
    {"-", MINUS},
    {"*", STAR},
    {"/", SLASH},

    {"=", EQ},
    {"<", LT},
    {"<=", LTEQ},
    {">", BT},
    {">=", BTEQ},
    {"=>", BTEQ},
    {"==", EQEQ},
    {"!=", NEQ},

    {"&&", AND},
    {"||", OR},

    {"&", BIT_AND},
    {"|", BIT_OR},
    {"^", BIT_XOR},
    {"~", BIT_NOT},
    {"#", SHARP},

    // punctuation
    {"(", LPAREN},
    {")", RPAREN},
    {"{", LBRACE},
    {"}", RBRACE},
    {"[", LBRACKET},
    {"]", RBRACKET},
    {";", SEMICOLON},
    {",", COMMA},
    {".", DOT},
    {":", COLON},
};




int is_string(const char* s, unsigned int* position) {
    unsigned int backup_position = *position;

    if (s[*position] != '"') return 0;
    
    position++;

    while (isalpha(s[*position])) (*position)++;
    
    if (s[*position] != '"'){
        *position = backup_position;
        return -1;
    }
    return 1;
}

int is_identifier_char(char c) {
    return isalnum(c) || c == '_';
}


token_type is_reserved(const char* s, unsigned int* position, unsigned int table_position) {
    unsigned int backup_position = *position;

    int best_type = -1;
    int best_len = 0;

    int count = sizeof(lexemes) / sizeof(lexemes[0]);

    for (int k = table_position; k < count; k++) {
        const char* lex = lexemes[k].word;
        int len = strlen(lex);

        if (strncmp(s + backup_position, lex, len) == 0) {

            if (isalpha(lex[0])) {
                char next = s[backup_position + len];
                if (is_identifier_char(next)) {
                    continue;
                }
            }

            if (len > best_len) {
                best_len = len;
                best_type = lexemes[k].key;
            }
        }
    }

    if (best_type != -1) {
        *position = backup_position + best_len;
        return best_type;
    }

    *position = backup_position;
    return -1;
}


int is_verilog_number(const char* s, unsigned int* position, Token* prev_token) {
    unsigned int backup_position = *position;
    

    if (s[*(position)] == '-'){
        if (prev_token->type == NUMBER || prev_token->type == ID) return -1;

        (*position)++;
        printf("val = %c, digit - %d, prev - %c\n",s[*position] , isdigit(s[*position]), s[*(position)-1]);
        if (!isdigit(s[*position])) return -1;
    }
    
    while (isdigit(s[*position])) (*position)++;


    if (*position == backup_position)
        return -1;

    if (s[*position] != '\'') {
        unsigned int check = *position;

        if (s[check] == '\0' || s[check] == '\n' || s[check] == ' ')
            return 1;

        if (is_reserved(s, &check,TABLE_OP_START) != -1)
            return 1;

        *position = backup_position;
        return -1;
    }

    unsigned int second_part_start = *position;

    while (isdigit(s[*position]) || tolower(s[*position]) == 'x' ||
           tolower(s[*position]) == 'z' ||
           (((*position) > (second_part_start+1)) && s[*position] == '_') ||
           (((*position) == (second_part_start+1)) &&
            (s[*position] == 'b' || s[*position] == 'h' ||
             s[*position] == 'o' || s[*position] == 'd'))) {
        (*position)++;
    }

    return 1;
}


int is_identifier(const char* s, unsigned int* position) {
    unsigned int backup_position = *position;
    int i = *position;

    if (s[i] == '\\') {
        i++;
        if (s[i] == '\0') return 0;

        while (s[i] && !isspace(s[i])) i++;
        return 1;
    }

    if (!(isalpha(s[i]) || s[i] == '_' || s[i] == '$'))
        return 0;

    i++;

    while (isalnum(s[i]) || s[i] == '_' || s[i] == '$')
        i++;

    *position = i;
    return 1;
}


Token* tokenize(const char* input, unsigned int* t_len, unsigned int len){

    unsigned int tokens_len = 0;
    unsigned int lines = 1;
    unsigned int position_on_line = 0;

    Token* tokens = malloc((len) * (sizeof(Token)));

    tokens_len = 0;

    Token prev_token;

    for (unsigned int i = 0; i < len; i++){
        // Counting lines 
        if (input[i] == '\n'){
            lines++;
            position_on_line = 0;
        }
        // Skiping spaces and new lines
        if (input[i] == ' ' || input[i] == '\n') continue;

        if (i < len - 1){
            // Igore comments
            if (strncmp(input + i, "//",2) == 0){
                while (input[i] != '\n') i++;
                lines++;
                continue;
            }

            if (strncmp(input + i, "/*",2) == 0){
                while (strncmp(input + i, "*/",2) != 0){
                    if (input[i] == '\n') lines++;
                    i++;
                }
                i++;
                continue;
            }
        }
        unsigned int position = i;
        unsigned int start = i;

        position_on_line++;

        if (is_string(input, &position) == 1){
            tokens[tokens_len].type = STRING;
            i = position - 1;
        }
        else if (is_verilog_number(input, &position, &prev_token) == 1){ 
            tokens[tokens_len].type = NUMBER;
            i = position - 1;
        }
        else{
            token_type type = is_reserved(input, &position, TABLE_START);
            
            if (type == -1){
                if (is_identifier(input, &position) == 1){ 
                    tokens[tokens_len].type = ID;
                    i = position - 1;
                }
            } else {
                tokens[tokens_len].type = type;
                i = position - 1;
            }
        }

        size_t token_size = position - start;
        tokens[tokens_len].value = malloc(token_size + 1);
        memcpy(tokens[tokens_len].value, input + start, token_size);
        tokens[tokens_len].value[token_size] = '\0';
        
        tokens[tokens_len].position = position_on_line;
        tokens[tokens_len].str_number = lines;

        tokens_len++;

        prev_token = tokens[tokens_len-1];
    }

    *t_len = tokens_len;

    return tokens;
}


const char* token_to_string(Token token){
    for (int k = 0; k < sizeof(lexemes) / sizeof(lexemes[0]); k++){
        if (token.type == lexemes[k].key){
            return lexemes[k].word;
        }
    }
    if (token.type == STRING) return "STRING";
    else if (token.type == NUMBER) return "NUMBER";
    else if (token.type == ID) return "ID";
    
    return "NONE";
}