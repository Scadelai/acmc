%{
#define YYPARSER /* distinguishes Yacc output from other code files */

#include "globals.h"
#include "util.h"
#include "scan.h"

#define YYSTYPE TreeNode *
static TreeNode * savedTree; /* stores syntax tree for later return */
static int yylex(void);
int yyerror(char *msg);


%}
%token NUM ID
%token IF ELSE WHILE RETURN VOID
%right INT
%token ERROR ENDFILE
%token MAIS SUB MULT DIV
%token MENOR MENIG MAIOR MAIIG IGDAD DIFER IGUAL
%token PV VIR APAR FPAR ACOL FCOL ACHAV FCHAV

%nonassoc FPAR
%nonassoc ELSE

%% /* Grammar for c- */

programa: declaracao_lista 
                 { savedTree = $1;}
            ;

declaracao_lista: declaracao_lista declaracao
          { YYSTYPE t = $1;
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

declaracao: var_declaracao  { $$ = $1 ;}
	    | fun_declaracao { $$ = $1; }
	    ;

var_declaracao: INT identificador PV
          { $$ = newExpNode(TypeK);
            $$->attr.name = "INT";
            $$->size = 1;
            $$->child[0] = $2;
            $2->kind.exp =  VarK;
            $2->type = intDType;
          }
	      | INT identificador ACOL numero FCOL PV
            { $$ = newExpNode(TypeK);
              $$->attr.name = "INT";
              $$->size = $4->attr.val;
              $$->child[0] = $2;
              $2->kind.exp =  VarK;
              $2->type = intDType;
              $$->child[0]->child[0] = $4;
              $4->kind.exp =  ConstK;
            }
	      ;

tipo_especificador: INT
              { $$ = newExpNode(TypeK);
                $$->attr.name = "INT";
                $$->type = intDType;
                $$->size = 1;
              }
            | VOID
              { $$ = newExpNode(TypeK);
                $$->attr.name = "VOID";
                $$->type = intDType;
                $$->size = 1;
              }
            ;

fun_declaracao: INT identificador APAR params FPAR composto_decl
            { $$ = newExpNode(TypeK);
              $$->attr.name = "INT";
              $$->child[0] = $2;
              $2->kind.exp = FuncK;
              $2->lineno = $$->lineno;
              $2->type = intDType;
              $2->child[0] = $4;
              $2->child[1] = $6;
            }
        | VOID identificador APAR params FPAR composto_decl
                    { $$ = newExpNode(TypeK);
                      $$->attr.name = "VOID";
                      $$->child[0] = $2;
                      $2->type = voidDType;
                      $2->kind.exp = FuncK;
                      $2->lineno = $$->lineno;
                      $2->child[0] = $4;
                      $2->child[1] = $6;
                    }
        ;

params: param_lista { $$ = $1; }
       | VOID
          { $$ = newExpNode(TypeK);
            $$->attr.name = "VOID";
            $$->size = 1;
            $$->child[0] = NULL;
          }
       ;

param_lista: param_lista VIR param_lista
              { YYSTYPE t = $1;
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

param: tipo_especificador identificador
        { $$ = $1;
          $$->child[0] = $2;
          $2->kind.exp = ParamK;

        }
      | tipo_especificador identificador ACOL FCOL
        { $$ = $1;
          $$->size = 0;
          $$->child[0] = $2;
          $2->kind.exp = ParamK;
        }
      ;

composto_decl: ACHAV local_declaracoes statement_lista FCHAV
              { YYSTYPE t = $2;
                  if (t != NULL){
                    while (t->sibling != NULL)
                       t = t->sibling;
                    t->sibling = $3;
                    $$ = $2;
                  }
                  else $$ = $3;
              }
             | ACHAV FCHAV {}
             | ACHAV  local_declaracoes FCHAV { $$ = $2; }
             | ACHAV statement_lista FCHAV { $$ = $2; }
             ;

local_declaracoes: local_declaracoes var_declaracao
            { YYSTYPE t = $1;
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

statement_lista: statement_lista statement
            { YYSTYPE t = $1;
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

statement: expressap_decl { $$ = $1; }
     | composto_decl { $$ = $1; }
     | selecao_decl { $$ = $1; }
     | iteracao_decl { $$ = $1; }
     | retorno_decl { $$ = $1; }
     ;

expressap_decl: expressao PV { $$ = $1; }
        |  PV {}
        ;

selecao_decl:  IF APAR expressao FPAR statement
          { $$ = newStmtNode(IfK);
            $$->child[0] = $3;
            $$->child[1] = $5;
          }
        | IF APAR expressao FPAR statement ELSE statement
          { $$ = newStmtNode(IfK);
            $$->child[0] = $3;
            $$->child[1] = $5;
            $$->child[2] = $7;
          }
        ;

iteracao_decl: WHILE APAR expressao FPAR statement
        { $$ = newStmtNode(WhileK);
          $$->child[0] = $3;
          $$->child[1] = $5;
        }
        ;

retorno_decl: RETURN PV { $$ = newStmtNode(ReturnK); }
            | RETURN expressao PV
              {
                $$ = newStmtNode(ReturnK);
                $$->child[0] = $2;
              }
            ;

expressao: var IGUAL expressao
      { $$ = newStmtNode(AssignK);
        $$->child[0] = $1;
        $$->child[1] = $3;
      }
    | simples_expressao { $$ = $1; }
    ;


var: identificador { $$ = $1; }
    | identificador ACOL expressao FCOL
      { $$ = $1;
        $$->type = intDType;
        $$->child[0] = $3;
      }
    ;

simples_expressao: soma_expressao relacional soma_expressao
              {   $$ = $2;
                  $$->child[0] = $1;
                  $$->child[1] = $3;
              }
            | soma_expressao { $$ = $1; }
            ;

relacional: MENOR
              { $$ = newExpNode(OpK);
                $$->attr.opr = MENOR;
              }
           | MENIG
              { $$ = newExpNode(OpK);
                $$->attr.opr = MENIG;
              }
           | MAIOR
              { $$ = newExpNode(OpK);
                $$->attr.opr = MAIOR;
              }
           | MAIIG
              { $$ = newExpNode(OpK);
                $$->attr.opr = MAIIG;
              }
           | IGDAD
              { $$ = newExpNode(OpK);
                $$->attr.opr = IGDAD;
              }
           | DIFER
              { $$ = newExpNode(OpK);
                $$->attr.opr = DIFER;
              }
           ;

soma_expressao: soma_expressao soma termo
         { $$ = $2;
           $$->child[0] = $1;
           $$->child[1] = $3;
         }
         | termo { $$ = $1; }
         ;

soma: MAIS
       { $$ = newExpNode(OpK);
         $$->attr.opr = MAIS;
       }
     | SUB
        { $$ = newExpNode(OpK);
          $$->attr.opr = SUB;
        }
     ;

termo: termo mult fator
            { $$ = $2;
              $$->child[0] = $1;
              $$->child[1] = $3;
            }
      | fator { $$ = $1; }
      ;

mult: MULT
       { $$ = newExpNode(OpK);
         $$->attr.opr = MULT;
       }
     | DIV
        { $$ = newExpNode(OpK);
          $$->attr.opr = DIV;
        }
     ;

fator: APAR expressao FPAR { $$ = $2; }
      | var { $$ = $1; }
      | ativacao { $$ = $1; }
      | numero { $$ = $1; }
      ;

ativacao: identificador APAR arg_lista FPAR
        { $$ = newExpNode(CallK);
          $$->attr.name = $1->attr.name;
          $$->child[0] = $3;
        }
        | identificador APAR FPAR
         { $$ = newExpNode(CallK);
           $$->attr.name = $1->attr.name;
         }
     ;


arg_lista: arg_lista VIR expressao
            { YYSTYPE t = $1;
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

identificador: ID
                { $$ = newExpNode(IdK);
                  $$->attr.name = copyString(tokenString);
                }
              ;

numero: NUM      
          { $$ = newExpNode(ConstK);
            $$->type = intDType;
            $$->attr.val = atoi(tokenString);
          }

%%

int yyerror(char * message)
{ fprintf(listing,"Syntax error at line %d: %s\n",lineno,message);
  fprintf(listing,"Current token: ");
  printToken(yychar,tokenString);
  Error = TRUE;
  return 0;
}

/* yylex calls getToken to make Yacc/Bison output
 * compatible with ealier versions of the TINY scanner
 */
static int yylex(void)
{ return getToken(); }

TreeNode * parse(void)
{ yyparse();
  return savedTree;
}
