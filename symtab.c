#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

#define SIZE 211   // Tamanho da tabela hash
#define SHIFT 4    // Fator de deslocamento usado na função hash

// Função hash: calcula o índice da tabela hash para uma dada chave
static int hash(char *key) {
  int temp = 0;
  int i = 0;
  while (key[i] != '\0') {
    temp = ((temp << SHIFT) + key[i]) % SIZE;
    ++i;
  }
  return temp;
}

// Estrutura para armazenar os números de linha em que uma variável é referenciada
typedef struct LineListRec {
  int lineno;
  struct LineListRec *next;
} *LineList;

// Estrutura que representa cada registro (bucket) na tabela de símbolos
typedef struct BucketListRec {
  char *name;
  char *scope;
  DataType dTypes;
  IDType idTypes;
  LineList lines;
  int memloc;
  struct BucketListRec *next;
} *BucketList;

// Tabela hash para armazenar os registros dos identificadores
static BucketList hashTable[SIZE];

// Insere um identificador na tabela de símbolos, registrando número da linha e localização na memória
void st_insert(char *name, int lineno, int opr, char *scope, DataType DType, IDType idTypes) {
  int h = hash(name);
  BucketList l = hashTable[h];

  // Percorre a lista para encontrar um registro com o mesmo nome
  while (l != NULL && strcmp(name, l->name) != 0) {
    l = l->next;
  }

  // Se o identificador não foi encontrado ou se certas condições são atendidas, insere um novo registro
  if (l == NULL || (opr != 0 && l->idTypes != fun && l->scope != scope)) {
    l = malloc(sizeof(struct BucketListRec));
    l->name = name;
    l->lines = malloc(sizeof(struct LineListRec));
    l->lines->lineno = lineno;
    l->lines->next = NULL;
    l->memloc = opr;
    l->dTypes = DType;
    l->idTypes = idTypes;
    l->scope = scope;
    l->next = hashTable[h];
    hashTable[h] = l;
  }
  // Se o nome já foi usado para declarar uma função e agora está sendo usado para variável
  else if (l->idTypes == fun && idTypes == var) {
    fprintf(listing, "ERRO SEMÂNTICO: Nome '%s' usado para declaração de função. LINHA: %d\n", name, lineno);
    Error = TRUE;
  }
  // Se o identificador já foi declarado no mesmo escopo
  else if (l->scope == scope && opr != 0) {
    fprintf(listing, "ERRO SEMÂNTICO: Múltiplas declarações de '%s'. LINHA: %d\n", name, lineno);
    Error = TRUE;
  }
  // Se o identificador está sendo usado em um escopo diferente e não é global, procura pela declaração global
  else if (l->scope != scope && (strcmp(l->scope, "global") != 0)) {
    while (l != NULL) {
      if (strcmp(l->scope, "global") == 0 && strcmp(name, l->name) == 0) {
        LineList t = l->lines;
        while (t->next != NULL) {
          t = t->next;
        }
        t->next = (LineList)malloc(sizeof(struct LineListRec));
        t->next->lineno = lineno;
        t->next->next = NULL;
        break;
      }
      l = l->next;
    }
    if (l == NULL) {
      fprintf(listing, "ERRO SEMÂNTICO: Variável '%s' não declarada. LINHA: %d\n", name, lineno);
      Error = TRUE;
    }
  }
  // Se opr é zero, apenas registra uma nova ocorrência (linha) do identificador
  else if (opr == 0) {
    LineList t = l->lines;
    while (t->next != NULL) {
      t = t->next;
    }
    t->next = malloc(sizeof(struct LineListRec));
    t->next->lineno = lineno;
    t->next->next = NULL;
  }
}

// Procura um identificador na tabela de símbolos e retorna sua localização na memória; retorna -1 se não encontrado
int st_lookup(char *name) {
  int h = hash(name);
  BucketList l = hashTable[h];
  while (l != NULL && strcmp(name, l->name) != 0) {
    l = l->next;
  }
  if (l == NULL) {
    return -1;
  } else {
    return l->memloc;
  }
}

// Imprime a tabela de símbolos de forma formatada no arquivo de listagem
void printSymTab(FILE *listing) {
  int i;
  char *id, *datatypes;
  fprintf(listing, "----------------------------------------------------------------------------------------\n");
  fprintf(listing, "Name       Scope      Type     Data Type   Lines\n");
  fprintf(listing, "----------------------------------------------------------------------------------------\n");
  
  for (i = 0; i < SIZE; ++i) {
    if (hashTable[i] != NULL) {
      BucketList l = hashTable[i];
      while (l != NULL) {
        LineList t = l->lines;
        fprintf(listing, "%-10s ", l->name);
        fprintf(listing, "%-10s  ", l->scope);
        
        if (l->idTypes == var)
          id = "var";
        else if (l->idTypes == fun)
          id = "func";
        
        if (l->dTypes == intDType)
          datatypes = "INT";
        else if (l->dTypes == voidDType)
          datatypes = "VOID";
        
        fprintf(listing, "%-7s  ", id);
        fprintf(listing, "%-9s  ", datatypes);
        
        while (t != NULL) {
          fprintf(listing, "%3d ", t->lineno);
          t = t->next;
        }
        fprintf(listing, "\n");
        l = l->next;
      }
    }
  }
}

// Verifica se a função "main" foi declarada; emite erro se não encontrada
void findMain(void) {
  int h = hash("main");
  BucketList l =  hashTable[h];

  while ((l != NULL) && ((strcmp("main",l->name) != 0 || l->idTypes == var))) {
    l = l->next;
  }

  if (l == NULL) {
    fprintf(listing, "ERRO SEMÂNTICO: Função MAIN não declarada.\n");
    Error = TRUE;
  }
}

/*
Retorna o tipo de dado de uma função dado seu nome.
A função procura na tabela de símbolos (implementada com hash) 
pelo registro que possui o nome especificado. Se o registro for encontrado,
retorna o tipo de dado (dTypes) associado; caso contrário, retorna -1.
*/
DataType getFunType(char *nome) {
  int h = hash(nome);
  BucketList l =  hashTable[h];

  while ((l != NULL) && (strcmp(nome,l->name) != 0)) {
    l = l->next;
  }

  if (l == NULL) {
    return -1;
  } else {
    return l->dTypes;
  }
}
