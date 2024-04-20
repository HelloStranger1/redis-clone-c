CC=gcc
CFLAGS=-I. -g -Wall
DEPS = parser.h utils.h data.h table.h resp_handler.h server.h 
OBJ = parser.o data.o table.o utils.o resp_handler.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
server: server.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	

main: main.o $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

