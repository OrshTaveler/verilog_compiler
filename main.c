#include "stdio.h"
#include "stdlib.h"
#ifndef LEXER_DEF_H
#define LEXER_DEF_H
#endif
#include "ast/parser.h"



int main(){

    unsigned int len = 0;
    unsigned int token_len = 0;
    char* initial_text = read_entire_file("test.vh",&len);
    Token* tokens = tokenize(initial_text,&token_len,len);

    for (int i = 0; i < token_len; i++){
        printf("%s ", token_to_string(tokens[i]));
        if (tokens[i].type == SEMICOLON) printf("\n");
    }
    
    printf("\n");

    struct ASTNode* root = malloc(sizeof(struct ASTNode));
    if (!root) {
        free(tokens);
        free(initial_text);
        return 1;
    }

    ast_node_init(root, AST_DESIGN, NULL, 0);

    size_t token_pointer = 0;

    if (parseExpression(tokens, &token_pointer, token_len, root)) {
        printf("Parse OK\n\n");
        ast_print(root);
    } else {
        printf("Parse error\n");
    }

    ast_free(root);
    free(root);

    free(tokens);
    free(initial_text);
    return 0;
}