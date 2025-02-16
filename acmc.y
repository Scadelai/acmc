%{
#define YYPARSER // Define para distinguir a saída do Yacc de outros arquivos

#include "globals.h"
#include "util.h"
#include "scan.h"

#define YYSTYPE TreeNode *
static TreeNode *savedTree; // Árvore de sintaxe abstrata gerada pelo parser
static int yylex(void);
int yyerror(char *msg);
%}

/* Tokens utilizados na gramática */
%token NUM ID
%token IF ELSE WHILE RETURN VOID
%right INT
%token ERROR ENDFILE
%token MAIS SUB MULT DIV
%token MENOR MENIG MAIOR MAIIG IGDAD DIFER IGUAL
%token PV VIR APAR FPAR ACOL FCOL ACHAV FCHAV

%nonassoc FPAR
%nonassoc ELSE

%%
// Gramática da linguagem C-

programa: declaracao_lista 
           { savedTree = $1; } // Salva a árvore sintática gerada
           ;

/* Lista de declarações */
declaracao_lista: declaracao_lista declaracao
           { 
              // Concatena declarações em uma lista encadeada
              YYSTYPE t = $1;
              if (t != NULL){
                while (t->sibling != NULL)
                   t = t->sibling;
                t->sibling = $2;
                $$ = $1;
              }
              else $$ = $2;
           }
          | declaracao { $$ = $1; }
          ;

/* Declaração pode ser de variável ou de função */
declaracao: var_declaracao  { $$ = $1; }
          | fun_declaracao  { $$ = $1; }
          ;

/* Declaração de variável: simples ou vetor */
var_declaracao: INT identificador PV
           { 
              // Declaração de variável simples
              $$ = newExpNode(TypeK);
              $$->attr.name = "INT";
              $$->size = 1;
              $$->child[0] = $2;
              $2->kind.exp = VarK;
              $2->type = intDType;
           }
          | INT identificador ACOL numero FCOL PV
           { 
              // Declaração de vetor (array)
              $$ = newExpNode(TypeK); 
              $$->attr.name = "INT";
              $$->size = $4->attr.val;
              $$->child[0] = $2;
              $2->kind.exp = VarK;
              $2->type = intDType;
              $$->child[0]->child[0] = $4;
              $4->kind.exp = ConstK;
           }
          ;

/* Especificador de tipo: INT ou VOID */
tipo_especificador: INT
           { 
              // Tipo INT
              $$ = newExpNode(TypeK);
              $$->attr.name = "INT";
              $$->type = intDType;
              $$->size = 1;
           }
         | VOID
           { 
              // Tipo VOID
              $$ = newExpNode(TypeK);
              $$->attr.name = "VOID";
              $$->type = voidDType;
              $$->size = 1;
           }
         ;

/* Declaração de função: retorno INT ou VOID */
fun_declaracao: INT identificador APAR params FPAR composto_decl
           { 
              // Função com retorno INT
              $$ = newExpNode(TypeK);
              $$->attr.name = "INT";
              $$->child[0] = $2;
              $2->kind.exp = FuncK;
              $2->type = intDType;
              $2->child[0] = $4; // Parâmetros
              $2->child[1] = $6; // Corpo da função
           }
         | VOID identificador APAR params FPAR composto_decl
           { 
              // Função com retorno VOID
              $$ = newExpNode(TypeK);
              $$->attr.name = "VOID";
              $$->child[0] = $2;
              $2->kind.exp = FuncK;
              $2->type = voidDType;
              $2->child[0] = $4; // Parâmetros
              $2->child[1] = $6; // Corpo da função
           }
         ;

/* Parâmetros: lista de parâmetros ou VOID indicando ausência de parâmetros */
params: param_lista { $$ = $1; }
       | VOID
         { 
            // Função sem parâmetros
            $$ = newExpNode(TypeK);
            $$->attr.name = "VOID";
            $$->size = 1;
            $$->child[0] = NULL;
         }
       ;

/* Lista de parâmetros separados por vírgula */
param_lista: param_lista VIR param
           { 
              // Adiciona novo parâmetro à lista
              YYSTYPE t = $1;
              if (t != NULL){
                while (t->sibling != NULL)
                   t = t->sibling;
                t->sibling = $3;
                $$ = $1;
              }
              else $$ = $3;
           }
         | param { $$ = $1; }
         ;

/* Declaração de parâmetro: simples ou vetor */
param: tipo_especificador identificador
       { 
          // Parâmetro simples
          $$ = $1;
          $$->child[0] = $2;
          $2->kind.exp = ParamK;
       }
     | tipo_especificador identificador ACOL FCOL
       { 
          // Parâmetro do tipo vetor (array)
          $$ = $1;
          $$->size = 0;
          $$->child[0] = $2;
          $2->kind.exp = ParamK;
       }
     ;

/* Declaração composta: bloco de declarações e/ou comandos */
composto_decl: ACHAV local_declaracoes statement_lista FCHAV
              { 
                 // Combina declarações locais e comandos
                 YYSTYPE t = $2;
                 if (t != NULL){
                   while (t->sibling != NULL)
                      t = t->sibling;
                   t->sibling = $3;
                   $$ = $2;
                 }
                 else $$ = $3;
              }
             | ACHAV FCHAV { $$ = NULL; } // Bloco vazio
             | ACHAV local_declaracoes FCHAV { $$ = $2; } // Apenas declarações
             | ACHAV statement_lista FCHAV { $$ = $2; } // Apenas comandos
             ;

/* Declarações locais dentro de um bloco */
local_declaracoes: local_declaracoes var_declaracao
            { 
               // Adiciona declaração de variável à lista local
               YYSTYPE t = $1;
               if (t != NULL){
                 while (t->sibling != NULL)
                    t = t->sibling;
                 t->sibling = $2;
                 $$ = $1;
               }
               else $$ = $2;
            }
          | var_declaracao { $$ = $1; }
          ;

/* Lista de comandos (statements) */
statement_lista: statement_lista statement
            { 
              // Concatena comandos na lista
              YYSTYPE t = $1;
              if (t != NULL){
                while (t->sibling != NULL)
                  t = t->sibling;
                t->sibling = $2;
                $$ = $1;
              }
              else $$ = $2;
            }
          | statement { $$ = $1; }
          ;

/* Comandos: expressão, bloco, seleção, iteração ou retorno */
statement: expressao_decl { $$ = $1; }
         | composto_decl { $$ = $1; }
         | selecao_decl { $$ = $1; }
         | iteracao_decl { $$ = $1; }
         | retorno_decl { $$ = $1; }
         ;

/* Declaração de expressão, que pode ser vazia */
expressao_decl: expressao PV { $$ = $1; }
              | PV { /* Comando vazio */ }
              ;

/* Comando de seleção: if ou if-else */
selecao_decl: IF APAR expressao FPAR statement
              { 
                // Comando if sem else
                $$ = newStmtNode(IfK);
                $$->child[0] = $3; // condição
                $$->child[1] = $5; // comando se verdadeiro
              }
            | IF APAR expressao FPAR statement ELSE statement
              { 
                // Comando if com else
                $$ = newStmtNode(IfK);
                $$->child[0] = $3; // condição
                $$->child[1] = $5; // comando se verdadeiro
                $$->child[2] = $7; // comando se falso
              }
            ;

/* Comando de iteração: while */
iteracao_decl: WHILE APAR expressao FPAR statement
              { 
                // Comando while
                $$ = newStmtNode(WhileK);
                $$->child[0] = $3; // condição
                $$->child[1] = $5; // comando do loop
              }
              ;

/* Comando de retorno: com ou sem valor */
retorno_decl: RETURN PV 
              { 
                // Retorno sem valor
                $$ = newStmtNode(ReturnK); 
              }
            | RETURN expressao PV
              { 
                // Retorno com valor
                $$ = newStmtNode(ReturnK);
                $$->child[0] = $2;
              }
            ;

/* Expressão: atribuição ou expressão simples */
expressao: var IGUAL expressao
           { 
             // Atribuição
             $$ = newStmtNode(AssignK);
             $$->child[0] = $1; // variável
             $$->child[1] = $3; // expressão atribuída
           }
         | simples_expressao { $$ = $1; }
         ;

/* Variável: simples ou acesso a vetor */
var: identificador 
       { $$ = $1; }
    | identificador ACOL expressao FCOL
       { 
         // Acesso a vetor (array)
         $$ = $1;
         $$->type = intDType;
         $$->child[0] = $3; // índice
       }
    ;

/* Expressão simples: com ou sem operador relacional */
simples_expressao: soma_expressao relacional soma_expressao
                   { 
                     // Expressão com operador relacional
                     $$ = $2;
                     $$->child[0] = $1;
                     $$->child[1] = $3;
                   }
                 | soma_expressao { $$ = $1; }
                 ;

/* Operadores relacionais */
relacional: MENOR
              { 
                // Operador '<'
                $$ = newExpNode(OpK);
                $$->attr.opr = MENOR;
              }
           | MENIG
              { 
                // Operador '<='
                $$ = newExpNode(OpK);
                $$->attr.opr = MENIG;
              }
           | MAIOR
              { 
                // Operador '>'
                $$ = newExpNode(OpK);
                $$->attr.opr = MAIOR;
              }
           | MAIIG
              { 
                // Operador '>='
                $$ = newExpNode(OpK);
                $$->attr.opr = MAIIG;
              }
           | IGDAD
              { 
                // Operador '=='
                $$ = newExpNode(OpK);
                $$->attr.opr = IGDAD;
              }
           | DIFER
              { 
                // Operador '!='
                $$ = newExpNode(OpK);
                $$->attr.opr = DIFER;
              }
           ;

/* Expressão de soma/subtração */
soma_expressao: soma_expressao soma termo
                { 
                  // Operação de adição ou subtração
                  $$ = $2;
                  $$->child[0] = $1;
                  $$->child[1] = $3;
                }
              | termo { $$ = $1; }
              ;

/* Operadores '+' e '-' */
soma: MAIS
       { 
         // Operador '+'
         $$ = newExpNode(OpK);
         $$->attr.opr = MAIS;
       }
     | SUB
        { 
          // Operador '-'
          $$ = newExpNode(OpK);
          $$->attr.opr = SUB;
        }
     ;

/* Expressão de multiplicação/divisão */
termo: termo mult fator
         { 
           // Operação de multiplicação ou divisão
           $$ = $2;
           $$->child[0] = $1;
           $$->child[1] = $3;
         }
       | fator { $$ = $1; }
       ;

/* Operadores '*' e '/' */
mult: MULT
       { 
         // Operador '*'
         $$ = newExpNode(OpK);
         $$->attr.opr = MULT;
       }
     | DIV
        { 
          // Operador '/'
          $$ = newExpNode(OpK);
          $$->attr.opr = DIV;
        }
     ;

/* Fatores: expressões entre parênteses, variável, chamada de função ou número */
fator: APAR expressao FPAR 
         { 
           // Expressão entre parênteses
           $$ = $2; 
         }
      | var 
         { 
           // Variável
           $$ = $1; 
         }
      | ativacao 
         { 
           // Chamada de função
           $$ = $1; 
         }
      | numero 
         { 
           // Constante numérica
           $$ = $1; 
         }
      ;

/* Chamada de função: com ou sem argumentos */
ativacao: identificador APAR arg_lista FPAR
          { 
            // Chamada de função com argumentos
            $$ = newExpNode(CallK);
            $$->attr.name = $1->attr.name;
            $$->child[0] = $3; // Lista de argumentos
          }
        | identificador APAR FPAR
          { 
            // Chamada de função sem argumentos
            $$ = newExpNode(CallK);
            $$->attr.name = $1->attr.name;
          }
        ;

/* Lista de argumentos separados por vírgula */
arg_lista: arg_lista VIR expressao
           { 
             // Adiciona argumento à lista
             YYSTYPE t = $1;
             if (t != NULL){
               while (t->sibling != NULL)
                 t = t->sibling;
               t->sibling = $3;
               $$ = $1;
             }
             else $$ = $3;
           }
         | expressao { $$ = $1; }
         ;

/* Identificador: cria nó com o nome do identificador */
identificador: ID
               { 
                 // Cria nó para identificador
                 $$ = newExpNode(IdK);
                 $$->attr.name = copyString(tokenString);
               }
             ;

/* Número: cria nó para constante numérica */
numero: NUM      
         { 
           $$ = newExpNode(ConstK);
           $$->type = intDType;
           $$->attr.val = atoi(tokenString);
         }
         ;

%%

int yyerror(char *message) {
  // Trata erros sintáticos e exibe mensagem com o token e a linha
  fprintf(listing, "ERRO SINTÁTICO: %s. LINHA: %d\n", tokenString, lineno);
  Error = TRUE;
  return 0;
}

static int yylex(void) {
  // Chama a função getToken() do scanner
  return getToken();
}

TreeNode *parse(void) {
  // Inicia a análise sintática e retorna a árvore gerada
  yyparse();
  return savedTree;
}
