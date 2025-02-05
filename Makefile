all:
	flex lexer.l
	gcc -o lexer lex.yy.c -lfl