
#define ID_LEN 128

// ===== ENUMS =====

typedef enum {
    // It is a signal of Error
    AST_NONE,

    // This node type is used for the top node of the AST tree. It has no corresponding Verilog construct.
    AST_DESIGN,

    AST_MODULE, AST_TASK, AST_FUNCTION,

    // input, output, wire, reg and integer
    AST_WIRE,

    AST_MEMORY,

    AST_PARAMETER, AST_LOCALPARAM,
    // A literal value
    AST_CONSTANT,

    //An Identifier (signal name in expression or cell/task/etc. name in other contexts)
    AST_IDENTIFIER,
    // Call to function or task
    AST_FCALL, AST_TCALL, 

    AST_BIT_NOT, AST_BIT_AND,
    AST_BIT_OR, AST_BIT_XOR, 
    AST_BIT_XNOR,

    AST_REDUCE_AND, AST_REDUCE_OR,
    AST_REDUCE_XOR, AST_REDUCE_XNOR,

    AST_SHIFT_LEFT, AST_SHIFT_RIGHT,
    AST_SHIFT_SLEFT, AST_SHIFT_SRIGHT,

    AST_LT, AST_LE, AST_EQ, AST_NE, AST_GE, AST_GT,
    AST_ADD, AST_SUB, AST_MUL, AST_DIV, AST_MOD, AST_POW,

    AST_POS, AST_NEG,

    AST_LOGIC_AND, AST_LOGIC_OR, AST_LOGIC_NOT,

    AST_ALWAYS, AST_INITIAL,

    AST_ASSIGN,

    AST_BLOCK,

    AST_ASSIGN_EQ, AST_ASSIGN_LE,

    AST_CASE, AST_COND, AST_DEFAULT,

    AST_FOR,

    AST_POSEDGE, AST_NEGEDGE, AST_EDGE
} NodeType;


typedef enum{
    WIRE_TYPE,
    INTEGER_TYPE,
    REG_TYPE
} WireType;


typedef enum{
    DIR_INPUT,
    DIR_OUTPUT,
    DIR_INOUT
} PortDirection;

//  ====== STRUCTS AND TYPES =====

typedef struct 
{   
    // Name in scope MAY be overidden by deeper scope
    char name [ID_LEN];
    // Unique id of identifire it is unique through all scopes
    unsigned int id; 
} ScopeTable;


struct ASTNode
{   
    NodeType nodeType;
    void* typedNode;

    unsigned int children_count;
    struct ASTNode* children;

    int str_number;
    int position;
};


typedef struct 
{
    // Name of a wire
    char id[ID_LEN];

    // Width of a wire
    int width; 

    // Direction
    PortDirection direction;

    // Type
    WireType type;

    int value;
} ASTWire;


typedef struct 
{   
    // Name of a module
    char id[ID_LEN];

     // Inputs and outputs
    unsigned int ports_count;
    ASTWire* ports;

    //Scope
    ScopeTable* scope;
    unsigned int scope_count;
} ASTModule;


typedef struct 
{
   char id[ID_LEN];
} ASTIdentifier;


typedef struct 
{
   int value;
} ASTConstant;


typedef struct 
{
    struct ASTNode LHS;
    struct ASTNode RHS;
} ASTBinExpression;



// ==== METHODS ====

int ast_node_init(struct ASTNode *root, NodeType nodeType, void *typedNode, unsigned int children_count);

int ast_free(struct ASTNode* root);

int add_node(struct ASTNode* root, struct ASTNode* node);

int add_to_scope(ScopeTable** scope, unsigned int* scope_count, char* name);

int ast_module_init(ASTModule* module, char* id);

int ast_const_init(ASTConstant* constant, int value);

int ast_id_init(ASTIdentifier* id, char* name);

void ast_print(struct ASTNode* root);