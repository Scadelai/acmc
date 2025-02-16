#ifndef _SYMTAB_H_
#define _SYMTAB_H_

#include "globals.h"

// Insere um identificador na tabela de símbolos, registrando a linha e a localização de memória
void st_insert(char *name, int lineno, int loc, char *scope, DataType dTypes, IDType idTypes);

// Retorna a localização de memória de um identificador; retorna -1 se não encontrado
int st_lookup(char *name);

// Retorna a localização de uma variável estática com base no nome e escopo
// Retorna -1 se não encontrado ou -2 se for uma variável dinâmica
int st_lookup2(char *name, char *scope);

// Imprime a tabela de símbolos formatada no arquivo de listagem
void printSymTab(FILE *listing);

// Verifica se a função MAIN foi declarada na tabela de símbolos
void findMain(void);

// Retorna o tipo de dado de uma função com base no nome
DataType getFunType(char *nome);

#endif
