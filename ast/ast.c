#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "stdio.h"

static unsigned int scope_next_id = 1;

static void ast_free_typed_node(struct ASTNode* node)
{
    if (node == NULL || node->typedNode == NULL)
        return;

    switch (node->nodeType) {
        case AST_MODULE: {
            ASTModule* module = (ASTModule*)node->typedNode;

            free(module->ports);
            free(module->scope);
            free(module);

            break;
        }

        case AST_CONSTANT: {
            ASTConstant* constant = (ASTConstant*)node->typedNode;
            free(constant);

            break;
        }

        case AST_IDENTIFIER: {
            ASTIdentifier* identifier = (ASTIdentifier*)node->typedNode;
            free(identifier);

            break;
        }

        case AST_WIRE: {
            ASTWire* wire = (ASTWire*)node->typedNode;
            free(wire);

            break;
        }

        case AST_DESIGN:
        case AST_NONE:
        default:
            free(node->typedNode);
            break;
    }

    node->typedNode = NULL;
}

int ast_node_init(
    struct ASTNode *root,
    NodeType nodeType,
    void *typedNode,
    unsigned int children_count
)
{
    if (root == NULL)
        return 0;

    root->nodeType = nodeType;
    root->typedNode = typedNode;

    root->children_count = 0;
    root->children = NULL;

    root->str_number = 0;
    root->position = 0;

    return 1;
}

int add_node(struct ASTNode* root, struct ASTNode* node)
{
    struct ASTNode* new_children;

    if (root == NULL || node == NULL)
        return 0;

    new_children = realloc(
        root->children,
        sizeof(struct ASTNode) * (root->children_count + 1)
    );

    if (new_children == NULL)
        return 0;

    root->children = new_children;

    root->children[root->children_count] = *node;
    root->children_count++;

    return 1;
}

int ast_free(struct ASTNode* root)
{
    unsigned int i;

    if (root == NULL)
        return 0;

    for (i = 0; i < root->children_count; i++)
        ast_free(&root->children[i]);

    free(root->children);
    root->children = NULL;
    root->children_count = 0;

    ast_free_typed_node(root);

    root->nodeType = AST_NONE;
    root->str_number = 0;
    root->position = 0;

    return 1;
}

int ast_module_init(ASTModule* module, char* id)
{
    if (module == NULL || id == NULL)
        return 0;

    strlcpy(module->id, id, ID_LEN);

    module->ports = NULL;
    module->ports_count = 0;

    module->scope = NULL;
    module->scope_count = 0;

    return 1;
}

int ast_const_init(ASTConstant* constant, int value)
{
    if (constant == NULL)
        return 0;

    constant->value = value;

    return 1;
}

int ast_id_init(ASTIdentifier* id, char* name) {
    strlcpy(id->id, name, ID_LEN);
}


int add_to_scope(ScopeTable** scope, unsigned int* scope_count, char* name)
{
    ScopeTable* new_scope;

    if (scope == NULL || scope_count == NULL || name == NULL)
        return 0;

    new_scope = realloc(
        *scope,
        sizeof(ScopeTable) * (*scope_count + 1)
    );

    if (new_scope == NULL)
        return 0;

    *scope = new_scope;

    strlcpy((*scope)[*scope_count].name, name, ID_LEN);
    (*scope)[*scope_count].id = scope_next_id;

    scope_next_id++;
    (*scope_count)++;

    return 1;
}

static const char* ast_node_type_to_string(NodeType type)
{
    switch (type) {
        case AST_NONE: return "AST_NONE";
        case AST_DESIGN: return "AST_DESIGN";

        case AST_MODULE: return "AST_MODULE";
        case AST_TASK: return "AST_TASK";
        case AST_FUNCTION: return "AST_FUNCTION";

        case AST_WIRE: return "AST_WIRE";
        case AST_MEMORY: return "AST_MEMORY";

        case AST_PARAMETER: return "AST_PARAMETER";
        case AST_LOCALPARAM: return "AST_LOCALPARAM";

        case AST_CONSTANT: return "AST_CONSTANT";
        case AST_IDENTIFIER: return "AST_IDENTIFIER";

        case AST_FCALL: return "AST_FCALL";
        case AST_TCALL: return "AST_TCALL";

        case AST_BIT_NOT: return "AST_BIT_NOT";
        case AST_BIT_AND: return "AST_BIT_AND";
        case AST_BIT_OR: return "AST_BIT_OR";
        case AST_BIT_XOR: return "AST_BIT_XOR";
        case AST_BIT_XNOR: return "AST_BIT_XNOR";

        case AST_REDUCE_AND: return "AST_REDUCE_AND";
        case AST_REDUCE_OR: return "AST_REDUCE_OR";
        case AST_REDUCE_XOR: return "AST_REDUCE_XOR";
        case AST_REDUCE_XNOR: return "AST_REDUCE_XNOR";

        case AST_SHIFT_LEFT: return "AST_SHIFT_LEFT";
        case AST_SHIFT_RIGHT: return "AST_SHIFT_RIGHT";
        case AST_SHIFT_SLEFT: return "AST_SHIFT_SLEFT";
        case AST_SHIFT_SRIGHT: return "AST_SHIFT_SRIGHT";

        case AST_LT: return "AST_LT";
        case AST_LE: return "AST_LE";
        case AST_EQ: return "AST_EQ";
        case AST_NE: return "AST_NE";
        case AST_GE: return "AST_GE";
        case AST_GT: return "AST_GT";

        case AST_ADD: return "AST_ADD";
        case AST_SUB: return "AST_SUB";
        case AST_MUL: return "AST_MUL";
        case AST_DIV: return "AST_DIV";
        case AST_MOD: return "AST_MOD";
        case AST_POW: return "AST_POW";

        case AST_POS: return "AST_POS";
        case AST_NEG: return "AST_NEG";

        case AST_LOGIC_AND: return "AST_LOGIC_AND";
        case AST_LOGIC_OR: return "AST_LOGIC_OR";
        case AST_LOGIC_NOT: return "AST_LOGIC_NOT";

        case AST_ALWAYS: return "AST_ALWAYS";
        case AST_INITIAL: return "AST_INITIAL";

        case AST_ASSIGN: return "AST_ASSIGN";

        case AST_BLOCK: return "AST_BLOCK";

        case AST_ASSIGN_EQ: return "AST_ASSIGN_EQ";
        case AST_ASSIGN_LE: return "AST_ASSIGN_LE";

        case AST_CASE: return "AST_CASE";
        case AST_COND: return "AST_COND";
        case AST_DEFAULT: return "AST_DEFAULT";

        case AST_FOR: return "AST_FOR";

        case AST_POSEDGE: return "AST_POSEDGE";
        case AST_NEGEDGE: return "AST_NEGEDGE";
        case AST_EDGE: return "AST_EDGE";

        default: return "AST_UNKNOWN";
    }
}

static void ast_print_indent(unsigned int depth)
{
    for (unsigned int i = 0; i < depth; i++) {
        printf("│   ");
    }
}

static void ast_print_typed_node(struct ASTNode* node)
{
    if (node == NULL)
        return;

    switch (node->nodeType) {
        case AST_MODULE: {
            ASTModule* module = (ASTModule*)node->typedNode;

            if (module != NULL) {
                printf(" id=\"%s\"", module->id);
                printf(" ports_count=%u", module->ports_count);
                printf(" scope_count=%u", module->scope_count);
            }

            break;
        }

        case AST_CONSTANT: {
            ASTConstant* constant = (ASTConstant*)node->typedNode;

            if (constant != NULL) {
                printf(" value=%d", constant->value);
            }

            break;
        }

        case AST_IDENTIFIER: {
            ASTIdentifier* identifier = (ASTIdentifier*)node->typedNode;

            if (identifier != NULL) {
                printf(" id=\"%s\"", identifier->id);
            }

            break;
        }

        case AST_WIRE: {
            ASTWire* wire = (ASTWire*)node->typedNode;

            if (wire != NULL) {
                printf(" id=\"%s\"", wire->id);
                printf(" width=%d", wire->width);
                printf(" value=%d", wire->value);

                switch (wire->direction) {
                    case DIR_INPUT:
                        printf(" direction=INPUT");
                        break;
                    case DIR_OUTPUT:
                        printf(" direction=OUTPUT");
                        break;
                    case DIR_INOUT:
                        printf(" direction=INOUT");
                        break;
                }

                switch (wire->type) {
                    case WIRE_TYPE:
                        printf(" type=WIRE");
                        break;
                    case INTEGER_TYPE:
                        printf(" type=INTEGER");
                        break;
                    case REG_TYPE:
                        printf(" type=REG");
                        break;
                }
            }

            break;
        }

        default:
            break;
    }
}

static void ast_print_rec(struct ASTNode* node, unsigned int depth, int is_last)
{
    if (node == NULL)
        return;

    for (unsigned int i = 0; i < depth; i++) {
        printf("%s", "│   ");
    }

    if (depth > 0) {
        printf("%s", is_last ? "└── " : "├── ");
    }

    printf("%s", ast_node_type_to_string(node->nodeType));

    ast_print_typed_node(node);

    printf(" children=%u", node->children_count);

    printf("\n");

    for (unsigned int i = 0; i < node->children_count; i++) {
        ast_print_rec(
            &node->children[i],
            depth + 1,
            i + 1 == node->children_count
        );
    }
}

void ast_print(struct ASTNode* root)
{
    if (root == NULL) {
        printf("(null AST)\n");
        return;
    }

    ast_print_rec(root, 0, 1);
}