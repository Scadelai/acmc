#include "globals.h"
#include "util.h"
#include "parse.h"
#include "analyze.h"

int lineno = 0;
FILE *source;
FILE *listing;
int Error = FALSE;

int main(int argc, char *argv[]) {
  TreeNode* syntaxTree;
  char pgm[120];

  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename>\n", argv[0]);
    exit(1);
  }

  strcpy(pgm, argv[1]);
  if (strchr(pgm, '.') == NULL) {
    strcat(pgm, ".cm");
  }

  source = fopen(pgm, "r");
  if (source == NULL) {
    fprintf(stderr, "File %s not found\n", pgm);
    exit(1);
  }

  listing = stdout;
  fprintf(listing, "\nacmc - Another C- Compiler\n\n");

  syntaxTree = parse();
  fprintf(listing, "\nSyntax tree:\n\n");
  printTree(syntaxTree);

  if (!Error) {
    buildSymtab(syntaxTree);
  }

  fclose(source);
  return 0;
}
