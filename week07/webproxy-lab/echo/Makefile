CC = gcc
CFLAGS = -O2 -Wall -I .
LIB = -lpthread

all: client server

client: client.c csapp.o
	$(CC) $(CFLAGS) -o client client.c csapp.o $(LIB)

server: server.c csapp.o
	$(CC) $(CFLAGS) -o server server.c csapp.o $(LIB)

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.o client server *~
