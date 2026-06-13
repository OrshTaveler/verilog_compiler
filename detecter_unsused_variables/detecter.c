#include "detecter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[ID_LEN];
    int width;
    int used;
    int str_number;
    int position;
} Symbol;

typedef struct {
    Symbol* symbols;
    unsigned int count;
    unsigned int errors;
    unsigned int warnings;
} DetectorContext;

static const char* node_name(NodeType type)
{
    switch (type) {
        case AST_MODULE: return "module";
        case AST_TASK: return "task";
        default: return "scope";
    }
}

static int constant_width(int value)
{
    unsigned int bits = 1;
    unsigned int shifted = value < 0 ? (unsigned int)(-value) : (unsigned int)value;

    while (shifted > 1) {
        shifted >>= 1;
        bits++;
    }

    return (int)bits;
}

static ASTIdentifier* identifier_from_node(struct ASTNode* node)
{
    if (node == NULL || node->nodeType != AST_IDENTIFIER)
        return NULL;

    return (ASTIdentifier*)node->typedNode;
}

static const char* declaration_name(struct ASTNode* node)
{
    ASTWire* wire;
    ASTIdentifier* id;

    if (node == NULL || node->nodeType != AST_WIRE)
        return NULL;

    wire = (ASTWire*)node->typedNode;
    if (wire != NULL && wire->id[0] != '\0')
        return wire->id;

    if (node->children_count > 0) {
        id = identifier_from_node(&node->children[0]);
        if (id != NULL)
            return id->id;
    }

    return NULL;
}

static int declaration_width(struct ASTNode* node)
{
    ASTWire* wire;

    if (node == NULL || node->nodeType != AST_WIRE)
        return 1;

    wire = (ASTWire*)node->typedNode;
    if (wire != NULL && wire->width > 0)
        return wire->width;

    return 1;
}

static Symbol* find_symbol(DetectorContext* ctx, const char* name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (unsigned int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->symbols[i].name, name) == 0)
            return &ctx->symbols[i];
    }

    return NULL;
}

static void print_detector_error(struct ASTNode* node, const char* message)
{
    if (node != NULL && node->str_number > 0) {
        printf(
            "Detector error at line %d, position %d: %s\n",
            node->str_number,
            node->position,
            message
        );
    } else {
        printf("Detector error: %s\n", message);
    }
}

static int add_symbol(DetectorContext* ctx, const char* name, int width, int str_number, int position)
{
    Symbol* new_symbols;

    if (ctx == NULL || name == NULL || name[0] == '\0')
        return 0;

    if (find_symbol(ctx, name) != NULL)
        return 1;

    new_symbols = realloc(ctx->symbols, sizeof(Symbol) * (ctx->count + 1));
    if (new_symbols == NULL)
        return 0;

    ctx->symbols = new_symbols;
    strlcpy(ctx->symbols[ctx->count].name, name, ID_LEN);
    ctx->symbols[ctx->count].width = width;
    ctx->symbols[ctx->count].used = 0;
    ctx->symbols[ctx->count].str_number = str_number;
    ctx->symbols[ctx->count].position = position;
    ctx->count++;

    return 1;
}

static void collect_declarations(struct ASTNode* node, DetectorContext* ctx)
{
    const char* name;

    if (node == NULL)
        return;

    if (node->nodeType == AST_WIRE) {
        name = declaration_name(node);
        if (!add_symbol(ctx, name, declaration_width(node), node->str_number, node->position)) {
            printf("Detector error: failed to register variable \"%s\"\n", name != NULL ? name : "");
            ctx->errors++;
        }
        return;
    }

    for (unsigned int i = 0; i < node->children_count; i++)
        collect_declarations(&node->children[i], ctx);
}

static void collect_module_ports(struct ASTNode* node, DetectorContext* ctx)
{
    ASTModule* module;

    if (node == NULL || node->nodeType != AST_MODULE)
        return;

    module = (ASTModule*)node->typedNode;
    if (module == NULL)
        return;

    for (unsigned int i = 0; i < module->ports_count; i++) {
        if (!add_symbol(ctx, module->scope[i].name, 1, node->str_number, node->position)) {
            printf("Detector error: failed to register port \"%s\"\n", module->scope[i].name);
            ctx->errors++;
        }
    }
}

static const char* task_name(struct ASTNode* node)
{
    ASTIdentifier* id;

    if (node == NULL || node->nodeType != AST_TASK || node->children_count == 0)
        return NULL;

    id = identifier_from_node(&node->children[0]);
    return id != NULL ? id->id : NULL;
}

static void collect_tasks(struct ASTNode* node, DetectorContext* ctx)
{
    const char* name;

    if (node == NULL)
        return;

    if (node->nodeType == AST_TASK) {
        name = task_name(node);
        if (!add_symbol(ctx, name, 0, node->str_number, node->position)) {
            printf("Detector error: failed to register task \"%s\"\n", name != NULL ? name : "");
            ctx->errors++;
        }
    }

    for (unsigned int i = 0; i < node->children_count; i++)
        collect_tasks(&node->children[i], ctx);
}

static int is_system_task(const char* name)
{
    return name != NULL && strcmp(name, "$display") == 0;
}

static void check_undeclared_tasks_rec(struct ASTNode* node, DetectorContext* ctx)
{
    ASTIdentifier* id;

    if (node == NULL)
        return;

    if (node->nodeType == AST_TCALL && node->children_count > 0) {
        id = identifier_from_node(&node->children[0]);
        if (id != NULL && !is_system_task(id->id) && find_symbol(ctx, id->id) == NULL) {
            char message[ID_LEN + 64];
            snprintf(message, sizeof(message), "undeclared task \"%s\"", id->id);
            print_detector_error(&node->children[0], message);
            ctx->errors++;
        }
    }

    for (unsigned int i = 0; i < node->children_count; i++)
        check_undeclared_tasks_rec(&node->children[i], ctx);
}

static int expression_width(struct ASTNode* node, DetectorContext* ctx);

static int expression_is_constant_only(struct ASTNode* node)
{
    if (node == NULL)
        return 0;

    if (node->nodeType == AST_STRING)
        return 1;

    if (node->nodeType == AST_CONSTANT)
        return 1;

    if (node->nodeType == AST_IDENTIFIER || node->nodeType == AST_WIRE)
        return 0;

    if (node->children_count == 0)
        return 0;

    for (unsigned int i = 0; i < node->children_count; i++) {
        if (!expression_is_constant_only(&node->children[i]))
            return 0;
    }

    return 1;
}

static void mark_identifier_used(struct ASTNode* node, DetectorContext* ctx)
{
    ASTIdentifier* id;
    Symbol* symbol;

    id = identifier_from_node(node);
    if (id == NULL)
        return;

    symbol = find_symbol(ctx, id->id);
    if (symbol != NULL)
        symbol->used = 1;
}

static int expression_width(struct ASTNode* node, DetectorContext* ctx)
{
    ASTConstant* constant;
    ASTIdentifier* id;
    Symbol* symbol;
    int left_width;
    int right_width;

    if (node == NULL)
        return 1;

    switch (node->nodeType) {
        case AST_CONSTANT:
            constant = (ASTConstant*)node->typedNode;
            if (constant == NULL)
                return 1;
            if (constant->width > 0)
                return constant->width;
            return constant_width(constant->value);

        case AST_STRING:
            return 1;

        case AST_IDENTIFIER:
            id = (ASTIdentifier*)node->typedNode;
            symbol = id != NULL ? find_symbol(ctx, id->id) : NULL;
            if (symbol == NULL)
                return 1;
            symbol->used = 1;
            return symbol->width;

        case AST_WIRE:
            return declaration_width(node);

        case AST_LT:
        case AST_LE:
        case AST_EQEQ:
        case AST_NE:
        case AST_GE:
        case AST_GT:
        case AST_LOGIC_AND:
        case AST_LOGIC_OR:
            for (unsigned int i = 0; i < node->children_count; i++)
                expression_width(&node->children[i], ctx);
            return 1;

        default:
            if (node->children_count == 0)
                return 1;

            left_width = expression_width(&node->children[0], ctx);
            right_width = left_width;
            for (unsigned int i = 1; i < node->children_count; i++) {
                int child_width = expression_width(&node->children[i], ctx);
                if (child_width > right_width)
                    right_width = child_width;
            }

            return right_width;
    }
}

static void check_assignment(struct ASTNode* node, DetectorContext* ctx)
{
    int lhs_width;
    int rhs_width;

    if (node == NULL || node->children_count < 2)
        return;

    lhs_width = expression_width(&node->children[0], ctx);
    rhs_width = expression_width(&node->children[1], ctx);

    if (rhs_width > lhs_width ||
        (lhs_width > rhs_width && !expression_is_constant_only(&node->children[1]))) {
        char message[128];
        snprintf(
            message,
            sizeof(message),
            "cannot assign %d-bit expression to %d-bit target",
            rhs_width,
            lhs_width
        );
        print_detector_error(node, message);
        ctx->errors++;
    }
}

static void check_widths_rec(struct ASTNode* node, DetectorContext* ctx)
{
    if (node == NULL)
        return;

    if (node->nodeType == AST_EQ || node->nodeType == AST_ASSIGN || node->nodeType == AST_ASSIGN_EQ) {
        check_assignment(node, ctx);
        return;
    }

    for (unsigned int i = 0; i < node->children_count; i++)
        check_widths_rec(&node->children[i], ctx);
}

static void check_undeclared_rec(struct ASTNode* node, DetectorContext* ctx, int declaration_context)
{
    int child_is_declaration;

    if (node == NULL)
        return;

    if (node->nodeType == AST_IDENTIFIER && !declaration_context) {
        ASTIdentifier* id = identifier_from_node(node);

        if (id != NULL && find_symbol(ctx, id->id) == NULL) {
            char message[ID_LEN + 64];
            snprintf(message, sizeof(message), "undeclared variable \"%s\"", id->id);
            print_detector_error(node, message);
            ctx->errors++;
        }

        return;
    }

    if (node->nodeType == AST_TCALL && node->children_count > 0) {
        for (unsigned int i = 1; i < node->children_count; i++)
            check_undeclared_rec(&node->children[i], ctx, 0);
        return;
    }

    if (node->nodeType == AST_TASK && node->children_count > 0) {
        for (unsigned int i = 1; i < node->children_count; i++)
            check_undeclared_rec(&node->children[i], ctx, 0);
        return;
    }

    child_is_declaration = node->nodeType == AST_WIRE;
    for (unsigned int i = 0; i < node->children_count; i++)
        check_undeclared_rec(&node->children[i], ctx, child_is_declaration);
}

static void mark_uses_rec(struct ASTNode* node, DetectorContext* ctx, int declaration_context)
{
    int child_is_declaration;

    if (node == NULL)
        return;

    if (node->nodeType == AST_IDENTIFIER && !declaration_context) {
        mark_identifier_used(node, ctx);
        return;
    }

    child_is_declaration = node->nodeType == AST_WIRE;
    for (unsigned int i = 0; i < node->children_count; i++)
        mark_uses_rec(&node->children[i], ctx, child_is_declaration);
}

static void check_unused_symbols(DetectorContext* ctx)
{
    if (ctx == NULL)
        return;

    for (unsigned int i = 0; i < ctx->count; i++) {
        if (!ctx->symbols[i].used) {
            if (ctx->symbols[i].str_number > 0) {
                printf(
                    "Detector error at line %d, position %d: unused variable \"%s\"\n",
                    ctx->symbols[i].str_number,
                    ctx->symbols[i].position,
                    ctx->symbols[i].name
                );
            } else {
                printf("Detector error: unused variable \"%s\"\n", ctx->symbols[i].name);
            }
            ctx->errors++;
        }
    }
}

static void init_context_for_scope(struct ASTNode* scope, DetectorContext* ctx)
{
    ctx->symbols = NULL;
    ctx->count = 0;
    ctx->errors = 0;
    ctx->warnings = 0;

    collect_module_ports(scope, ctx);
    collect_declarations(scope, ctx);
}

static int detect_width_mismatches_in_scope(struct ASTNode* scope)
{
    DetectorContext ctx;
    unsigned int errors;

    init_context_for_scope(scope, &ctx);
    check_widths_rec(scope, &ctx);

    errors = ctx.errors;
    free(ctx.symbols);
    return errors == 0;
}

static int detect_undeclared_variables_in_scope(struct ASTNode* scope)
{
    DetectorContext ctx;
    unsigned int errors;

    init_context_for_scope(scope, &ctx);
    check_undeclared_rec(scope, &ctx, 0);

    errors = ctx.errors;
    free(ctx.symbols);
    return errors == 0;
}

static int detect_unused_variables_in_scope(struct ASTNode* scope)
{
    DetectorContext ctx;
    unsigned int errors;

    init_context_for_scope(scope, &ctx);
    mark_uses_rec(scope, &ctx, 0);
    check_unused_symbols(&ctx);

    errors = ctx.errors;
    free(ctx.symbols);
    return errors == 0;
}

static int detect_undeclared_tasks_in_design(struct ASTNode* root)
{
    DetectorContext ctx;
    unsigned int errors;

    ctx.symbols = NULL;
    ctx.count = 0;
    ctx.errors = 0;
    ctx.warnings = 0;

    collect_tasks(root, &ctx);
    check_undeclared_tasks_rec(root, &ctx);

    errors = ctx.errors;
    free(ctx.symbols);
    return errors == 0;
}

static int walk_scopes(struct ASTNode* node, int (*callback)(struct ASTNode*))
{
    int ok = 1;

    if (node == NULL)
        return 1;

    if (node->nodeType == AST_MODULE || node->nodeType == AST_TASK) {
        printf("Detector: checking %s\n", node_name(node->nodeType));
        if (!callback(node))
            ok = 0;
    }

    for (unsigned int i = 0; i < node->children_count; i++) {
        if (!walk_scopes(&node->children[i], callback))
            ok = 0;
    }

    return ok;
}

int detect_width_mismatches(struct ASTNode* root)
{
    return walk_scopes(root, detect_width_mismatches_in_scope);
}

int detect_undeclared_variables(struct ASTNode* root)
{
    return walk_scopes(root, detect_undeclared_variables_in_scope);
}

int detect_unused_variables(struct ASTNode* root)
{
    return walk_scopes(root, detect_unused_variables_in_scope);
}

int detect_undeclared_tasks(struct ASTNode* root)
{
    printf("Detector: checking tasks\n");
    return detect_undeclared_tasks_in_design(root);
}

int run_ast_detectors(struct ASTNode* root)
{
    int widths_ok;
    int undeclared_ok;
    int tasks_ok;
    int unused_ok;

    printf("\nDetector results\n");
    tasks_ok = detect_undeclared_tasks(root);
    undeclared_ok = detect_undeclared_variables(root);
    widths_ok = detect_width_mismatches(root);
    unused_ok = detect_unused_variables(root);

    if (tasks_ok && undeclared_ok && widths_ok && unused_ok) {
        printf("Detector OK\n");
        return 1;
    }

    return 0;
}
