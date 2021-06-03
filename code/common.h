#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//for handling ctrl+c
#include <signal.h>


#ifndef _WIN32
	#include <unistd.h>
	#include <sys/sem.h>
	#include <pthread.h>

	// Shared Memory
	#include <sys/types.h>
	#include <sys/ipc.h>
	#include <sys/shm.h>
#else
	#include <windows.h>
	#include <conio.h>

	#define fgetc(stdin) _getch()
#endif

#include "semaphore.h"

#define DEFAULT_BUFLEN 1024

#define STDIN_FILENO 0
#define NB_ENABLE 1
#define NB_DISABLE 0

#define UPDATE 1
#define DONE 0

struct SharedMemory {
	char clientflag;
	unsigned int number;
	char serverflag[10];
	unsigned int slot[10];
	double progress[10];
};

#ifndef _WIN32
int kbhit();
void nonblock(int state);
#endif
