#ifndef TREE_H
#define TREE_H

typedef struct TreeNode {
    char *name;                 /* Nome do nó (por exemplo, "if_stmt", "var_declaration", etc.) */
    int lineno;                 /* Linha onde o nó foi gerado */
    struct TreeNode *child[10]; /* Vetor de ponteiros para os filhos (número máximo fixo para simplificação) */
    int nchild;                 /* Quantidade de filhos */
} TreeNode;

#endif