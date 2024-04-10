CC=gcc
CFLAGS=-I. -g -Wall
DEPS = parser.h common.h data.h table.h 
OBJ = parser.o data.o table.o common.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
server: server.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
client2: client2.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
main: main.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

