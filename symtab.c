/****************************************************/
/* File: symtab.c                                   */
/* Symbol table implementation for the TINY compiler*/
/* (allows only one symbol table)                   */
/* Symbol table is implemented as a chained         */
/* hash table                                       */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

/* SIZE is the size of the hash table */
#define SIZE 211

/* SHIFT is the power of two used as multiplier
   in hash function  */
#define SHIFT 4

/* the hash function */
static int hash (char *key) {
  int temp = 0;
  int i = 0;

  while (key[i] != '\0') {
    temp = ((temp << SHIFT) + key[i]) % SIZE;
    ++i;
  }

  return temp;
}

/* the list of line numbers of the source
 * code in which a variable is referenced
 */
typedef struct LineListRec {
  int lineno;
  struct LineListRec *next;
} *LineList;

/* The record in the bucket lists for
 * each variable, including name,
 * assigned memory location, and
 * the list of line numbers in which
 * it appears in the source code
 */
typedef struct BucketListRec {
  char *name;
  char *scope;
  DataType dTypes;
  IDType idTypes;
  LineList lines;
  int memloc ; /* memory location for variable */
  struct BucketListRec *next;
} *BucketList;

/* the hash table */
static BucketList hashTable[SIZE];

/* Procedure st_insert inserts line numbers and
 * memory locations into the symbol table
 * loc = memory location is inserted only the
 * first time, otherwise ignored
 */

void st_insert(char *name, int lineno, int opr, char *scope, DataType DType, IDType idTypes ) {
  int h = hash(name);
  BucketList l =  hashTable[h];
  // última com mesmo nome
  while ((l != NULL) && ((strcmp(name,l->name) != 0))) {
    l = l->next;
  }
  // variable not yet in table
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
  } else if (l->idTypes == fun  && idTypes == var) {
    fprintf(listing,"ERRO SEMÂNTICO: Nome '%s' usado para declaração de função. LINHA: %d\n", name, lineno);
    Error = TRUE;
  } else if (l->scope == scope && opr != 0) {
    fprintf(listing,"ERRO SEMÃNTICO: Múltiplas declarações de '%s'. LINHA: %d\n", name, lineno);
    Error = TRUE;
  } else if (l->scope != scope && (strcmp(l->scope,"global") != 0)) {
    // procura por variável global antes de supor que não existe
    while ((l != NULL)) {
      if((strcmp(l->scope, "global") == 0) && ((strcmp( name, l->name) == 0))) {
        LineList t = l->lines;
        while (t->next != NULL) t = t->next;
        t->next = (LineList) malloc(sizeof(struct LineListRec));
        t->next->lineno = lineno;
        t->next->next = NULL;
        break;
      }
      l = l->next;
    }

    if (l == NULL) {
      fprintf(listing,"ERRO SEMÂNTICO: Variável '%s' não declarada. LINHA: %d\n",name, lineno);
      Error = TRUE;
    }
  } else if(opr == 0) {
    LineList t = l->lines;

    while (t->next != NULL) {
      t = t->next;
    }

    t->next = malloc(sizeof(struct LineListRec));
    t->next->lineno = lineno;
    t->next->next = NULL;
  }
}

/* Function st_lookup returns the memory
 * location of a variable or -1 if not found
 */
int st_lookup(char *name) {
  int h = hash(name);
  BucketList l =  hashTable[h];

  while ((l != NULL) && (strcmp(name,l->name) != 0)) {
    l = l->next;
  }

  if (l == NULL) {
    return -1;
  } else {
    return l->memloc;
  }
}


/* Procedure printSymTab prints a formatted
 * listing of the symbol table contents
 * to the listing file
 */
void printSymTab(FILE *listing) {
  int i;
  char *id, *datatypes;
  fprintf(listing, "----------------------------------------------------------------------------------------\n");
  fprintf(listing, "Nome       Escopo      Tipo     Tipo dado   Linhas\n");
  fprintf(listing, "----------------------------------------------------------------------------------------\n");
  
  for (i=0; i < SIZE; ++i) {
    if (hashTable[i] != NULL) {
      BucketList l = hashTable[i];

      while (l != NULL) {
        LineList t = l->lines;
        fprintf(listing,"%-10s ",l->name);
        fprintf(listing,"%-10s  ",l->scope);

        if(l->idTypes == var) {
          id = "var";
        } else if (l->idTypes == fun) {
          id = "func";
        }

        if (l->dTypes == intDType) {
          datatypes = "INT";
        } else if (l->dTypes == voidDType) {
          datatypes = "VOID";
        }

        fprintf(listing,"%-7s  ",id);
        fprintf(listing,"%-9s  ",datatypes);

        while (t != NULL) {
          fprintf(listing,"%3d ",t->lineno);
          t = t->next;
        }

        fprintf(listing, "\n");
        l = l->next;
      }
    }
  }
}

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
