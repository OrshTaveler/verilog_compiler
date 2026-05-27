#include "parser.h"
#include <stdlib.h>



int parseExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);

static int getTokenPriority(token_type type)
{
    switch (type) {
        case OR:
            return 5;

        case AND:
            return 6;

        case BIT_OR:
            return 7;

        case BIT_XOR:
            return 8;

        case BIT_AND:
            return 9;

        case EQEQ:
        case NEQ:
            return 10;

        case PLUS:
        case MINUS:
            return 20;

        case STAR:
        case SLASH:
            return 40;

        default:
            return -1;
    }
}


static NodeType getASTType(token_type type)
{
    switch (type) {
        case OR:
            return AST_LOGIC_OR;

        case AND:
            return AST_LOGIC_AND;

        case BIT_OR:
            return AST_BIT_OR;

        case BIT_XOR:
            return AST_BIT_XOR;

        case BIT_AND:
            return AST_BIT_AND;

        case EQEQ:
            return AST_EQ;
        case NEQ:
            return AST_NE;

        case PLUS:
            return AST_ADD;
        case MINUS:
            return AST_SUB;

        case STAR:
            return AST_MUL;
        case SLASH:
            return AST_DIV;

        default:
            return -1;
    }
}


// module ( ID | INPUT | OUTPUT ) ;
int parseModule(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[++(*token_pointer)].type != ID) return 0;

    ASTModule* module = malloc(sizeof(ASTModule));
    if (!module) return 0;

    ast_module_init(module, tokens[*token_pointer].value);

    if (tokens[++(*token_pointer)].type != LPAREN) {
        free(module);
        return 0;
    }

    while (tokens[++(*token_pointer)].type == ID || 
           tokens[++(*token_pointer)].type == INPUT || 
           tokens[++(*token_pointer)].type == OUTPUT 
    ) {
        if (tokens[(*token_pointer)].type == ID){      
            add_to_scope(&module->scope, &module->ports_count, tokens[(*token_pointer)].value);
        }
    }

    if (tokens[(*token_pointer)++].type != RPAREN) {
        free(module);
        return 0;
    }

    if (tokens[(*token_pointer)++].type != SEMICOLON) {
        free(module);
        return 0;
    }

    struct ASTNode* node = malloc(sizeof(struct ASTNode));
    if (!node) {
        free(module);
        return 0;
    }

    ast_node_init(node, AST_MODULE, module, module->scope_count);

    add_node(root,node);

    free(node);

    return 1;
}

int parseNumeric(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[(*token_pointer)].type == NUMBER) {
        struct ASTNode* node = malloc(sizeof(struct ASTNode));
        if (!node) return 0;

        ASTConstant* constant = malloc(sizeof(ASTConstant));
        if (!constant) {
            free(node);
            return 0;
        }

        ast_const_init(constant,(int)strtol(tokens[(*token_pointer)++].value, NULL, 10));

        ast_node_init(node, AST_CONSTANT, constant, 0);
       
        add_node(root,node);

        free(node);

        return 1;
    }

    return 0;
}


int parseID(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[(*token_pointer)].type == ID) {
        struct ASTNode* node = malloc(sizeof(struct ASTNode));
        if (!node) return 0;

        ASTIdentifier* id = malloc(sizeof(ASTIdentifier));
        if (!id) {
            free(node);
            return 0;
        }

        ast_id_init(id, tokens[(*token_pointer)++].value);

        ast_node_init(node, AST_IDENTIFIER, id, 0);
       
        add_node(root,node);

        free(node);

        return 1;
    }

    return 0;
}


int parseBinExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    
}


int parseParenExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[(*token_pointer)++].type != LPAREN) return 0;

    if (!parseExpression(tokens, token_pointer, token_len, root)) return 0;

    if (tokens[(*token_pointer)++].type != RPAREN) return 0;

    return 1;
}

int parsePrimary(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    switch (tokens[*token_pointer].type)
    {
    case NUMBER:
        return parseNumeric(tokens,token_pointer,token_len,root);
        break;
    case ID:
        return parseID(tokens,token_pointer,token_len,root);
        break;
    case LPAREN:
        return parseParenExpression(tokens,token_pointer,token_len,root);
        break;
    default:
        break;
    }
}

static int parseBinRHS(
    Token* tokens,
    size_t* token_pointer,
    unsigned int token_len,
    struct ASTNode* root,
    int minPriority,
    struct ASTNode* lhs
){
    while (1)
    {
        int prec = getTokenPriority(tokens[*token_pointer].type);

        if (prec < minPriority){
            return 1;
        }

        struct ASTNode* op = malloc(sizeof(struct ASTNode));
        if (!op) return 0;

        ast_node_init(
            op,
            getASTType(tokens[*token_pointer].type),
            NULL,
            2
        );

        (*token_pointer)++;

        struct ASTNode* rhs = malloc(sizeof(struct ASTNode));
        if (!rhs) {
            free(op);
            return 0;
        }

        if (!parsePrimary(tokens, token_pointer, token_len, rhs)) {
            free(rhs);
            free(op);
            return 0;
        }

        int precNext = getTokenPriority(tokens[*token_pointer].type);

        if (precNext > prec){
            if (!parseBinRHS(tokens, token_pointer, token_len, root, prec + 1, rhs)) {
                ast_free(rhs);
                free(rhs);
                free(op);
                return 0;
            }
        }

        add_node(op, lhs);
        add_node(op, rhs);

        *lhs = *op;

        free(rhs);
        free(op);
    }

    return 0;
}


int parseExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    struct ASTNode* lhs = malloc(sizeof(struct ASTNode));
    if (!lhs) return 0;

    if (!parsePrimary(tokens, token_pointer, token_len, lhs)) {
        free(lhs);
        return 0;
    }

    if (!parseBinRHS(tokens, token_pointer, token_len,root, 0, lhs)) {
        ast_free(lhs);
        free(lhs);
        return 0;
    }

    add_node(root, lhs);

    free(lhs);

    return 1;
}