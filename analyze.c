#include <stdio.h>
#include "globals.h"
#include "symtab.h"
#include "analyze.h"

// Escopo atual; inicia como "global"
char *scope = "global";

// Contador para alocação única de endereços de variáveis
static int location = 0;

/*
Função recursiva para percorrer a árvore sintática.
Aplica as funções preProc (pré-ordem) e postProc (pós-ordem) em cada nó.
*/
static void traverse(TreeNode *t, void (*preProc)(TreeNode *), void (*postProc)(TreeNode *)) {
  if (t != NULL) {
    // Se o primeiro filho for uma declaração de função, atualiza o escopo
    if (t->child[0] != NULL && t->child[0]->kind.exp == FuncK) {
      scope = t->child[0]->attr.name;
    }

    preProc(t);

    // Percorre todos os filhos do nó atual
    for (int i = 0; i < MAXCHILDREN; i++) {
      traverse(t->child[i], preProc, postProc);
    }

    // Ao finalizar a função, retorna o escopo para "global"
    if (t->child[0] != NULL && t->child[0]->kind.exp == FuncK) {
      scope = "global";
    }

    postProc(t);

    // Percorre os nós irmãos
    traverse(t->sibling, preProc, postProc);
  }
}

// Função nula utilizada para travessias onde não se deseja realizar nenhuma operação.
static void nullProc(TreeNode *t) {
  return;
}

/*
Insere os identificadores da árvore sintática na tabela de símbolos.
Realiza verificações para detectar declarações múltiplas ou usos indevidos.
*/
static void insertNode(TreeNode *t) {
  switch (t->nodekind) {
    case StmtK:
      // Processa atribuições
      if (t->kind.stmt == AssignK) {
        // Verifica se a variável foi declarada
        if (st_lookup(t->child[0]->attr.name) == -1) {
          fprintf(listing, "ERRO SEMÂNTICO: Variável '%s' não declarada. LINHA: %d\n", t->child[0]->attr.name, t->lineno);
          Error = TRUE;
        } else {
          // Insere a variável na tabela de símbolos
          st_insert(t->child[0]->attr.name, t->lineno, 0, scope, intDType, var);
        }
        // Marca que a variável já foi processada
        t->child[0]->add = 1;
      }
      break;

    case ExpK:
      switch(t->kind.exp) {
        case IdK:
          // Se o nó não foi marcado como já adicionado
          if (t->add != 1) {
            // Verifica se o identificador foi declarado
            if (st_lookup(t->attr.name) == -1) {
              fprintf(listing, "ERRO SEMÂNTICO: Variável '%s' não declarada. LINHA: %d\n", t->attr.name, t->lineno);
              Error = TRUE;
            } else {
              // Insere o identificador (como função) na tabela
              st_insert(t->attr.name, t->lineno, 0, scope, intDType, fun);
            }
          }
          break;
        case TypeK:
          if (t->child[0] != NULL) {
            switch (t->child[0]->kind.exp) {
              case VarK:
                // Insere a variável se ainda não foi declarada
                if (st_lookup(t->attr.name) == -1) {
                  if (t->child[0]->child[0] == NULL) {
                    st_insert(t->child[0]->attr.name, t->lineno, location++, scope, intDType, var);
                  } else {
                    st_insert(t->child[0]->attr.name, t->lineno, location, scope, intDType, var);
                    location = location + t->child[0]->child[0]->attr.val;
                  }
                } else {
                  st_insert(t->child[0]->attr.name, t->lineno, 0, scope, intDType, var);
                }
                break;
              case FuncK:
                // Insere a função na tabela se não houver declarações duplicadas
                if (st_lookup(t->attr.name) == -1) {
                  st_insert(t->child[0]->attr.name, t->child[0]->lineno, location++, "global", t->child[0]->type, fun);
                } else {
                  fprintf(listing, "ERRO SEMÂNTICO: Múltiplas declarações de '%s'. LINHA: %d\n", t->child[0]->attr.name, t->child[0]->lineno);
                }
                break;
              default:
                break;
            }
          }
          break;
        case CallK:
          // Verifica se a função foi declarada, exceto funções pré-definidas "input" e "output"
          if (st_lookup(t->attr.name) == -1 && strcmp(t->attr.name, "input") != 0 && strcmp(t->attr.name, "output") != 0) {
            fprintf(listing, "ERRO SEMÂNTICO: Função '%s' não declarada. LINHA: %d\n", t->attr.name, t->lineno);
            Error = TRUE;
          } else {
            st_insert(t->attr.name, t->lineno, 0, scope, 0, fun);
          }
          break;
        case ParamK:
          st_insert(t->attr.name,t->lineno,location++, scope,intDType, var);
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

/*
Constrói a tabela de símbolos a partir da árvore sintática.
Insere funções pré-definidas, percorre a árvore para inserir nós, realiza a verificação de tipos
e exibe a tabela caso não haja erros semânticos.
*/
void build_symbol_table(TreeNode *syntax_tree) {
  // Insere funções pré-definidas na tabela de símbolos
  st_insert("input", 0, location++, "global", intDType, fun);
  st_insert("output", 0, location++, "global", voidDType, fun);

  // Percorre a árvore inserindo os nós na tabela de símbolos
  traverse(syntax_tree, insertNode, nullProc);

  // Realiza a verificação de tipos na árvore
  typeCheck(syntax_tree);

  // Verifica a existência da função main (ou similar)
  findMain();

  // Se não houver erros, imprime a tabela de símbolos
  if (!Error) {
    fprintf(listing, "\nSymbol table:\n\n");
    printSymTab(listing);
  }
}

// Registra um erro de tipo, exibindo mensagem e linha correspondente.
static void typeError(TreeNode *t, char *message) {
  fprintf(listing, "ERRO SEMÂNTICO: Erro de tipo: %s. LINHA: %d\n", message, t->lineno);
  Error = TRUE;
}

/*
Realiza a verificação de tipos em um nó específico da árvore.
Verifica, por exemplo, se uma função do tipo void é utilizada de maneira incorreta.
*/
void checkNode(TreeNode *t) {
  switch (t->nodekind) {
    case ExpK:
      switch (t->kind.exp) {
        case OpK:
        // Se algum dos operandos for o resultado de uma chamada de função void, gera erro
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
          // Impede que o resultado de uma função void seja atribuído
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

/*
Inicia a verificação de tipos na árvore sintática utilizando uma travessia pós-ordem.
*/
void typeCheck(TreeNode *syntax_tree) {
  traverse(syntax_tree, checkNode, nullProc);
}

// Reset analysis state for new compilation
void analyze_reset(void) {
  location = 0;
  scope = "global";
}
