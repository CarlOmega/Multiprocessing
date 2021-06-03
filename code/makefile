all: Server Client

Server: server.c common.c common.h semaphore.c semaphore.h
	gcc server.c common.c semaphore.c -o Server -g
Client: client.c common.c common.h semaphore.c semaphore.h
	gcc client.c common.c semaphore.c -o Client -g
