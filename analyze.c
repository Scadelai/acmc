/****************************************************/
/* File: analyze.c                                  */
/* Semantic analyzer implementation                 */
/* for the TINY compiler                            */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/
#include <stdio.h>
#include "globals.h"
#include "symtab.h"
#include "analyze.h"

char *scope = "global"; // padrão se não atribuido

/* counter for variable memory locations */
static int location = 0;


/* Procedure traverse is a generic recursive
 * syntax tree traversal routine:
 * it applies preProc in preorder and postProc
 * in postorder to tree pointed to by t
 */
static void traverse(TreeNode *t, void (*preProc)(TreeNode *), void (*postProc)(TreeNode *)) {
  if (t != NULL) {
    if (t->child[0] != NULL && t->child[0]->kind.exp == FuncK) {
      scope = t->child[0]->attr.name;
    }

    preProc(t);

    for (int i = 0; i < MAXCHILDREN; i++) {
      traverse(t->child[i], preProc, postProc);
    }

    if (t->child[0] != NULL && t->child[0]->kind.exp == FuncK) {
      scope = "global";
    }

    postProc(t);
    traverse(t->sibling, preProc, postProc);
  }
}


/* nullProc is a do-nothing procedure to
 * generate preorder-only or postorder-only
 * traversals from traverse
 */
static void nullProc(TreeNode *t) {
  if (t==NULL) {
    return;
  } else {
    return;
  }
}

/* Procedure insertNode inserts
 * identifiers stored in t into
 * the symbol table
 */
static void insertNode(TreeNode *t) {
  switch (t->nodekind) {
    case StmtK:
      if (t->kind.stmt == AssignK) {
          // procurar na tabela
          if (st_lookup(t->child[0]->attr.name) == -1){
          // não achou
            fprintf(listing,"ERRO SEMÂNTICO: Variável '%s' não declarada. LINHA: %d\n", t->child[0]->attr.name, t->lineno);
            Error = TRUE;
          }
          else {
            // achou
            // insere na tabela
            st_insert(t->child[0]->attr.name, t->lineno,0, scope, intDType, var);
          }
          t->child[0]->add = 1; // add linha
      }
      break;
    case ExpK:
      switch(t->kind.exp) {
        case IdK:
          if (t->add != 1){
            if (st_lookup(t->attr.name) == -1) {
              // não achou
              fprintf(listing,"ERRO SEMÂNTICO: Variável '%s' não declarada. LINHA: %d\n", t->attr.name, t->lineno);
              Error = TRUE;
            }
            else {
              //achou
              //insere
              st_insert(t->attr.name, t->lineno,0, scope, intDType, fun);
            }
          }
          break;
        case TypeK:
          if (t->child[0] != NULL) {
            switch (t->child[0]->kind.exp) {
              case  VarK:
                if (st_lookup(t->attr.name) == -1) {
                  if (t->child[0]->child[0] == NULL) {
                    st_insert(t->child[0]->attr.name, t->lineno,location++, scope, intDType, var);
                  } else {
                    st_insert(t->child[0]->attr.name, t->lineno,location, scope, intDType, var);
                    location = location + t->child[0]->child[0]->attr.val;
                  }
                } else {
                  st_insert(t->child[0]->attr.name, t->lineno,0, scope, intDType, var);      
                }
                break;
              case FuncK:
                if (st_lookup(t->attr.name) == -1) {
                  // não achou
                  // insere
                  st_insert(t->child[0]->attr.name, t->child[0]->lineno, location++, "global", t->child[0]->type, fun);
                } else {
                  // achou
                  fprintf(listing, "ERRO SEMÂNTICO: Múltiplas declarações de '%s'. LINHA: %d\n", t->child[0]->attr.name, t->child[0]->lineno);
                }
                break;
              default:
                break;
            }
          }
          break;
        case CallK:
          if (st_lookup(t->attr.name) == -1 && strcmp(t->attr.name, "input") != 0 && strcmp(t->attr.name, "output") != 0) {
            fprintf(listing,"ERRO SEMÂNTICO: Função '%s' não declarada. LINHA: %d\n", t->attr.name, t->lineno);
            Error = TRUE;
          } else {
            st_insert(t->attr.name,t->lineno,0, scope,0,fun);
          }
          break;
        default:
          break;
        case ParamK:
          st_insert(t->attr.name,t->lineno,location++, scope,intDType, var);
          break;
      }
      break;
    default:
      break;
  }
}

/* Function buildSymtab constructs the symbol
 * table by preorder traversal of the syntax tree
 */
void buildSymtab(TreeNode *syntaxTree) {
  // TODO: why?
  // st_insert("input", 0, location++, "global", intDType, fun);
  // st_insert("output", 0, location++, "global", voidDType, fun);
  traverse(syntaxTree,insertNode,nullProc);
  typeCheck(syntaxTree);
  findMain();
  // só faz a tabela de simbolos se não ocorreu erro antes
  if (!Error ) {
    fprintf(listing,"\nSymbol table:\n\n");
    printSymTab(listing);
  }
}

static void typeError(TreeNode *t, char *message) {
  fprintf(listing,"ERRO SEMÂNTICO: Erro de tipo: %s'. LINHA: %d\n", message, t->lineno);
  Error = TRUE;
}

/* Procedure checkNode performs
 * type checking at a single tree node
 */
void checkNode(TreeNode *t) {
  switch (t->nodekind) { 
    case ExpK:
      switch (t->kind.exp) {
        case OpK:
          if (((t->child[0]->kind.exp == CallK) &&( getFunType(t->child[0]->attr.name)) == voidDType) || ((t->child[1]->kind.exp == CallK) && (getFunType(t->child[1]->attr.name) == voidDType))) {
            typeError(t->child[0], "Operando com função VOID");
          }
          break;
        default:
          break;
      }
      break;
    case StmtK:
      switch (t->kind.stmt) {
        case AssignK:
          if (t->child[1]->kind.exp == CallK && getFunType(t->child[1]->attr.name) == voidDType) {
            typeError(t->child[1], "Função tipo VOID sendo atribuída");
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

/* Procedure typeCheck performs type checking
 * by a postorder syntax tree traversal
 */
void typeCheck(TreeNode *syntaxTree) {
  traverse(syntaxTree,checkNode, nullProc);
}
