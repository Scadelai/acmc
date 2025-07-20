CC = gcc
BIN = acmc
BINARY_GEN = binary_generator
OBJS = acmc.tab.o lex.yy.o analyze.o symtab.o util.o main.o codegen.o assembly.o binary_generator.o

all: $(BIN) $(BINARY_GEN)

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS) -lfl

$(BINARY_GEN): binary_generator_standalone.o binary_generator.o
	$(CC) -o $(BINARY_GEN) binary_generator_standalone.o binary_generator.o

binary_generator_standalone.o: binary_generator_standalone.c
	$(CC) -c binary_generator_standalone.c

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
	-rm -f $(BINARY_GEN)

check:
	valgrind --leak-check=full ./acmc
