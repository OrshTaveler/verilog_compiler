# AST Optimization

This optimizer currently does three bottom-up passes over the AST:

1. Constant folding
2. Boolean minimization
3. Unreachable code removal

All passes are called through:

```c
int optimize_ast(struct ASTNode* root);
```

Constant folding can also be called directly through:

```c
int optimize_ast_constants(struct ASTNode* root);
```

The function returns the number of AST nodes that were replaced.

## Constant Folding

Constant folding evaluates expression nodes whose operands are already constants.

Example:

```verilog
$display("%d", (10 + 1 + 3) / 2);
```

The parser builds an expression tree for `(10 + 1 + 3) / 2`. The optimizer walks from the leaves upward:

```text
10 + 1  -> 11
11 + 3  -> 14
14 / 2  -> 7
```

After optimization, the AST subtree becomes:

```text
AST_CONSTANT value=7
```

The optimizer folds arithmetic, comparisons, bitwise operators, logical operators, and unary operators when all inputs are constants. It does not fold division or modulo by zero.

## Boolean Minimization

Boolean minimization works like a small Karnaugh map / Quine-McCluskey simplifier.

Supported boolean operators:

```text
&&  ||  !  &  |  ~
```

Supported leaves:

```text
identifiers and constants
```

The optimizer only minimizes expressions with up to 4 variables. That keeps the truth table small and close to hand-drawn Karnaugh maps.

## How It Works

For an expression like:

```verilog
(a && b) || (a && !b)
```

the optimizer collects variables:

```text
a, b
```

Then it evaluates the expression for all input combinations:

```text
a b | result
0 0 | 0
0 1 | 0
1 0 | 1
1 1 | 1
```

The true rows are minterms:

```text
10, 11
```

Those two minterms differ only in `b`, so `b` is irrelevant. In a Karnaugh map, these two adjacent cells combine into one group:

```text
a
```

So the AST is rewritten from:

```text
AST_LOGIC_OR
├── AST_LOGIC_AND(a, b)
└── AST_LOGIC_AND(a, !b)
```

to:

```text
AST_IDENTIFIER id="a"
```

## Limitations

This is intentionally small and conservative:

- It minimizes only boolean-shaped AST subtrees.
- It supports up to 4 distinct identifiers.
- It does not simplify expressions containing comparisons mixed with variables yet.
- It rewrites only when the generated expression is smaller than the original AST.
- It treats bitwise boolean operators as single-bit boolean logic during minimization.

This is enough for classic Karnaugh-map patterns such as:

```verilog
(a && b) || (a && !b)       -> a
(a || b) && (a || !b)       -> a
(a && b) || (!a && b)       -> b
(a && b) || (a && b && c)   -> a && b
```

## Unreachable Code Removal

After constant folding and boolean minimization, some control-flow conditions become known at compile time.

The optimizer removes unreachable branches for AST forms currently supported by the parser.

False `if` conditions are replaced with an empty block:

```verilog
if (0) begin
  a = 1;
end
```

becomes:

```text
AST_BLOCK children=0
```

True `if` conditions are unwrapped into the reachable body:

```verilog
if (1) begin
  a = 1;
end
```

becomes the body block.

For loops with constant false conditions keep only their initializer, because the initializer still executes before the condition check:

```verilog
for (integer i = 0; 0; i++) begin
  a = a + i;
end
```

becomes equivalent to:

```verilog
integer i = 0;
```

This pass intentionally does not remove code after `return`, `break`, or `continue`, because those constructs are not represented by this parser yet.
