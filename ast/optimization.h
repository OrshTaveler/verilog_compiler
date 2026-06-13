#ifndef OPTIMIZATION_H
#define OPTIMIZATION_H

#include "ast.h"

int optimize_ast_constants(struct ASTNode* root);
int optimize_ast_unreachable_code(struct ASTNode* root);
int optimize_ast(struct ASTNode* root);

#endif
