CC = gcc
BIN = acmc
OBJS = acmc.tab.o lex.yy.o analyze.o symtab.o util.o main.o codegen.o assembly.o binary_generator.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS)

lex.yy.o: acmc.l
	flex acmc.l
	$(CC) -c lex.yy.c

acmc.tab.o: acmc.y
	bison -d acmc.y
	$(CC) -c acmc.tab.c

clean:
	-rm -f acmc.tab.c
	-rm -f acmc.tab.h
	-rm -f lex.yy.c
	-rm -f $(OBJS)
	-rm -f binary_generator_standalone.o
	-rm -f $(BIN)
	-rm -f *.o
	-rm -f *.bin
	-rm -f *.binbd
	-rm -f *.asm
	-rm -f *.ir


check:
	valgrind --leak-check=full ./acmc
