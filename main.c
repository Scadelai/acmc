#include "globals.h"
#include "util.h"
#include "parse.h"
#include "analyze.h"

FILE *source;
FILE *listing;
int lineno = 0;
int Error = FALSE;

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

  // Se não houver erros, constrói a tabela de símbolos
  if (!Error) {
    build_symbol_table(syntax_tree);
  }

  fclose(source);
  return 0;
}
