# Author: Javier Izquierdo Hernandez (javizqh@gmail.com)
# Also: j.izquierdoh.2021@alumnos.urjc.es
# Date: 2023-10-18

# If compile with debug change to this $(CC) -c -DDEBUG -o $@ $< $(CFLAGS)

CC=gcc
CFLAGS=-I.
DEPS =

%.o: %.c $(DEPS)
	$(CC) -g -c -o $@ $< $(CFLAGS)

all: stub.o server client

server: server.c 
	gcc -g -Wall -o server server.c -lpthread stub.o

client: client.c 
	gcc -g -Wall -o client client.c -lpthread stub.o

clean:
	rm -f *.o client server
