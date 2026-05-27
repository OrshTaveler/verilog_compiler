#include "stdio.h"
#include "ast.h"
#include "../lexer/lexer.h"

int parseExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);