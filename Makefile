PHONY: all

d_lexer := ./lexer
d_build := ./build
d_parser := ./ast
d_general := ./general
d_detecter := ./detecter_unsused_variables


all: 
	clang main.c $(d_parser)/ast.c $(d_parser)/parser.c $(d_parser)/optimization.c $(d_lexer)/lexer.c $(d_general)/stringlib.c $(d_detecter)/detecter.c -o $(d_build)/main
	./build/main
