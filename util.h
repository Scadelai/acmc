#ifndef _UTIL_H_
#define _UTIL_H_

// Imprime um token e seu lexema no arquivo de listagem
void printToken(TokenType, const char *);

// Cria e retorna um novo nó de declaração para a árvore sintática
TreeNode *newStmtNode(StmtKind);

// Cria e retorna um novo nó de expressão para a árvore sintática
TreeNode *newExpNode(ExpKind);

// Cria uma cópia de uma string, alocando a memória necessária para ela
char *copyString(char *);

// Imprime a árvore sintática no arquivo de listagem utilizando indentação para indicar a hierarquia
void print_tree(TreeNode *);

#endif
