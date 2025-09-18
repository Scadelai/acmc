#ifndef _ANALYZE_H_
#define _ANALYZE_H_

// Constrói a tabela de símbolos através de uma travessia em pré-ordem na árvore sintática
void build_symbol_table(TreeNode *);

// Realiza a verificação de tipos através de uma travessia em pós-ordem na árvore sintática
void typeCheck(TreeNode *);

// Reset analysis state (location counter and scope)
void analyze_reset(void);

#endif
