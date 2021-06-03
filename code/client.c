#include "common.h"

// might remove from global and just pass pointer later.
struct SharedMemory *sharedMemory;
#ifndef _WIN32
// Last update
struct timespec last;

// Just to print when they are done
typedef struct {
	int active[10];
	struct timespec start[10];
	unsigned int number[10];
} Query;

#else
LARGE_INTEGER freq;
// Last update
LARGE_INTEGER last;

// Just to print when they are done
typedef struct {
	int active[10];
	LARGE_INTEGER start[10];
	unsigned int number[10];
} Query;
#endif
// Globals
Query queries;
// Progress flag for progress bar updates.
int progress = 0;
// Flag to say if test mode is running.
int testModeFlag = 0;
// maxLineLength to clear line for user to enter
int maxLineLength = 1;

#ifndef _WIN32
// Quitter handler on unix dont do this on windows because its a lot more to deal with.
void quitter(int sig) {
	signal(sig, SIG_IGN);
	printf("Shutting down...\n");
	while (sharedMemory->clientflag != DONE) {
		// Waiting until server is ready
	}
	sharedMemory->clientflag = 2;
	shmdt((void*)sharedMemory);
	exit(0);
}
#endif

// Updates if progress is still active and uses progress flag to tell.
void updateProgress() {
	int test = 0;
	for (int i = 0; i < 10; i++) {
		if (queries.active[i] == 1) {
			test = 1;
			break;
		}
	}
	progress = test;
	if (progress == 0) {
		maxLineLength = 1;
		testModeFlag = 0;
	}
}

// Setup the query
void query(unsigned int number) {
	int slot;
	// wait for server to be ready
	while (sharedMemory->clientflag != DONE) {
		// Waiting until server is ready
	}
	sharedMemory->number = number;
	sharedMemory->clientflag = 1;
	// wati for server to say what slot
	while (sharedMemory->clientflag != DONE) {
		// Waiting until server is ready
	}
	slot = sharedMemory->number;
	queries.active[slot] = 1;
	queries.number[slot] = number;
	// Start timers
	#ifndef _WIN32
	clock_gettime(CLOCK_MONOTONIC, &(queries.start[slot]));
	#else
	QueryPerformanceCounter(&(queries.start[slot]));
	#endif
	progress = 1;
	testModeFlag = 0;
	// recieve info from slot
}

void testMode() {
	while (sharedMemory->clientflag != DONE) {
		// Waiting until server is ready
	}
	sharedMemory->number = 0;
	sharedMemory->clientflag = UPDATE;
	// wati for server to say what slot
	while (sharedMemory->clientflag != DONE) {
		// Waiting until server is ready
	}
	//assume first three slots
	queries.active[0] = 1;
	queries.active[1] = 1;
	queries.active[2] = 1;
	queries.number[0] = 0;
	queries.number[1] = 0;
	queries.number[2] = 0;
	#ifndef _WIN32
	clock_gettime(CLOCK_MONOTONIC, &(queries.start[0]));
	clock_gettime(CLOCK_MONOTONIC, &(queries.start[1]));
	clock_gettime(CLOCK_MONOTONIC, &(queries.start[2]));
	#else
	QueryPerformanceCounter(&(queries.start[0]));
	QueryPerformanceCounter(&(queries.start[1]));
	QueryPerformanceCounter(&(queries.start[2]));
	#endif
	progress = 1;
}

// used to not allow more than 10 queries
int checkSlots() {
	for (int i = 0; i < 10; i++) {
		if (queries.active[i] == 0) {
			return 1;
		}
	}
	return 0;
}

// to clear x ammount of characters because windows doesnt like \30[2k thingo
// Big note that if 10 queires are running it over prints and does weird behaviour
void deleteLine(int length) {
	char test[DEFAULT_BUFLEN] = {0};
	//for (int i = 0; i < length; i++)
		printf("\r");
	for (int i = 0; i < length; i++)
		test[i] = ' ';
	printf("%s", test);
	//for (int i = 0; i < length; i++)
		printf("\r");
}


int main(int argc, char* argv[]) {
	char input[DEFAULT_BUFLEN], output[DEFAULT_BUFLEN];
	// make sure all queires arent active
	for (int i = 0; i < 10; i++) {
		queries.active[i] = 0;
	}

	#ifndef _WIN32
	// shared mem ids
	key_t sharedMemoryKey;
	int sharedMemoryID;

	// setup ctrl+c handler on unix
	signal(SIGINT, quitter);

	// Create shared memory key from file. using an example params because they work well
	sharedMemoryKey = ftok(".", 'x');
	sharedMemoryID = shmget(sharedMemoryKey, sizeof(struct SharedMemory), IPC_CREAT | 0666);
	// Error check.
	if (sharedMemoryID < 0) {
		printf("ERROR: Failed to setup shared memory\n");
		exit(1);
	}
	// Set variable to reference shared memory.
	sharedMemory = (struct SharedMemory *)shmat(sharedMemoryID, NULL, 0);
	#else
	//---------------------------windows----------------------------
	//setup the timer freq for later
	QueryPerformanceFrequency(&freq);

	//Windows shared memory stuff
	HANDLE hMapFile;
	char* szName = "Global\\SharedMemFile";
	hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct SharedMemory), szName);
	if (hMapFile == NULL)
	{
		printf("Could not create file mapping object (%d).\n", GetLastError());
		return 1;
	}
	sharedMemory = (struct SharedMemory *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct SharedMemory));
	//---------------------------windows----------------------------
	#endif
	// Do work.
	#ifndef _WIN32
	//make terminal non blocking on unix
	nonblock(NB_ENABLE);
	#endif
	while (1) {
		// Get user input.
		// use non blocking report
		if (kbhit()) {
			// Block so user can acually enter a number
			char c = fgetc(stdin);
			int index = 0;
			unsigned int inputNumber = 0;
			//clear line and display input
			deleteLine(maxLineLength);
			printf("%c", c);
			// if q is pressed first quit. tbh really didnt want to waste time error checking all inputs
			if (c == 'q') {
				break;
			}
			while (c != '\n' && c != 13) {
				input[index++] = c;
				c = fgetc(stdin);
				#ifdef _WIN32
				// Because windows is lame.
				printf("%c", c);
				#endif
			}
			input[index] = '\0';
			// convert input to number again did want to bother making sure only numbers were entered
			// so if no numbers were entered it defaults to 0. atol()
			
			inputNumber = atol(input);
			// Check if 0 was entered for test mode
			if (testModeFlag == 0) {
				if (inputNumber == 0) {
					if (progress == 0) {
						printf("Test Mode starting\n");
						testModeFlag = 1;
						testMode();
					}
				} else  {
					if (checkSlots()) {
						printf("Starting query\n");
						query(inputNumber);
					} else {
						printf("System busy\n");
					}
				}
			}

		} else {
			// check if updates need to be printed ie factors or progress.
			if (progress == 1) {
				// Check active slots
				for (int i = 0; i < 10; i++) {
					if (queries.active[i] == 1) {
						unsigned int factor;
						if (sharedMemory->serverflag[i] == UPDATE) {
							factor = sharedMemory->slot[i];
							sharedMemory->serverflag[i] = DONE;
							// check if query is done.
							if (sharedMemory->progress[i] == 100.0 && factor == 0) {
								// could user negative error codes
								queries.active[i] = 0;
								sharedMemory->progress[i] = 0.0;
								double timeDiff = 0.0;
								// end time for a query.
								#ifndef _WIN32
								struct timespec end;
								clock_gettime(CLOCK_MONOTONIC, &end);
								timeDiff = (end.tv_sec - queries.start[i].tv_sec) * 1000.0;
								timeDiff += (end.tv_nsec - queries.start[i].tv_nsec) / 1000000.0;
								#else
								LARGE_INTEGER end;
								QueryPerformanceCounter(&end);
								timeDiff = (end.QuadPart - queries.start[i].QuadPart) * 1000.0 / freq.QuadPart;
								#endif
								deleteLine(maxLineLength);
								printf("Query #%d Done. With number %u Took:%lfms\n", i+1, queries.number[i], timeDiff);
								updateProgress();
								continue;
							}
							// if its not done then print factor
							deleteLine(maxLineLength);
							printf("Factor: %u\n", factor);
							// update last time a factor was recieved.
							#ifndef _WIN32
							clock_gettime(CLOCK_MONOTONIC, &last);
							#else
							QueryPerformanceCounter(&last);
							#endif
						}
					}
				}
				double timeDiff = 0.0;
				// Get time since last factor was received
				#ifndef _WIN32
				struct timespec end;
				clock_gettime(CLOCK_MONOTONIC, &end);
				timeDiff = (end.tv_sec - last.tv_sec) * 1000.0;
				timeDiff += (end.tv_nsec - last.tv_nsec) / 1000000.0;
				#else
				LARGE_INTEGER end;
				QueryPerformanceCounter(&end);
				timeDiff = (end.QuadPart - last.QuadPart) * 1000.0 / freq.QuadPart;
				#endif
				// if time > 500 ms print update
				if (timeDiff > 200 && testModeFlag == 0) {
					if (progress == 1) {
						// build progress bar output.
						strcpy(output, "Progress: ");
						for (int i = 0; i < 10; i++) {
							if (queries.active[i] == 1) {
								char temp[256];
								memset(temp, '\0', 256);
								sprintf(temp, " Q%d:%3.2lf%%", i+1, sharedMemory->progress[i]);
								int t = (int)sharedMemory->progress[i]/10;
								int len = strlen(temp);
								temp[len++] = '[';
								for (int j = 0; j < t; j++)
									temp[len++] = '#';
								for (int j = 0; j < 10-t; j++)
									temp[len++] = '_';
								temp[len++] = ']';
								strcat(output, temp);

							}
						}
						// delete line and print output
						deleteLine(maxLineLength);
						printf("%s", output);
						// since no '\n' need to force flush stdin
						fflush(0);
						maxLineLength = strlen(output) + 1;
						//update time so it doesnt spam progress more than every half a second
						#ifndef _WIN32
						clock_gettime(CLOCK_MONOTONIC, &last);
						#else
						QueryPerformanceCounter(&last);
						#endif
					}
				}
			}
		}
	}
	// Detach shared memory. and close server
	while (sharedMemory->clientflag != DONE) {
		// Waiting until server is ready
	}
	sharedMemory->clientflag = 2;
	#ifndef _WIN32
	shmdt((void*)sharedMemory);
	#else
	UnmapViewOfFile(sharedMemory);
	CloseHandle(hMapFile);
	#endif
	return 0;
}
