#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void print_error(Token* token, const char* message)
{
    printf("Error at line %d, position %d: %s\n", token->str_number, token->position, message);
}

static void copy_node_position(struct ASTNode* node, Token* token)
{
    if (node == NULL || token == NULL)
        return;

    node->str_number = token->str_number;
    node->position = token->position;
}

int parseExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseID(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseString(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseScope(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseEq(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseExpressionStatement(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseTask(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);
int parseTaskCall(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root);

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

        case LT:
        case LTEQ:
        case BT:
        case BTEQ:
            return 15;

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
            return AST_EQEQ;
        case NEQ:
            return AST_NE;

        case LT:   return AST_LT;
        case LTEQ: return AST_LE;
        case BT:   return AST_GT;
        case BTEQ: return AST_GE;

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

static NodeType getPrefixASTType(token_type type)
{
    switch (type) {
        case PLUS: return AST_POS;
        case MINUS: return AST_NEG;
        case LOGIC_NOT: return AST_LOGIC_NOT;
        case BIT_NOT: return AST_BIT_NOT;
        case INC: return AST_PRE_INC;
        case DEC: return AST_PRE_DEC;
        default: return AST_NONE;
    }
}

static NodeType getPostfixASTType(token_type type)
{
    switch (type) {
        case INC: return AST_POST_INC;
        case DEC: return AST_POST_DEC;
        default: return AST_NONE;
    }
}

int parseDeclaration(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    Token* start_token = &tokens[*token_pointer];
    ASTWire* wire = malloc(sizeof(ASTWire));
    if (!wire) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }

    struct ASTNode* node = malloc(sizeof(struct ASTNode));
    if (!node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(wire);
        return 0;
    }

    char id[ID_LEN];
    long width; 
    PortDirection direction = DIR_NONE;
    WireType type;

    if (tokens[*token_pointer].type == INPUT) {
        direction = DIR_INPUT;
        (*token_pointer)++;
    } else if (tokens[*token_pointer].type == OUTPUT) {
        direction = DIR_OUTPUT;
        (*token_pointer)++;
    }

    if (tokens[*token_pointer].type == WIRE || 
        tokens[*token_pointer].type == REG 
    ) {
        if (tokens[*token_pointer].type == WIRE) type = WIRE_TYPE;
        if (tokens[*token_pointer].type == REG) type = REG_TYPE;

        (*token_pointer)++;

        if (tokens[*token_pointer].type == LBRACKET) {
            long left_bound;
            long right_bound;

            if (tokens[++(*token_pointer)].type != NUMBER) {
                print_error(&tokens[*token_pointer], "expected number after bracket");
                return 0;
            }
            left_bound = tokens[*token_pointer].num_val;

            if (tokens[++(*token_pointer)].type == COLON) {
                if (tokens[++(*token_pointer)].type != NUMBER) {
                    print_error(&tokens[*token_pointer], "expected number after colon");
                    return 0;
                }
                right_bound = tokens[*token_pointer].num_val;
                if (tokens[++(*token_pointer)].type != RBRACKET)  {
                    print_error(&tokens[*token_pointer], "expected closing bracket");
                    return 0;
                }

                width = left_bound > right_bound
                    ? left_bound - right_bound + 1
                    : right_bound - left_bound + 1;
                (*token_pointer)++;
            } else if (tokens[*token_pointer].type == RBRACKET) 
            {
                width = left_bound;
                (*token_pointer)++;
            }
            else {
                print_error(&tokens[*token_pointer], "expected colon or closing bracket");
                free(wire);
                return 0;
            }
        } else if (tokens[*token_pointer].type == ID) {
            width = 1;
        } else {
            print_error(&tokens[*token_pointer], "expected ID or bracket");
            free(wire);
            return 0;
        } 
    } else if (tokens[*token_pointer].type == INTEGER) {
        width = 32;
        type = INTEGER_TYPE;

        (*token_pointer)++;
    } else {
        free(wire);
        return 0;
    }
    
    ast_wire_init(wire, id, width, direction, type);
    ast_node_init(node,AST_WIRE,wire,0);
    copy_node_position(node, start_token);

    if (!parseID(tokens,token_pointer,token_len,node)){
        print_error(&tokens[*token_pointer], "expected ID");
        return 0;
    }

    if (node->children_count > 0 && node->children[0].nodeType == AST_IDENTIFIER) {
        ASTIdentifier* parsed_id = (ASTIdentifier*)node->children[0].typedNode;
        if (parsed_id != NULL)
            strlcpy(wire->id, parsed_id->id, ID_LEN);
    }
    
    if (tokens[(*token_pointer)].type == SEMICOLON) { 
        (*token_pointer)++;

        add_node(root,node);
        free(node);
        return 1;
    } else if (tokens[(*token_pointer)].type == EQ) {
        struct ASTNode* eq_node = malloc(sizeof(struct ASTNode));
        if (!eq_node) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            return 0;
        }

        ast_node_init(eq_node, AST_EQ, NULL, 0);
        copy_node_position(eq_node, start_token);

        add_node(eq_node, node);

        (*token_pointer)++;
        
        if (!parseExpression(tokens, token_pointer, token_len, eq_node)){
            print_error(&tokens[*token_pointer], "failed to parse expression");
            free(node);
            free(eq_node);
            free(wire);
            return 0;
        }
    
        if (tokens[(*token_pointer)].type == SEMICOLON) { 
            (*token_pointer)++;
            
            add_node(root,eq_node);
            free(eq_node);
            free(node);

            return 1;
        }
        
        free(eq_node);
        free(node);
        free(wire);
        print_error(&tokens[*token_pointer], "expected ';'");
        return 0;
    } else { 
        print_error(&tokens[*token_pointer], "expected semicolon or equals");
        free(node);
        free(wire);
        return 0;
    }
} 

int parseAssign(Token* tokens,
                size_t* token_pointer,
                unsigned int token_len,
                struct ASTNode* root)
{
    if (tokens[*token_pointer].type != ASSIGN)
        return 0;

    Token* start_token = &tokens[*token_pointer];
    struct ASTNode* assignNode = malloc(sizeof(struct ASTNode));
    if (!assignNode) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }

    ast_node_init(assignNode, AST_ASSIGN, NULL, 0);
    copy_node_position(assignNode, start_token);

    (*token_pointer)++;


    if (!parseID(tokens, token_pointer, token_len, assignNode)) {
        print_error(&tokens[*token_pointer], "expected identifier");
        free(assignNode);
        return 0;
    }


    if (tokens[*token_pointer].type != EQ) {
        print_error(&tokens[*token_pointer], "expected '='");
        free(assignNode);
        return 0;
    }

    (*token_pointer)++;

    if (!parseExpression(tokens,
                         token_pointer,
                         token_len,
                         assignNode))
    {
        print_error(&tokens[*token_pointer], "failed to parse assignment expression");
        free(assignNode);
        return 0;
    }

    if (tokens[*token_pointer].type != SEMICOLON) {
        print_error(&tokens[*token_pointer], "expected ';'");
        free(assignNode);
        return 0;
    }

    (*token_pointer)++;

    add_node(root, assignNode);

    free(assignNode);

    return 1;
}

int parseIf(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    Token* start_token = &tokens[*token_pointer];
    if (tokens[(*token_pointer)++].type != IF)
        return 0;
    
    if (tokens[(*token_pointer)].type != LPAREN) {
        print_error(&tokens[*token_pointer], "expected '(' after if");
        return 0;
    }

    struct ASTNode* if_node = malloc(sizeof(struct ASTNode));
    if (!if_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(if_node);
        return 0;
    } 

    struct ASTNode* cond_node = malloc(sizeof(struct ASTNode));
    if (!cond_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(cond_node);
        free(if_node);
        return 0;
    } 

    ast_node_init(if_node, AST_IF, NULL, 0);
    ast_node_init(cond_node, AST_COND, NULL, 0);
    copy_node_position(if_node, start_token);
    copy_node_position(cond_node, start_token);


    if (!parseExpression(tokens,token_pointer,token_len, cond_node)){
        print_error(&tokens[*token_pointer], "failed to parse if condition");
        free(cond_node);
        free(if_node); 
        return 0;
    }

    if (tokens[(*token_pointer)++].type != BEGIN){ 
        print_error(&tokens[(*token_pointer) - 1], "expected begin after if condition");
        free(cond_node);
        free(if_node);
        return 0;
    }

    struct ASTNode* block_node = malloc(sizeof(struct ASTNode));
    if (!block_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(block_node);
        free(cond_node);
        free(if_node);
        return 0;
    } 

    ast_node_init(block_node, AST_BLOCK, NULL, 0);
    copy_node_position(block_node, start_token);

    while (*token_pointer < token_len && tokens[*(token_pointer)].type != END)
    {   
       if (!parseScope(tokens, token_pointer, token_len, block_node)){
        print_error(&tokens[*token_pointer], "failed to parse scope");
        free(block_node);
        free(cond_node);
        free(if_node);
        return 0;
       }
    }

    if (tokens[(*token_pointer)++].type != END){ 
        print_error(&tokens[(*token_pointer) - 1], "expected end");
        free(block_node);
        free(cond_node);
        free(if_node);
        return 0;
    }

    add_node(if_node,cond_node);
    add_node(if_node, block_node);
    add_node(root,if_node);

    free(block_node);
    free(cond_node);
    free(if_node);
    return 1;
}

int parseFor(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    Token* start_token = &tokens[*token_pointer];
    if (tokens[(*token_pointer)++].type != FOR)
        return 0;
    
    if (tokens[(*token_pointer)++].type != LPAREN) {
        print_error(&tokens[(*token_pointer) - 1], "expected '(' after for");
        return 0;
    }
    
    struct ASTNode* for_node = malloc(sizeof(struct ASTNode));
    if (!for_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(for_node);
        return 0;
    } 
    ast_node_init(for_node, AST_FOR, NULL, 0);
    copy_node_position(for_node, start_token);

    size_t cur_pntr = *token_pointer;
    if (parseDeclaration(tokens, &cur_pntr, token_len, for_node)) {
        *token_pointer = cur_pntr;
    } else {
        cur_pntr = *token_pointer;
        if (parseEq(tokens, &cur_pntr, token_len, for_node)) {
            *token_pointer = cur_pntr;
        } else {
            print_error(&tokens[*token_pointer], "failed to parse for initializer");
            free(for_node);
            return 0;
        }
    }

    struct ASTNode* cond_node = malloc(sizeof(struct ASTNode));
    if (!cond_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(cond_node);
        free(for_node);
        return 0;
    } 
    ast_node_init(cond_node, AST_COND, NULL, 0);
    copy_node_position(cond_node, start_token);

    if (!parseExpression(tokens,token_pointer,token_len,cond_node)) {
        print_error(&tokens[*token_pointer], "failed to parse for condition");
        free(cond_node);
        free(for_node);
        return 0;
    }

    if (tokens[(*token_pointer)++].type != SEMICOLON) {
        print_error(&tokens[(*token_pointer) - 1], "expected ';' after for condition");
        free(cond_node);
        free(for_node);
        return 0;
    }

    struct ASTNode* block_node = malloc(sizeof(struct ASTNode));
    if (!block_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(cond_node);
        free(for_node);
        free(block_node);
        return 0;
    } 
    ast_node_init(block_node, AST_BLOCK, NULL, 0);
    copy_node_position(block_node, start_token);

    struct ASTNode* increment_node = malloc(sizeof(struct ASTNode));
    if (!increment_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(cond_node);
        free(for_node);
        free(block_node);
        return 0;
    }
    ast_node_init(increment_node, AST_BLOCK, NULL, 0);
    copy_node_position(increment_node, start_token);

    size_t increment_pointer = *token_pointer;
    if (parseEq(tokens, &increment_pointer, token_len, increment_node)) {
        *token_pointer = increment_pointer;
    } else if (parseExpressionStatement(tokens, token_pointer, token_len, increment_node)) {
        /* parsed below */
    } else {
        print_error(&tokens[*token_pointer], "failed to parse for increment");
        free(cond_node);
        free(for_node);
        free(block_node);
        ast_free(increment_node);
        free(increment_node);
        return 0;
    }

    if (tokens[(*token_pointer)++].type != BEGIN) {
        print_error(&tokens[(*token_pointer) - 1], "expected begin after for increment");
        free(cond_node);
        free(for_node);
        free(block_node);
        ast_free(increment_node);
        free(increment_node);
        return 0;
    }

    while (*token_pointer < token_len && tokens[*(token_pointer)].type != END)
    {   
       if (!parseScope(tokens, token_pointer, token_len, block_node)){
        print_error(&tokens[*token_pointer], "failed to parse scope");
        free(block_node);
        free(cond_node);
        free(for_node);
        ast_free(increment_node);
        free(increment_node);
        return 0;
       }
    }

    if (tokens[(*token_pointer)++].type != END){ 
        print_error(&tokens[(*token_pointer) - 1], "expected end");
        free(block_node);
        free(cond_node);
        free(for_node);
        ast_free(increment_node);
        free(increment_node);
        return 0;
    }

    for (unsigned int i = 0; i < increment_node->children_count; i++) {
        add_node(block_node, &increment_node->children[i]);
    }
    free(increment_node->children);
    increment_node->children = NULL;
    increment_node->children_count = 0;

    add_node(for_node,cond_node);
    add_node(for_node, block_node);
    add_node(root,for_node);

    free(block_node);
    free(cond_node);
    free(for_node);
    free(increment_node);
    return 1;
}

int parseScope(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    size_t cur_pntr = *token_pointer;

    if (parseDeclaration(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    } 

    cur_pntr = *token_pointer;
    if (parseAssign(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    } 

    cur_pntr = *token_pointer;
    if (parseIf(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    } 

    cur_pntr = *token_pointer;
    if (parseTaskCall(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    } 

    cur_pntr = *token_pointer;
  
    if (parseEq(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    } 

    cur_pntr = *token_pointer;
    if (parseFor(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    } 

    cur_pntr = *token_pointer;
    if (parseExpressionStatement(tokens, &cur_pntr, token_len, root)) {
        *token_pointer = cur_pntr;
        return 1;
    }

    return 0;
}

int parseModule(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    Token* start_token = &tokens[*token_pointer];
    if (tokens[++(*token_pointer)].type != ID) {
        print_error(&tokens[*token_pointer], "expected module ID");
        return 0;
    }

    ASTModule* module = malloc(sizeof(ASTModule));
    if (!module) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }

    ast_module_init(module, tokens[*token_pointer].value);

    if (tokens[++(*token_pointer)].type != LPAREN) {
        print_error(&tokens[*token_pointer], "expected '('");
        free(module);
        return 0;
    }

   
    while (tokens[++(*token_pointer)].type == ID || 
           tokens[(*token_pointer)].type == INPUT || 
           tokens[(*token_pointer)].type == OUTPUT || 
           tokens[(*token_pointer)].type == COMMA
    ) {
        if (tokens[(*token_pointer)].type == ID){      
            add_to_scope(&module->scope, &module->ports_count, tokens[(*token_pointer)].value);
        }
    }

    if (tokens[(*token_pointer)++].type != RPAREN) {
        print_error(&tokens[*token_pointer], "expected ')'");
        free(module);
        return 0;
    }

    if (tokens[(*token_pointer)++].type != SEMICOLON) {
        print_error(&tokens[*token_pointer], "expected ';'");
        free(module);
        return 0;
    }

    struct ASTNode* node = malloc(sizeof(struct ASTNode));
    if (!node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(module);
        return 0;
    }

    ast_node_init(node, AST_MODULE, module, module->scope_count);
    copy_node_position(node, start_token);
    
    while (*token_pointer < token_len && tokens[*(token_pointer)].type != ENDMODULE)
    {
       if (!parseScope(tokens, token_pointer, token_len, node)){
        print_error(&tokens[*token_pointer], "failed to parse scope");
        free(module);
        free(node);
        return 0;
       }
    }

    add_node(root,node);

    free(node);

    return 1;
}

int parseTask(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    Token* start_token = &tokens[*token_pointer];
    if (tokens[(*token_pointer)++].type != TASK)
        return 0;

    struct ASTNode* task_node = malloc(sizeof(struct ASTNode));
    if (!task_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }

    ast_node_init(task_node, AST_TASK, NULL, 0);
    copy_node_position(task_node, start_token);

    if (!parseID(tokens, token_pointer, token_len, task_node)) {
        print_error(&tokens[*token_pointer], "expected task ID");
        free(task_node);
        return 0;
    }

    if (tokens[(*token_pointer)++].type != SEMICOLON) {
        print_error(&tokens[*token_pointer], "expected ';'");
        free(task_node);
        return 0;
    }

    while (*token_pointer < token_len && tokens[*(token_pointer)].type != ENDTASK)
    {
       if (!parseScope(tokens, token_pointer, token_len, task_node)){
        print_error(&tokens[*token_pointer], "failed to parse scope");
        free(task_node);
        return 0;
       }
    }

    if (tokens[(*token_pointer)++].type != ENDTASK){
        print_error(&tokens[(*token_pointer) - 1], "expected endtask");
        free(task_node);
        return 0;
    }

    add_node(root,task_node);

    free(task_node);
    return 1;
}

int parseTaskCall(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[*token_pointer].type != ID)
        return 0;

    Token* start_token = &tokens[*token_pointer];
    struct ASTNode* call_node = malloc(sizeof(struct ASTNode));
    if (!call_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }

    ast_node_init(call_node, AST_TCALL, NULL, 0);
    copy_node_position(call_node, start_token);

    if (!parseID(tokens, token_pointer, token_len, call_node)) {
        print_error(&tokens[*token_pointer], "expected task call ID");
        free(call_node);
        return 0;
    }

    if (tokens[*token_pointer].type == LPAREN) {
        (*token_pointer)++;

        while (*token_pointer < token_len && tokens[*token_pointer].type != RPAREN) {
            if (tokens[*token_pointer].type == STRING) {
                if (!parseString(tokens, token_pointer, token_len, call_node)) {
                    print_error(&tokens[*token_pointer], "failed to parse task call string argument");
                    free(call_node);
                    return 0;
                }
            } else if (!parseExpression(tokens, token_pointer, token_len, call_node)) {
                print_error(&tokens[*token_pointer], "failed to parse task call argument");
                free(call_node);
                return 0;
            }

            if (tokens[*token_pointer].type == COMMA) {
                (*token_pointer)++;
            } else if (tokens[*token_pointer].type != RPAREN) {
                print_error(&tokens[*token_pointer], "expected ',' or ')' in task call");
                free(call_node);
                return 0;
            }
        }

        if (tokens[(*token_pointer)++].type != RPAREN) {
            print_error(&tokens[(*token_pointer) - 1], "expected ')' after task call arguments");
            free(call_node);
            return 0;
        }
    }

    if (tokens[(*token_pointer)++].type != SEMICOLON) {
        if (tokens[(*token_pointer) - 2].type == RPAREN) {
            print_error(&tokens[(*token_pointer) - 1], "expected ';' after task call");
        }
        free(call_node);
        return 0;
    }

    add_node(root,call_node);

    free(call_node);
    return 1;
}

int parseNumeric(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[(*token_pointer)].type == NUMBER) {
        Token* start_token = &tokens[*token_pointer];
        struct ASTNode* node = malloc(sizeof(struct ASTNode));
        if (!node) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            return 0;
        }

        ASTConstant* constant = malloc(sizeof(ASTConstant));
        if (!constant) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            free(node);
            return 0;
        }

        ast_const_init_width(
            constant,
            (int)tokens[*token_pointer].num_val,
            tokens[*token_pointer].width
        );
        (*token_pointer)++;

        ast_node_init(node, AST_CONSTANT, constant, 0);
        copy_node_position(node, start_token);
       
        add_node(root,node);

        free(node);

        return 1;
    }

    return 0;
}

int parseString(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[*token_pointer].type == STRING) {
        Token* start_token = &tokens[*token_pointer];
        struct ASTNode* node = malloc(sizeof(struct ASTNode));
        if (!node) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            return 0;
        }

        ASTString* string = malloc(sizeof(ASTString));
        if (!string) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            free(node);
            return 0;
        }

        ast_string_init(string, tokens[*token_pointer].value);
        (*token_pointer)++;

        ast_node_init(node, AST_STRING, string, 0);
        copy_node_position(node, start_token);

        add_node(root,node);

        free(node);

        return 1;
    }

    return 0;
}



int parseID(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){

    if (tokens[(*token_pointer)].type == ID) {
        Token* start_token = &tokens[*token_pointer];
        
        struct ASTNode* node = malloc(sizeof(struct ASTNode));
        if (!node) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            return 0;
        }

        ASTIdentifier* id = malloc(sizeof(ASTIdentifier));
        if (!id) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            free(node);
            return 0;
        }
    
        ast_id_init(id, tokens[(*token_pointer)++].value);

        ast_node_init(node, AST_IDENTIFIER, id, 0);
        copy_node_position(node, start_token);
       
        add_node(root,node);

        free(node);

        return 1;
    }

    return 0;
}

int parseEq(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    Token* start_token = &tokens[*token_pointer];
    struct ASTNode* eq_node = malloc(sizeof(struct ASTNode));
    if (!eq_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        free(eq_node);
        return 0;
    }
    ast_node_init(eq_node, AST_EQ, NULL, 0);
    copy_node_position(eq_node, start_token);

    if(!parseID(tokens,token_pointer,token_len,eq_node)){
        free(eq_node);
        return 0;
    }
   
    if (tokens[(*token_pointer)++].type == EQ) {
        if (!parseExpression(tokens,token_pointer,token_len,eq_node) ||
            !(tokens[(*token_pointer)++].type == SEMICOLON || tokens[(*token_pointer) - 1].type == RPAREN)) 
        {
            print_error(&tokens[(*token_pointer) - 1], "expected expression followed by ';' or ')'");
            free(eq_node);
            return 0;
        }
        add_node(root,eq_node);

        free(eq_node);
        return 1;
    }
    
    free(eq_node);
    return 0;
}

int parseParenExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    if (tokens[(*token_pointer)++].type != LPAREN) {
        print_error(&tokens[*token_pointer], "expected '('");
        return 0;
    }

    if (!parseExpression(tokens, token_pointer, token_len, root)) {
        print_error(&tokens[*token_pointer], "failed to parse expression in parentheses");
        return 0;
    }

    if (tokens[(*token_pointer)++].type != RPAREN) {
        print_error(&tokens[*token_pointer], "expected ')'");
        return 0;
    }

    return 1;
}

static int parseBasePrimary(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
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
    return 0;
}

int parsePrimary(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    NodeType prefix_type = getPrefixASTType(tokens[*token_pointer].type);

    if (prefix_type != AST_NONE) {
        Token* start_token = &tokens[*token_pointer];
        struct ASTNode* op = malloc(sizeof(struct ASTNode));
        struct ASTNode* child = malloc(sizeof(struct ASTNode));

        if (!op || !child) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            free(op);
            free(child);
            return 0;
        }

        ast_node_init(op, prefix_type, NULL, 0);
        copy_node_position(op, start_token);
        ast_node_init(child, AST_NONE, NULL, 0);

        (*token_pointer)++;
        if (!parsePrimary(tokens, token_pointer, token_len, child)) {
            print_error(&tokens[*token_pointer], "failed to parse unary expression");
            free(op);
            free(child);
            return 0;
        }

        if (child->nodeType != AST_NONE)
            add_node(op, child);
        else if (child->children_count > 0)
            add_node(op, child->children);

        *root = *op;
        free(op);
        free(child);
        return 1;
    }

    if (!parseBasePrimary(tokens, token_pointer, token_len, root))
        return 0;

    NodeType postfix_type = getPostfixASTType(tokens[*token_pointer].type);
    if (postfix_type != AST_NONE) {
        Token* op_token = &tokens[*token_pointer];
        struct ASTNode* op = malloc(sizeof(struct ASTNode));

        if (!op) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            return 0;
        }

        ast_node_init(op, postfix_type, NULL, 0);
        copy_node_position(op, op_token);

        if (root->nodeType != AST_NONE)
            add_node(op, root);
        else if (root->children_count > 0)
            add_node(op, root->children);

        *root = *op;
        free(op);
        (*token_pointer)++;
    }

    return 1;
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
        if (!op) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            return 0;
        }

        ast_node_init(
            op,
            getASTType(tokens[*token_pointer].type),
            NULL,
            2
        );
        
        (*token_pointer)++;

        struct ASTNode* rhs = malloc(sizeof(struct ASTNode));
        if (!rhs) {
            print_error(&tokens[*token_pointer], "memory allocation failed");
            free(op);
            return 0;
        }
        ast_node_init(rhs, AST_NONE, NULL, 0);

        if (!parsePrimary(tokens, token_pointer, token_len, rhs)) {
            print_error(&tokens[*token_pointer], "failed to parse primary expression");
            free(rhs);
            free(op);
            return 0;
        }

        int precNext = getTokenPriority(tokens[*token_pointer].type);

        if (precNext > prec){
            if (!parseBinRHS(tokens, token_pointer, token_len, root, prec + 1, rhs)) {
                print_error(&tokens[*token_pointer], "failed to parse binary RHS");
                ast_free(rhs);
                free(rhs);
                free(op);
                return 0;
            }
        }
        
        if (lhs->nodeType != AST_NONE)
            add_node(op, lhs);
        else 
            add_node(op, lhs->children);
        
        if (rhs->nodeType != AST_NONE)
            add_node(op, rhs);
        else 
            add_node(op, rhs->children);

        *lhs = *op;

        free(rhs);
        free(op);
    }

    return 0;
}


int parseExpression(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    struct ASTNode* lhs = malloc(sizeof(struct ASTNode));
    if (!lhs) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }
    ast_node_init(lhs, AST_NONE, NULL, 0);

    if (!parsePrimary(tokens, token_pointer, token_len, lhs)) {
        print_error(&tokens[*token_pointer], "failed to parse primary in expression");
        free(lhs);
        return 0;
    }

    if (!parseBinRHS(tokens, token_pointer, token_len,root, 0, lhs)) {
        print_error(&tokens[*token_pointer], "failed to parse binary RHS in expression");
        ast_free(lhs);
        free(lhs);
        return 0;
    }
    if (lhs->nodeType != AST_NONE)
        add_node(root, lhs);
    else 
        add_node(root, lhs->children);

    free(lhs);

    return 1;
}

int parseExpressionStatement(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root)
{
    struct ASTNode* expr_node = malloc(sizeof(struct ASTNode));
    if (!expr_node) {
        print_error(&tokens[*token_pointer], "memory allocation failed");
        return 0;
    }

    ast_node_init(expr_node, AST_BLOCK, NULL, 0);
    copy_node_position(expr_node, &tokens[*token_pointer]);

    if (!parseExpression(tokens, token_pointer, token_len, expr_node)) {
        free(expr_node);
        return 0;
    }

    if (tokens[*token_pointer].type != SEMICOLON && tokens[*token_pointer].type != RPAREN) {
        ast_free(expr_node);
        free(expr_node);
        return 0;
    }

    (*token_pointer)++;

    if (expr_node->children_count > 0)
        add_node(root, expr_node->children);

    free(expr_node->children);
    expr_node->children = NULL;
    expr_node->children_count = 0;
    free(expr_node);

    return 1;
}


int parseDesign(Token* tokens, size_t* token_pointer, unsigned int token_len, struct ASTNode* root){
    while (*token_pointer < token_len) {
        if (tokens[*token_pointer].type == MODULE) {
            if (!parseModule(tokens,token_pointer,token_len,root)) {
                print_error(&tokens[*token_pointer], "failed to parse module");
                return 0;
            }

            if (*token_pointer < token_len && tokens[*token_pointer].type == ENDMODULE)
                (*token_pointer)++;
        } else if (tokens[*token_pointer].type == TASK) {
            if (!parseTask(tokens,token_pointer,token_len,root)) {
                print_error(&tokens[*token_pointer], "failed to parse task");
                return 0;
            }
        } else {
            print_error(&tokens[*token_pointer], "expected module or task");
            return 0;
        }
    }
    return 1;
}
