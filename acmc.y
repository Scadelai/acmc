%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"  /* Funções da tabela de símbolos */
#include "tree.h" /* Inclui a definição de TreeNode */

extern int line_num;
extern int yylex();
void yyerror(const char *s);

/* Função para alocar um nó da árvore */
TreeNode *newNode(const char *name, int lineno) {
    TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
    node->name = strdup(name);
    node->lineno = lineno;
    node->nchild = 0;
    for (int i = 0; i < 10; i++)
        node->child[i] = NULL;
    return node;
}

/* Função para imprimir a árvore sintática (recursivamente) */
void printTree(TreeNode *root, int indent) {
    if(root == NULL) return;
    for(int i = 0; i < indent; i++) 
        printf("  ");
    printf("%s (linha %d)\n", root->name, root->lineno);
    for(int i = 0; i < root->nchild; i++){
        printTree(root->child[i], indent+1);
    }
}

/* Ponteiro para a raiz da árvore sintática */
TreeNode *syntaxTree = NULL;

/* Protótipo da função de análise semântica */
void semanticAnalysis(TreeNode *node, char *currentScope);

%}

/* Definição dos tipos e tokens usados */
%union {
    int num;
    char *id;
    struct TreeNode *node;
}

%token <num> NUM
%token <id> ID
%token IF ELSE WHILE RETURN INT VOID
%token EQ NE LE GE LT GT ASSIGN SEMI COMMA LPAREN RPAREN LBRACKET RBRACKET LBRACE RBRACE PLUS MINUS TIMES DIVIDE

%type <node> program declaration_list declaration var_declaration type_specifier fun_declaration params param_list param compound_stmt local_declarations local_declaration statement_list statement  selection_stmt iteration_stmt return_stmt expression var simple_expression additive_expression term factor call args arg_list

%%

/* Gramática simplificada para C– */
/* (Observe que esta gramática é apenas exemplificativa e não cobre todos os casos da linguagem.) */

program:
    declaration_list { syntaxTree = $1; }
;

declaration_list:
    declaration { 
        $$ = newNode("declaration_list", line_num);
        $$->child[0] = $1;
        $$->nchild = 1;
    }
    | declaration_list declaration {
        $$ = $1;
        $$->child[$$->nchild++] = $2;
    }
;

declaration:
    var_declaration { $$ = $1; }
    | fun_declaration { $$ = $1; }
;

var_declaration:
    type_specifier ID SEMI {
        $$ = newNode("var_declaration", line_num);
        $$->child[0] = $1; /* Tipo */
        /* Nó para o identificador */
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        $$->nchild = 2;
    }
    | type_specifier ID LBRACKET NUM RBRACKET SEMI {
        $$ = newNode("var_declaration_array", line_num);
        $$->child[0] = $1;
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        TreeNode *numNode = newNode("NUM", line_num);
        $$->child[2] = numNode;
        $$->nchild = 3;
    }
;

type_specifier:
    INT { $$ = newNode("int", line_num); }
    | VOID { $$ = newNode("void", line_num); }
;

fun_declaration:
    type_specifier ID LPAREN params RPAREN compound_stmt {
        $$ = newNode("fun_declaration", line_num);
        $$->child[0] = $1; /* Tipo de retorno */
        /* Nó para o identificador da função */
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        $$->child[2] = $4; /* Parâmetros */
        $$->child[3] = $6; /* Corpo da função */
        $$->nchild = 4;
    }
;

params:
    param_list { $$ = $1; }
    | VOID { $$ = newNode("params_void", line_num); }
;

param_list:
    param { 
        $$ = newNode("param_list", line_num); 
        $$->child[0] = $1; 
        $$->nchild = 1; 
    }
    | param_list COMMA param { 
         $$ = $1; 
         $$->child[$$->nchild++] = $3; 
    }
;

param:
    type_specifier ID { 
        $$ = newNode("param", line_num); 
        $$->child[0] = $1; 
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        $$->nchild = 2;
    }
    | type_specifier ID LBRACKET RBRACKET { 
        $$ = newNode("param_array", line_num); 
        $$->child[0] = $1; 
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        $$->nchild = 2;
    }
;

compound_stmt:
    LBRACE local_declarations statement_list RBRACE {
        $$ = newNode("compound_stmt", line_num);
        $$->child[0] = $2;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
;

local_declarations:
    /* vazio */ { $$ = newNode("local_declarations_empty", line_num); }
    | local_declarations local_declaration {
        $$ = $1;
        $$->child[$$->nchild++] = $2;
    }
;

local_declaration:
    type_specifier ID SEMI {
        $$ = newNode("local_declaration", line_num);
        $$->child[0] = $1;
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        $$->nchild = 2;
    }
    | type_specifier ID LBRACKET NUM RBRACKET SEMI {
        $$ = newNode("local_declaration_array", line_num);
        $$->child[0] = $1;
        TreeNode *idNode = newNode($2, line_num);
        $$->child[1] = idNode;
        TreeNode *numNode = newNode("NUM", line_num);
        $$->child[2] = numNode;
        $$->nchild = 3;
    }
;

statement_list:
    /* vazio */ { $$ = newNode("statement_list_empty", line_num); }
    | statement_list statement {
        $$ = $1;
        $$->child[$$->nchild++] = $2;
    }
;

statement:
    expression SEMI {
        $$ = newNode("expression_stmt", line_num);
        $$->child[0] = $1;
        $$->nchild = 1;
    }
    | compound_stmt { $$ = $1; }
    | /* Outras sentenças: seleção, iteração, return */
      /* Para simplificar, são implementadas a seguir */
      selection_stmt { $$ = $1; }
    | iteration_stmt { $$ = $1; }
    | return_stmt { $$ = $1; }
;

selection_stmt:
    IF LPAREN expression RPAREN statement {
        $$ = newNode("if_stmt", line_num);
        $$->child[0] = $3;
        $$->child[1] = $5;
        $$->nchild = 2;
    }
    | IF LPAREN expression RPAREN statement ELSE statement {
        $$ = newNode("if_else_stmt", line_num);
        $$->child[0] = $3;
        $$->child[1] = $5;
        $$->child[2] = $7;
        $$->nchild = 3;
    }
;

iteration_stmt:
    WHILE LPAREN expression RPAREN statement {
        $$ = newNode("while_stmt", line_num);
        $$->child[0] = $3;
        $$->child[1] = $5;
        $$->nchild = 2;
    }
;

return_stmt:
    RETURN SEMI {
        $$ = newNode("return_stmt", line_num);
        $$->nchild = 0;
    }
    | RETURN expression SEMI {
        $$ = newNode("return_expr_stmt", line_num);
        $$->child[0] = $2;
        $$->nchild = 1;
    }
;

expression:
    var ASSIGN expression {
        $$ = newNode("assign_expr", line_num);
        $$->child[0] = $1;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
    | simple_expression { $$ = $1; }
;

var:
    ID {
        $$ = newNode("var", line_num);
        TreeNode *idNode = newNode($1, line_num);
        $$->child[0] = idNode;
        $$->nchild = 1;
    }
    | ID LBRACKET expression RBRACKET {
        $$ = newNode("array_var", line_num);
        TreeNode *idNode = newNode($1, line_num);
        $$->child[0] = idNode;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
;

simple_expression:
    additive_expression { $$ = $1; }
    /* Outras produções (com operadores relacionais) podem ser adicionadas */
;

additive_expression:
    term { $$ = $1; }
    | additive_expression PLUS term {
        $$ = newNode("add_expr_plus", line_num);
        $$->child[0] = $1;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
    | additive_expression MINUS term {
        $$ = newNode("add_expr_minus", line_num);
        $$->child[0] = $1;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
;

term:
    factor { $$ = $1; }
    | term TIMES factor {
        $$ = newNode("term_times", line_num);
        $$->child[0] = $1;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
    | term DIVIDE factor {
        $$ = newNode("term_divide", line_num);
        $$->child[0] = $1;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
;

factor:
    LPAREN expression RPAREN { $$ = $2; }
    | var { $$ = $1; }
    | call { $$ = $1; }
    | NUM { $$ = newNode("NUM", line_num); }
;

call:
    ID LPAREN args RPAREN {
        $$ = newNode("call", line_num);
        TreeNode *idNode = newNode($1, line_num);
        $$->child[0] = idNode;
        $$->child[1] = $3;
        $$->nchild = 2;
    }
;

args:
    arg_list { $$ = $1; }
    | /* vazio */ { $$ = newNode("args_empty", line_num); }
;

arg_list:
    expression { 
        $$ = newNode("arg_list", line_num); 
        $$->child[0] = $1; 
        $$->nchild = 1; 
    }
    | arg_list COMMA expression {
        $$ = $1;
        $$->child[$$->nchild++] = $3;
    }
;

%%

/* Função principal: chama o parser, imprime a árvore, realiza análise semântica e imprime a tabela de símbolos */
int main(int argc, char **argv) {
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (!f) { perror("Erro ao abrir o arquivo"); return 1; }
        extern FILE *yyin;
        yyin = f;
    }
    yyparse();
    printf("\nÁrvore sintática:\n");
    printTree(syntaxTree, 0);

    /* Inicializa a tabela de símbolos e executa a análise semântica */
    initSymbolTable();
    semanticAnalysis(syntaxTree, "global");
    printf("\nTabela de Símbolos:\n");
    printSymbolTable();
    freeSymbolTable();

    return 0;
}

/* Função de tratamento de erros sintáticos */
void yyerror(const char *s) {
    fprintf(stderr, "ERRO SINTÁTICO: %s LINHA: %d\n", s, line_num);
}

/* Função de análise semântica simples:
   - Registra declarações de variáveis, arrays, funções e parâmetros na tabela de símbolos.
   - Verifica usos de variáveis (nó "var") se estão declaradas no escopo corrente ou global.
*/
void semanticAnalysis(TreeNode *node, char *currentScope) {
    if(node == NULL) return;
    
    /* Registra declarações */
    if(strcmp(node->name, "var_declaration") == 0) {
        char *id = node->child[1]->name;
        char *type = node->child[0]->name;
        addSymbol(id, type, currentScope, node->lineno);
    } else if(strcmp(node->name, "var_declaration_array") == 0) {
        char *id = node->child[1]->name;
        char *type = node->child[0]->name;
        addSymbol(id, type, currentScope, node->lineno);
    } else if(strcmp(node->name, "fun_declaration") == 0) {
        char *id = node->child[1]->name;
        char *type = node->child[0]->name;
        addSymbol(id, type, "global", node->lineno);
        /* Para os filhos dentro do corpo da função, o novo escopo é o nome da função */
        currentScope = id;
    } else if(strcmp(node->name, "param") == 0 ||
              strcmp(node->name, "param_array") == 0) {
        char *id = node->child[1]->name;
        char *type = node->child[0]->name;
        addSymbol(id, type, currentScope, node->lineno);
    }
    
    /* Verifica usos de variável (nó "var") */
    if(strcmp(node->name, "var") == 0) {
        char *id = node->child[0]->name;
        if (!existsSymbol(id, currentScope) && !existsSymbol(id, "global")) {
            printf("ERRO SEMÂNTICO: %s não declarado LINHA: %d\n", id, node->lineno);
        }
    }
    
    /* Percorre recursivamente os filhos */
    for (int i = 0; i < node->nchild; i++) {
        semanticAnalysis(node->child[i], currentScope);
    }
}
