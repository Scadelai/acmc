#include "globals.h"
#include "util.h"
#include "parse.h"
#include "analyze.h"
#include "codegen.h"

// Incluir stdio e string para operações com arquivos e strings
#include <stdio.h>
#include <string.h>

FILE *source;
FILE *listing;
int lineno = 0;
int Error = FALSE;

// Função para comparar dois arquivos linha a linha
int compareFiles(const char *file1, const char *file2) {
    FILE *f1 = fopen(file1, "r");
    FILE *f2 = fopen(file2, "r");
    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return 0;
    }
    
    char buf1[256], buf2[256];
    int equal = 1;
    while (1) {
        char *s1 = fgets(buf1, sizeof(buf1), f1);
        char *s2 = fgets(buf2, sizeof(buf2), f2);
        if (s1 == NULL && s2 == NULL)
            break;
        if (s1 == NULL || s2 == NULL || strcmp(buf1, buf2) != 0) {
            equal = 0;
            break;
        }
    }
    fclose(f1);
    fclose(f2);
    return equal;
}

int main(int argc, char *argv[]) {
  TreeNode *syntax_tree;
  char filename[100];

  // Verifica se o número de argumentos está correto
  if (argc != 2) {
    fprintf(stderr, "try: %s <filename>\n", argv[0]);
    return 1;
  }

  // Copia o nome do arquivo informado
  strcpy(filename, argv[1]);
  // Se o nome do arquivo não tiver extensão, adiciona ".c-"
  if (strchr(filename, '.') == NULL) {
    strcat(filename, ".c-");
  }

  // Abre o arquivo de origem para leitura
  source = fopen(filename, "r");
  if (source == NULL) {
    fprintf(stderr, "File %s not found\n", filename);
    return 1;
  }

  listing = stdout;
  
  // Realiza a análise sintática e imprime a árvore sintática
  syntax_tree = parse();
  fprintf(listing, "\nSyntax tree:\n\n");
  print_tree(syntax_tree);

  // Se não houver erros, constrói a tabela de símbolos e gera o código intermediário
  if (!Error) {
    build_symbol_table(syntax_tree);
    // Chama a função para gerar o código intermediário; 
    // esta função deve gravar a saída em "output.ir"
    codeGen(syntax_tree, "output.ir");
  }

  fclose(source);
  return 0;
}
