#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

typedef struct Symbol {
    char *name;
    char *type;
    char *scope;         /* Escopo em que o identificador foi declarado (por exemplo, o nome da função) */
    struct Symbol *next;
} Symbol;

void initSymbolTable();
void addSymbol(char *name, char *type, char *scope, int line);
int existsSymbol(char *name, char *scope);
void printSymbolTable();
void freeSymbolTable();

#endif
