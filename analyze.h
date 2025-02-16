#ifndef _ANALYZE_H_
#define _ANALYZE_H_

/* Function build_symbol_table constructs the symbol
 * table by preorder traversal of the syntax tree
 */
void build_symbol_table(TreeNode *);

/* Procedure typeCheck performs type checking
 * by a postorder syntax tree traversal
 */
void typeCheck(TreeNode *);

#endif
