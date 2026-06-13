#ifndef DETECTER_H
#define DETECTER_H

#include "../ast/ast.h"

int detect_width_mismatches(struct ASTNode* root);
int detect_undeclared_tasks(struct ASTNode* root);
int detect_undeclared_variables(struct ASTNode* root);
int detect_unused_variables(struct ASTNode* root);
int run_ast_detectors(struct ASTNode* root);

#endif
