#ifndef _WIN32
#define threadReturn void*
typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int jobFlag;
} BSemaphore;

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int slots[10];
} Semaphore;

// Binary Semaphore
void bSemaphoreInit(BSemaphore *jobWait);

void bSemaphoreSignal(BSemaphore *jobWait);

void bSemaphoreWait(BSemaphore *jobWait);

// Array Semaphore
void semaphoreInit(Semaphore *slotWait);

void semaphoreOpen(Semaphore *slotWait);

void semaphoreSignal(Semaphore *slotWait, int slot);

void semaphoreWait(Semaphore *slotWait, int slot);

#else
#define threadReturn DWORD WINAPI

typedef struct {
	HANDLE mutex;
	HANDLE semaphore;
	int slots[10];
} Semaphore;

// Binary Semaphore
void bSemaphoreWait(HANDLE *jobWait);

void bSemaphoreSignal(HANDLE *jobWait);

void semaphoreInit(Semaphore *slotWait);

void semaphoreSignal(Semaphore *slotWait, int slot);

void semaphoreWait(Semaphore *slotWait, int slot);

#endif
