#include "optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BOOL_OPT_MAX_VARS 4
#define BOOL_OPT_MAX_MINTERMS (1 << BOOL_OPT_MAX_VARS)
#define BOOL_OPT_MAX_IMPLICANTS 64

typedef struct {
    int bits;
    int mask;
    int covered;
    int used;
} Implicant;

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int constant_width_from_value(int value)
{
    unsigned int bits = 1;
    unsigned int shifted = value < 0 ? (unsigned int)(-value) : (unsigned int)value;

    while (shifted > 1) {
        shifted >>= 1;
        bits++;
    }

    return (int)bits;
}

static int node_constant_value(struct ASTNode* node, int* value, int* width)
{
    ASTConstant* constant;

    if (node == NULL || value == NULL || width == NULL || node->nodeType != AST_CONSTANT)
        return 0;

    constant = (ASTConstant*)node->typedNode;
    if (constant == NULL)
        return 0;

    *value = constant->value;
    *width = constant->width > 0
        ? constant->width
        : constant_width_from_value(constant->value);

    return 1;
}

static int fold_unary(NodeType type, int value, int* result)
{
    if (result == NULL)
        return 0;

    switch (type) {
        case AST_POS:
            *result = value;
            return 1;
        case AST_NEG:
            *result = -value;
            return 1;
        case AST_LOGIC_NOT:
            *result = !value;
            return 1;
        case AST_BIT_NOT:
            *result = ~value;
            return 1;
        default:
            return 0;
    }
}

static int fold_binary(NodeType type, int lhs, int rhs, int* result)
{
    if (result == NULL)
        return 0;

    switch (type) {
        case AST_ADD:
            *result = lhs + rhs;
            return 1;
        case AST_SUB:
            *result = lhs - rhs;
            return 1;
        case AST_MUL:
            *result = lhs * rhs;
            return 1;
        case AST_DIV:
            if (rhs == 0)
                return 0;
            *result = lhs / rhs;
            return 1;
        case AST_MOD:
            if (rhs == 0)
                return 0;
            *result = lhs % rhs;
            return 1;
        case AST_BIT_AND:
            *result = lhs & rhs;
            return 1;
        case AST_BIT_OR:
            *result = lhs | rhs;
            return 1;
        case AST_BIT_XOR:
            *result = lhs ^ rhs;
            return 1;
        case AST_LOGIC_AND:
            *result = lhs && rhs;
            return 1;
        case AST_LOGIC_OR:
            *result = lhs || rhs;
            return 1;
        case AST_LT:
            *result = lhs < rhs;
            return 1;
        case AST_LE:
            *result = lhs <= rhs;
            return 1;
        case AST_EQEQ:
            *result = lhs == rhs;
            return 1;
        case AST_NE:
            *result = lhs != rhs;
            return 1;
        case AST_GE:
            *result = lhs >= rhs;
            return 1;
        case AST_GT:
            *result = lhs > rhs;
            return 1;
        default:
            return 0;
    }
}

static int replace_with_constant(struct ASTNode* node, int value, int width)
{
    int str_number;
    int position;
    ASTConstant* constant;

    if (node == NULL)
        return 0;

    str_number = node->str_number;
    position = node->position;

    ast_free(node);

    constant = malloc(sizeof(ASTConstant));
    if (constant == NULL)
        return 0;

    ast_const_init_width(constant, value, width);
    ast_node_init(node, AST_CONSTANT, constant, 0);
    node->str_number = str_number;
    node->position = position;

    return 1;
}

static int is_boolean_operator(NodeType type)
{
    return type == AST_LOGIC_AND ||
           type == AST_LOGIC_OR ||
           type == AST_LOGIC_NOT ||
           type == AST_BIT_AND ||
           type == AST_BIT_OR ||
           type == AST_BIT_NOT;
}

static int is_boolean_leaf(struct ASTNode* node)
{
    return node != NULL &&
           (node->nodeType == AST_IDENTIFIER || node->nodeType == AST_CONSTANT);
}

static int add_boolean_var(char vars[][ID_LEN], int* vars_count, const char* name)
{
    if (vars == NULL || vars_count == NULL || name == NULL)
        return 0;

    for (int i = 0; i < *vars_count; i++) {
        if (strcmp(vars[i], name) == 0)
            return 1;
    }

    if (*vars_count >= BOOL_OPT_MAX_VARS)
        return 0;

    strlcpy(vars[*vars_count], name, ID_LEN);
    (*vars_count)++;

    return 1;
}

static int collect_boolean_vars(struct ASTNode* node, char vars[][ID_LEN], int* vars_count)
{
    ASTIdentifier* id;

    if (node == NULL)
        return 0;

    if (node->nodeType == AST_IDENTIFIER) {
        id = (ASTIdentifier*)node->typedNode;
        return add_boolean_var(vars, vars_count, id != NULL ? id->id : NULL);
    }

    if (node->nodeType == AST_CONSTANT)
        return 1;

    if (!is_boolean_operator(node->nodeType))
        return 0;

    for (unsigned int i = 0; i < node->children_count; i++) {
        if (!collect_boolean_vars(&node->children[i], vars, vars_count))
            return 0;
    }

    return 1;
}

static int var_value(char vars[][ID_LEN], int vars_count, const char* name, int assignment)
{
    for (int i = 0; i < vars_count; i++) {
        if (strcmp(vars[i], name) == 0)
            return (assignment >> i) & 1;
    }

    return 0;
}

static int eval_boolean_expr(struct ASTNode* node, char vars[][ID_LEN], int vars_count, int assignment, int* ok)
{
    ASTIdentifier* id;
    int value;
    int width;
    int lhs;
    int rhs;

    if (node == NULL || ok == NULL)
        return 0;

    switch (node->nodeType) {
        case AST_CONSTANT:
            if (!node_constant_value(node, &value, &width)) {
                *ok = 0;
                return 0;
            }
            return value != 0;

        case AST_IDENTIFIER:
            id = (ASTIdentifier*)node->typedNode;
            if (id == NULL) {
                *ok = 0;
                return 0;
            }
            return var_value(vars, vars_count, id->id, assignment);

        case AST_LOGIC_NOT:
        case AST_BIT_NOT:
            if (node->children_count != 1) {
                *ok = 0;
                return 0;
            }
            return !eval_boolean_expr(&node->children[0], vars, vars_count, assignment, ok);

        case AST_LOGIC_AND:
        case AST_BIT_AND:
            if (node->children_count != 2) {
                *ok = 0;
                return 0;
            }
            lhs = eval_boolean_expr(&node->children[0], vars, vars_count, assignment, ok);
            rhs = eval_boolean_expr(&node->children[1], vars, vars_count, assignment, ok);
            return lhs && rhs;

        case AST_LOGIC_OR:
        case AST_BIT_OR:
            if (node->children_count != 2) {
                *ok = 0;
                return 0;
            }
            lhs = eval_boolean_expr(&node->children[0], vars, vars_count, assignment, ok);
            rhs = eval_boolean_expr(&node->children[1], vars, vars_count, assignment, ok);
            return lhs || rhs;

        default:
            *ok = 0;
            return 0;
    }
}

static int implicant_covers(Implicant implicant, int minterm)
{
    return (minterm & ~implicant.mask) == (implicant.bits & ~implicant.mask);
}

static int add_implicant(Implicant implicants[], int* count, int bits, int mask, int covered)
{
    for (int i = 0; i < *count; i++) {
        if (implicants[i].bits == bits && implicants[i].mask == mask) {
            implicants[i].covered |= covered;
            return 1;
        }
    }

    if (*count >= BOOL_OPT_MAX_IMPLICANTS)
        return 0;

    implicants[*count].bits = bits;
    implicants[*count].mask = mask;
    implicants[*count].covered = covered;
    implicants[*count].used = 0;
    (*count)++;

    return 1;
}

static int merge_implicants(Implicant a, Implicant b, Implicant* result)
{
    int diff;

    if (result == NULL || a.mask != b.mask)
        return 0;

    diff = (a.bits ^ b.bits) & ~a.mask;
    if (diff == 0 || (diff & (diff - 1)) != 0)
        return 0;

    result->bits = a.bits & ~diff;
    result->mask = a.mask | diff;
    result->covered = a.covered | b.covered;
    result->used = 0;

    return 1;
}

static int find_prime_implicants(int minterms[], int minterm_count, Implicant primes[], int* prime_count)
{
    Implicant current[BOOL_OPT_MAX_IMPLICANTS];
    int current_count = 0;

    *prime_count = 0;

    for (int i = 0; i < minterm_count; i++) {
        if (!add_implicant(current, &current_count, minterms[i], 0, 1 << i))
            return 0;
    }

    while (current_count > 0) {
        Implicant next[BOOL_OPT_MAX_IMPLICANTS];
        int next_count = 0;
        int any_merged = 0;

        for (int i = 0; i < current_count; i++)
            current[i].used = 0;

        for (int i = 0; i < current_count; i++) {
            for (int j = i + 1; j < current_count; j++) {
                Implicant merged;
                if (merge_implicants(current[i], current[j], &merged)) {
                    current[i].used = 1;
                    current[j].used = 1;
                    any_merged = 1;
                    if (!add_implicant(next, &next_count, merged.bits, merged.mask, merged.covered))
                        return 0;
                }
            }
        }

        for (int i = 0; i < current_count; i++) {
            if (!current[i].used) {
                if (!add_implicant(primes, prime_count, current[i].bits, current[i].mask, current[i].covered))
                    return 0;
            }
        }

        if (!any_merged)
            break;

        for (int i = 0; i < next_count; i++)
            current[i] = next[i];
        current_count = next_count;
    }

    return 1;
}

static int implicant_cost(Implicant implicant, int vars_count)
{
    int cost = 0;

    for (int i = 0; i < vars_count; i++) {
        if ((implicant.mask & (1 << i)) == 0)
            cost++;
    }

    return cost;
}

static int choose_implicants(Implicant primes[], int prime_count, int minterm_count, int vars_count, int selected[])
{
    int all_covered = (1 << minterm_count) - 1;
    int best_mask = 0;
    int best_literals = 100000;
    int best_terms = 100000;
    int max_subsets;

    if (prime_count <= 0)
        return 0;

    if (prime_count >= 30)
        return 0;

    max_subsets = 1 << prime_count;

    for (int mask = 1; mask < max_subsets; mask++) {
        int covered = 0;
        int literals = 0;
        int terms = 0;

        for (int i = 0; i < prime_count; i++) {
            if (mask & (1 << i)) {
                covered |= primes[i].covered;
                literals += implicant_cost(primes[i], vars_count);
                terms++;
            }
        }

        if ((covered & all_covered) == all_covered &&
            (literals < best_literals || (literals == best_literals && terms < best_terms))) {
            best_mask = mask;
            best_literals = literals;
            best_terms = terms;
        }
    }

    if (best_mask == 0)
        return 0;

    for (int i = 0; i < prime_count; i++)
        selected[i] = (best_mask & (1 << i)) != 0;

    return 1;
}

static struct ASTNode* make_constant_node(int value)
{
    struct ASTNode* node = malloc(sizeof(struct ASTNode));
    ASTConstant* constant = malloc(sizeof(ASTConstant));

    if (node == NULL || constant == NULL) {
        free(node);
        free(constant);
        return NULL;
    }

    ast_const_init_width(constant, value, 1);
    ast_node_init(node, AST_CONSTANT, constant, 0);

    return node;
}

static struct ASTNode* make_identifier_node(const char* name)
{
    struct ASTNode* node = malloc(sizeof(struct ASTNode));
    ASTIdentifier* id = malloc(sizeof(ASTIdentifier));

    if (node == NULL || id == NULL) {
        free(node);
        free(id);
        return NULL;
    }

    ast_id_init(id, (char*)name);
    ast_node_init(node, AST_IDENTIFIER, id, 0);

    return node;
}

static struct ASTNode* make_unary_node(NodeType type, struct ASTNode* child)
{
    struct ASTNode* node = malloc(sizeof(struct ASTNode));

    if (node == NULL || child == NULL) {
        free(node);
        return NULL;
    }

    ast_node_init(node, type, NULL, 0);
    add_node(node, child);
    free(child);

    return node;
}

static struct ASTNode* make_binary_node(NodeType type, struct ASTNode* lhs, struct ASTNode* rhs)
{
    struct ASTNode* node = malloc(sizeof(struct ASTNode));

    if (node == NULL || lhs == NULL || rhs == NULL) {
        free(node);
        return NULL;
    }

    ast_node_init(node, type, NULL, 0);
    add_node(node, lhs);
    add_node(node, rhs);
    free(lhs);
    free(rhs);

    return node;
}

static struct ASTNode* build_implicant_node(Implicant implicant, char vars[][ID_LEN], int vars_count)
{
    struct ASTNode* term = NULL;

    for (int i = 0; i < vars_count; i++) {
        struct ASTNode* literal;

        if (implicant.mask & (1 << i))
            continue;

        literal = make_identifier_node(vars[i]);
        if (literal == NULL)
            return NULL;

        if (((implicant.bits >> i) & 1) == 0)
            literal = make_unary_node(AST_LOGIC_NOT, literal);

        if (term == NULL) {
            term = literal;
        } else {
            term = make_binary_node(AST_LOGIC_AND, term, literal);
        }

        if (term == NULL)
            return NULL;
    }

    if (term == NULL)
        term = make_constant_node(1);

    return term;
}

static struct ASTNode* build_boolean_expression(Implicant primes[], int prime_count, int selected[], char vars[][ID_LEN], int vars_count)
{
    struct ASTNode* expr = NULL;

    for (int i = 0; i < prime_count; i++) {
        struct ASTNode* term;

        if (!selected[i])
            continue;

        term = build_implicant_node(primes[i], vars, vars_count);
        if (term == NULL)
            return NULL;

        if (expr == NULL) {
            expr = term;
        } else {
            expr = make_binary_node(AST_LOGIC_OR, expr, term);
        }

        if (expr == NULL)
            return NULL;
    }

    return expr;
}

static int node_cost(struct ASTNode* node)
{
    int cost = 1;

    if (node == NULL)
        return 0;

    for (unsigned int i = 0; i < node->children_count; i++)
        cost += node_cost(&node->children[i]);

    return cost;
}

static int replace_with_node(struct ASTNode* target, struct ASTNode* replacement)
{
    int str_number;
    int position;

    if (target == NULL || replacement == NULL)
        return 0;

    str_number = target->str_number;
    position = target->position;

    ast_free(target);
    *target = *replacement;
    target->str_number = str_number;
    target->position = position;
    free(replacement);

    return 1;
}

static int replace_with_empty_block(struct ASTNode* node)
{
    int str_number;
    int position;

    if (node == NULL)
        return 0;

    str_number = node->str_number;
    position = node->position;

    ast_free(node);
    ast_node_init(node, AST_BLOCK, NULL, 0);
    node->str_number = str_number;
    node->position = position;

    return 1;
}

static int replace_with_detached_child(struct ASTNode* node, unsigned int child_index)
{
    int str_number;
    int position;
    struct ASTNode replacement;

    if (node == NULL || child_index >= node->children_count)
        return 0;

    str_number = node->str_number;
    position = node->position;
    replacement = node->children[child_index];

    node->children[child_index].nodeType = AST_NONE;
    node->children[child_index].typedNode = NULL;
    node->children[child_index].children = NULL;
    node->children[child_index].children_count = 0;

    ast_free(node);
    *node = replacement;

    if (node->str_number == 0)
        node->str_number = str_number;
    if (node->position == 0)
        node->position = position;

    return 1;
}

static int condition_constant_value(struct ASTNode* condition_node, int* value)
{
    int width;

    if (condition_node == NULL || value == NULL)
        return 0;

    if (condition_node->nodeType == AST_COND &&
        condition_node->children_count == 1) {
        return node_constant_value(&condition_node->children[0], value, &width);
    }

    return node_constant_value(condition_node, value, &width);
}

static int try_remove_unreachable_node(struct ASTNode* node)
{
    int condition;

    if (node == NULL)
        return 0;

    if (node->nodeType == AST_IF && node->children_count >= 2 &&
        condition_constant_value(&node->children[0], &condition)) {
        if (condition)
            return replace_with_detached_child(node, 1);

        return replace_with_empty_block(node);
    }

    if (node->nodeType == AST_FOR && node->children_count >= 3 &&
        condition_constant_value(&node->children[1], &condition) &&
        !condition) {
        return replace_with_detached_child(node, 0);
    }

    return 0;
}

static int try_minimize_boolean_node(struct ASTNode* node)
{
    char vars[BOOL_OPT_MAX_VARS][ID_LEN];
    int vars_count = 0;
    int minterms[BOOL_OPT_MAX_MINTERMS];
    int minterm_count = 0;
    Implicant primes[BOOL_OPT_MAX_IMPLICANTS];
    int prime_count = 0;
    int selected[BOOL_OPT_MAX_IMPLICANTS] = {0};
    struct ASTNode* replacement;
    int assignments;

    if (node == NULL || is_boolean_leaf(node) || !is_boolean_operator(node->nodeType))
        return 0;

    if (!collect_boolean_vars(node, vars, &vars_count))
        return 0;

    if (vars_count == 0)
        return 0;

    assignments = 1 << vars_count;
    for (int assignment = 0; assignment < assignments; assignment++) {
        int ok = 1;
        int value = eval_boolean_expr(node, vars, vars_count, assignment, &ok);

        if (!ok)
            return 0;

        if (value)
            minterms[minterm_count++] = assignment;
    }

    if (minterm_count == 0)
        replacement = make_constant_node(0);
    else if (minterm_count == assignments)
        replacement = make_constant_node(1);
    else {
        if (!find_prime_implicants(minterms, minterm_count, primes, &prime_count))
            return 0;
        if (!choose_implicants(primes, prime_count, minterm_count, vars_count, selected))
            return 0;
        replacement = build_boolean_expression(primes, prime_count, selected, vars, vars_count);
    }

    if (replacement == NULL)
        return 0;

    if (node_cost(replacement) >= node_cost(node)) {
        ast_free(replacement);
        free(replacement);
        return 0;
    }

    return replace_with_node(node, replacement);
}

static int try_fold_node(struct ASTNode* node)
{
    int value;
    int width;

    if (node == NULL)
        return 0;

    if (node->children_count == 1) {
        int child_value;
        int child_width;

        if (!node_constant_value(&node->children[0], &child_value, &child_width))
            return 0;

        if (!fold_unary(node->nodeType, child_value, &value))
            return 0;

        width = node->nodeType == AST_LOGIC_NOT
            ? 1
            : max_int(child_width, constant_width_from_value(value));

        return replace_with_constant(node, value, width);
    }

    if (node->children_count == 2) {
        int lhs_value;
        int lhs_width;
        int rhs_value;
        int rhs_width;

        if (!node_constant_value(&node->children[0], &lhs_value, &lhs_width) ||
            !node_constant_value(&node->children[1], &rhs_value, &rhs_width)) {
            return 0;
        }

        if (!fold_binary(node->nodeType, lhs_value, rhs_value, &value))
            return 0;

        switch (node->nodeType) {
            case AST_LT:
            case AST_LE:
            case AST_EQEQ:
            case AST_NE:
            case AST_GE:
            case AST_GT:
            case AST_LOGIC_AND:
            case AST_LOGIC_OR:
                width = 1;
                break;
            default:
                width = max_int(max_int(lhs_width, rhs_width), constant_width_from_value(value));
                break;
        }

        return replace_with_constant(node, value, width);
    }

    return 0;
}

int optimize_ast_constants(struct ASTNode* root)
{
    int changes = 0;

    if (root == NULL)
        return 0;

    for (unsigned int i = 0; i < root->children_count; i++)
        changes += optimize_ast_constants(&root->children[i]);

    if (try_fold_node(root))
        changes++;

    if (try_minimize_boolean_node(root))
        changes++;

    return changes;
}

int optimize_ast_unreachable_code(struct ASTNode* root)
{
    int changes = 0;

    if (root == NULL)
        return 0;

    for (unsigned int i = 0; i < root->children_count; i++)
        changes += optimize_ast_unreachable_code(&root->children[i]);

    if (try_remove_unreachable_node(root))
        changes++;

    return changes;
}

int optimize_ast(struct ASTNode* root)
{
    int total_changes = 0;
    int pass_changes;

    if (root == NULL)
        return 0;

    do {
        pass_changes = 0;
        pass_changes += optimize_ast_constants(root);
        pass_changes += optimize_ast_unreachable_code(root);
        total_changes += pass_changes;
    } while (pass_changes > 0);

    return total_changes;
}
