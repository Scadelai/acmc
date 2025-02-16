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

  if (argc != 2) {
    fprintf(stderr, "try: %s <filename>\n", argv[0]);
    return 1;
  }

  strcpy(filename, argv[1]);
  if (strchr(filename, '.') == NULL) {
    strcat(filename, ".c-");
  }

  source = fopen(filename, "r");
  if (source == NULL) {
    fprintf(stderr, "File %s not found\n", filename);
    return 1;
  }

  listing = stdout;
  
  syntax_tree = parse();
  fprintf(listing, "\nSyntax tree:\n\n");
  print_tree(syntax_tree);

  if (!Error) {
    build_symbol_table(syntax_tree);
  }

  fclose(source);
  return 0;
}
