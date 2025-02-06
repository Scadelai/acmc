#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

Symbol *symbolTable = NULL;

void initSymbolTable() {
    symbolTable = NULL;
}

void addSymbol(char *name, char *type, char *scope, int line) {
    /* Verifica se o símbolo já foi declarado no mesmo escopo */
    Symbol *current = symbolTable;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0 && strcmp(current->scope, scope) == 0) {
            printf("ERRO SEMÂNTICO: %s já declarado na mesma função em linha %d\n", name, line);
            return;
        }
        current = current->next;
    }
    /* Cria novo símbolo e o adiciona à lista */
    Symbol *newSymbol = (Symbol*) malloc(sizeof(Symbol));
    newSymbol->name = strdup(name);
    newSymbol->type = strdup(type);
    newSymbol->scope = strdup(scope);
    newSymbol->next = symbolTable;
    symbolTable = newSymbol;
}

int existsSymbol(char *name, char *scope) {
    Symbol *current = symbolTable;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0 && strcmp(current->scope, scope) == 0)
            return 1;
        current = current->next;
    }
    return 0;
}

void printSymbolTable() {
    printf("Tabela de Símbolos:\n");
    Symbol *current = symbolTable;
    while (current != NULL) {
        printf("Nome: %s, Tipo: %s, Escopo: %s\n", current->name, current->type, current->scope);
        current = current->next;
    }
}

void freeSymbolTable() {
    Symbol *current = symbolTable;
    while (current != NULL) {
        Symbol *temp = current;
        current = current->next;
        free(temp->name);
        free(temp->type);
        free(temp->scope);
        free(temp);
    }
}
