#include "common.h"

// Global shared memeory
struct SharedMemory *sharedMemory;


//Custom structs
// this struct defines a job and enable progress reporting for the server
typedef struct {
	unsigned int number;
	int slot;
	int progress;
	int complete;
} Job;
// this defines a que linked list object that points to a job
typedef struct Que {
	Job* job;
	struct Que* nextQue;
} Que;
// this defines the queries that point to all the jobs
typedef struct {
	Job *jobs[32];
	unsigned int number;
	int slot;
	int active;
} Query;
// this is for a test job since it only uses 10 jobs per request not 32 so went at it a different way
typedef struct {
	int slot;
	int progress;
	int complete;
	int magnitude;
} testJob;

// Global job que and query structures
static Query** queries;
static Que *que = NULL;

// Mutex and semaphore setup.
#ifndef _WIN32
key_t sharedMemoryKey;
int sharedMemoryID;

BSemaphore *jobWait;
pthread_mutex_t threadPoolLock;
Semaphore *slotWait;

// quitter for ctrl+c on unix because windows is a pain and assignment sheet didnt say windows
void quitter(int sig) {
	signal(sig, SIG_IGN);
	printf("Shutting down server...\n");
	shmdt((void*)sharedMemory);
	// Clean up shared memory.
	shmctl(sharedMemoryID, IPC_RMID, NULL);
	exit(0);
}
#else
// windows semaphores and mutexs HANDLE was how the ms docs had it but I would of liked type defs instead
HANDLE *jobWait;
HANDLE *jobWaitMutex;
HANDLE threadPoolLock;

Semaphore *slotWait;

#endif

// This function is the thread function fo the test mode.
// Note: "threadReturn" is a preprocessor because windows threads use different return type.
threadReturn testworker(void* arg) {
	//job is pasted in with testing and doesnt use a que because it is testing sync not thread pool
	testJob* job = (testJob*)arg;
	for (int i = (10*job->magnitude); i < (10*job->magnitude)+10; i++) {
		//random wait
		int delay = rand()%91+10;

		#ifndef _WIN32
		usleep(delay*1000);
		#else
		Sleep(delay);
		#endif
		// wait for sharedMemoryslot to be free
		semaphoreWait(slotWait, job->slot);
		// Critical start
		while (sharedMemory->serverflag[job->slot] == 1) {
			// Wait on client to read the factor.
		}
		sharedMemory->slot[job->slot] = i;
		sharedMemory->serverflag[job->slot] = 1;
		// tell other threads that slot is open using semaphore
		semaphoreSignal(slotWait, job->slot);
		// update progress on test not that its used because the test is too quick.
		job->progress = (i%10)*100/10;
	}
	// set the job status to complete
	job->progress = 100;
	job->complete = 1;
	return 0;
}


// This function is the worker thread function using a dispatch que driven thread pool.
// I modeled it after the lab reference on syncronisation. it waits until signaled that
// there is jobs then carefully takes job off que using mutex to make sure nothing breaks
// then proceeds to complete the job. if there is no jobs it waits for a signal.
threadReturn worker(void* arg) {
	Job* currentJob = NULL;
	int id = *((int*)arg);
	while (1) {
		// Wait until server says there is a job.
		bSemaphoreWait(jobWait);
		// lock mutex to take job off que
		#ifndef _WIN32
		pthread_mutex_lock(&threadPoolLock);
		#else
		WaitForSingleObject(threadPoolLock, INFINITE);
		#endif
		// Get job
		if (que != NULL) {
			//there is a job waiting take it
			Que* temp = que;
			currentJob = temp->job;
			que = temp->nextQue;
		} else {
			// no jobs
			currentJob = NULL;
		}
		// unlock mutex to allow other thread to get jobs.
		#ifndef _WIN32
		pthread_mutex_unlock(&threadPoolLock);
		#else
		ReleaseMutex(threadPoolLock);
		#endif
		// Do job if there is one
		if (currentJob != NULL) {
			// Debug statment just to see if they took a job correctly.
			printf("Doing job 2->%u(%u/2)\n", currentJob->number/2, currentJob->number);
			for (unsigned long i = 2; i <= (currentJob->number/2); i++) {
				if (currentJob->number%i==0) {
					// write to appropriate slot.
					semaphoreWait(slotWait, currentJob->slot);
					// Critical start
					while (sharedMemory->serverflag[currentJob->slot] == 1) {
						// Wait on client to read the factor.
					}
					//printf("Number: %lu Factor: %lu id: %d slot:%d\n", currentJob->number, i, id, currentJob->slot);
					sharedMemory->slot[currentJob->slot] = i;
					sharedMemory->serverflag[currentJob->slot] = 1;
					// signal that slot is open
					semaphoreSignal(slotWait, currentJob->slot);
				}
				currentJob->progress = (i*100LL)/(currentJob->number/2);
			}
			currentJob->progress = 100;
			currentJob->complete = 1;
			// signal that job is done to wake any threads that didnt get a job from server signal
			bSemaphoreSignal(jobWait);
		}
	}
	return 0;
}

// sets up the queires to be default values
Query** setupQueriesSlots() {
	//create Initial queries struct
	Query** queries = (Query**)malloc(10*sizeof(Query));
	for (int j = 0; j < 10; j++) {
		queries[j] = malloc(sizeof(Query));
		queries[j]->slot = -1;
		queries[j]->number = -1;
		queries[j]->active = 0;
		for (int i = 0; i < 32; i++) {
			queries[j]->jobs[i] = malloc(sizeof(Job));
			queries[j]->jobs[i]->slot = -1;
			queries[j]->jobs[i]->complete = 0;
			queries[j]->jobs[i]->progress = 0;
			queries[j]->jobs[i]->number = 0;
		}
	}

	return queries;
}

// finds if there is a free slot on the server. else return -1
int getFreeSlot(Query** queries) {
	int slot = -1;
	for (int i = 0; i < 10; i++) {
		if (queries[i]->active == 0) {
			return i;
		}
	}
	return -1;
}

// Initial setup for the server.
int main(int argc, char* argv[]) {
	int numberThreads;

	if (argc == 2) {
		// ***Validate that thread number isnt huge
		numberThreads = atoi(argv[1]);
		if (numberThreads < 1 || numberThreads > 1000) {
			perror("Please make sure number of threads is a non zero number between 1-1000\n");
			exit(0);
		}
	} else if (argc == 1) {
		numberThreads = 320;
	} else {
		perror("Please make sure using format: 'filename <number of threads>'\n");
		exit(0);
	}
	#ifndef _WIN32

	signal(SIGINT, quitter);

	// Create shared memory key from file
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
	// Shared memory setup
	sharedMemory->clientflag = 0;
	for (int i = 0; i < 10; i++) {
		sharedMemory->serverflag[i] = 0;
		sharedMemory->progress[i] = 0.0;
	}

	#ifndef _WIN32
	//unix semaphore
	jobWait = (BSemaphore *)malloc(sizeof(BSemaphore));
	bSemaphoreInit(jobWait);
	slotWait = (Semaphore *)malloc(sizeof(Semaphore));
	semaphoreInit(slotWait);
	#else
	//win32 semaphore
	jobWait = (HANDLE *)malloc(sizeof(HANDLE));
	*jobWait = CreateSemaphore(NULL, 0, 1, NULL);
	slotWait = (Semaphore *)malloc(sizeof(Semaphore));
	semaphoreInit(slotWait);
	threadPoolLock = CreateMutex(NULL, FALSE, NULL);

	#endif
	// Setup queries
	queries = setupQueriesSlots();

	#ifndef _WIN32
	// Start thread pool
	pthread_t thread;
	for (int i = 0; i < numberThreads; i++) {
		int* j = (int*)malloc(sizeof(int));
		*j = i;
		pthread_create(&thread, NULL, worker, (void*)j);
		pthread_detach(thread);
	}
	#else
	//win32 thread pool
	for (int i = 0; i < numberThreads; i++) {
		int* j = (int*)malloc(sizeof(int));
		*j = i;
		CreateThread(NULL, 0, worker, (void *)j, 0, NULL);
	}

	#endif
	// Do work.
	while (1) {

		if (sharedMemory->clientflag == 1) {
			// Go job.
			unsigned int number = sharedMemory->number;
			//printf("Got number %lu\n", sharedMemory->number);
			// Create jobs from number
				// Get free slot
			if (number == 0) {
				sharedMemory->clientflag = 0;
				testJob* jobs[3][10];
				int activeJob[3] = {1, 1, 1};
				for (int i = 0; i < 3; i++) {
					#ifndef _WIN32
					pthread_t testThread;
					#endif
					for (int j = 0; j < 10; j++) {
						testJob* tj = (testJob*)malloc(sizeof(testJob));
						tj->magnitude = j;
						tj->slot = i;
						tj->progress = 0;
						tj->complete = 0;
						jobs[i][j] = tj;
						#ifndef _WIN32
						pthread_create(&testThread, NULL, testworker, (void*)tj);
						pthread_detach(testThread);
						#else
						CreateThread(NULL, 0, testworker, (void *)tj, 0, NULL);
						#endif
					}
				}
				//test if done or not and update progress
				int testProgress = 0;
				do {
					testProgress = 1;
					for (int i = 0; i < 3; i++) {
						if (activeJob[i] == 1) {
							testProgress = 0;
							int total = 0;
							double totalProgress = 0.0;
							for (int j = 0; j < 10; j++) {
								if (jobs[i][j]->complete) {
									total++;
								}
								totalProgress += jobs[i][j]->progress;
							}
							totalProgress /= 10;
							if (total == 10) {
								activeJob[i] = 0;
								printf("Query %d done...\n", i);
								sharedMemory->progress[i] = 100.0;
								// Signal to the client that query is done.
								semaphoreWait(slotWait, i);
								// Critical start
								while (sharedMemory->serverflag[i] == 1) {
									// Wait on client to read the factor.
								}
								sharedMemory->slot[i] = 0;
								sharedMemory->serverflag[i] = 1;
								semaphoreSignal(slotWait, i);
							} else {
								sharedMemory->progress[i] = totalProgress;
							}
						}
					}
				} while (testProgress != 1);


			} else {
				int slot = getFreeSlot(queries);
				if (slot != -1) {
					// setup query with request.
					queries[slot]->active = 1;
					queries[slot]->slot = slot;
					queries[slot]->number = number;
					sharedMemory->number = (unsigned int)slot;
					sharedMemory->clientflag = 0;
					printf("Starting jobs on number %u...\n", number);
					for (int i = 0; i < 32; i++) {
						#ifndef _WIN32
						pthread_mutex_lock(&threadPoolLock);
						#else
						WaitForSingleObject(threadPoolLock, INFINITE);
						#endif
						queries[slot]->jobs[i]->slot = queries[slot]->slot;
						unsigned int temp = queries[slot]->number << (32-i);
						queries[slot]->jobs[i]->number = (queries[slot]->number >> i) | temp;

						queries[slot]->jobs[i]->complete = 0;
						queries[slot]->jobs[i]->progress = 0;
						// lock threads so they dont take off jobs while added one.

						// create new job
						Que* newJob = (Que*)malloc(sizeof(Que));
						newJob->job = queries[slot]->jobs[i];
						newJob->nextQue = NULL;
						if (que == NULL) {
							que = newJob;
						} else {
							Que* temp = que;
							while (temp->nextQue != NULL) {
								temp = temp->nextQue;
							}
							temp->nextQue = newJob;
						}
						#ifndef _WIN32
						pthread_mutex_unlock(&threadPoolLock);
						#else
						ReleaseMutex(threadPoolLock);
						#endif
						bSemaphoreSignal(jobWait);
						// Signal threads to start
					}
				}
			}
		} else if (sharedMemory->clientflag == 2) {
			printf("Shutting down from client...\n");
			#ifndef _WIN32
			shmdt((void*)sharedMemory);
			// Clean up shared memory.
			shmctl(sharedMemoryID, IPC_RMID, NULL);
			#endif
			exit(0);
		} else {
			// update progress on all active queries
			for (int i = 0; i < 10; i++) {
				if (queries[i]->active) {
					int total = 0;
					double totalProgress = 0.0;
					//printf("Progress of %d : ", i);
					for (int j = 0; j < 32; j++) {
						if (queries[i]->jobs[j]->complete){
							//printf("|100|");
							total++;
						} else {
							//printf("|%3d %10u|", queries[i]->jobs[j]->progress, queries[i]->jobs[j]->number);
						}
						totalProgress += queries[i]->jobs[j]->progress;
						//printf("%d ", queries[i]->jobs[j]->progress);
					}
					totalProgress /= 32.0;
					//printf("%d/32\r", total);
					//fflush(0);
					//printf("\n");
					if (total == 32) {
						queries[i]->active = 0;
						printf("Query %d done...\n", i);
						sharedMemory->progress[i] = 100.0;
						// Signal to the client that query is done.
						semaphoreWait(slotWait, i);
						// Critical start
						while (sharedMemory->serverflag[i] == 1) {
							// Wait on client to read the factor.
						}
						sharedMemory->slot[i] = 0;
						sharedMemory->serverflag[i] = 1;
						semaphoreSignal(slotWait, i);
					} else {
						sharedMemory->progress[i] = totalProgress;
					}

				}

			}
		}
	}
	#ifndef _WIN32
	// Detach shared memory.
	shmdt((void*)sharedMemory);

	// Clean up shared memory.
	shmctl(sharedMemoryID, IPC_RMID, NULL);
	#endif
	return 0;
}
