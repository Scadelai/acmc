# Variáveis de compilação e ferramentas
CC         = gcc
CFLAGS     = -Wall -g
LEX        = flex
BISON      = bison
BISONFLAGS = -d
LDFLAGS    = -ll

# Nome do executável final
TARGET     = acmc

# Arquivos gerados
BISON_C    = acmc.tab.c
BISON_H    = acmc.tab.h
LEX_C      = lex.yy.c

# Arquivos objeto
OBJS       = symbol_table.o

# Regra padrão: gera o executável final
all: $(TARGET)

# Regra para gerar o executável
$(TARGET): $(BISON_C) $(LEX_C) $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(BISON_C) $(LEX_C) $(OBJS) $(LDFLAGS)

# Regra para gerar os arquivos do Bison a partir do acmc.y
$(BISON_C) $(BISON_H): acmc.y
	$(BISON) $(BISONFLAGS) acmc.y

# Regra para gerar o arquivo do Flex a partir do acmc.l
$(LEX_C): acmc.l $(BISON_H)
	$(LEX) acmc.l

# Compila o módulo da tabela de símbolos
symbol_table.o: symbol_table.c symbol_table.h
	$(CC) $(CFLAGS) -c symbol_table.c

# Limpa os arquivos gerados
clean:
	rm -f $(TARGET) $(BISON_C) $(BISON_H) $(LEX_C) $(OBJS)
