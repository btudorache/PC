CFLAGS = -Wall -g
CC=gcc

PORT = 10099
IP_SERVER = 127.0.0.1

all: server subscriber

server: server.c queue.c list.c
	${CC} ${CFLAGS} server.c queue.c list.c -o server

subscriber: subscriber.c queue.c list.c
	${CC} ${CFLAGS} subscriber.c queue.c list.c -o subscriber -lm

.PHONY: clean run_server

run_server:
	./server ${PORT}

clean:
	rm -f server subscriber
