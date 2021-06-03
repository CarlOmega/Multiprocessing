#include "common.h"
// Binary Semaphore
#ifndef _WIN32
void bSemaphoreInit(BSemaphore *jobWait) {
	pthread_mutex_init(&(jobWait->mutex), NULL);
	pthread_cond_init(&(jobWait->cond), NULL);
	jobWait->jobFlag = 0;
}

void bSemaphoreSignal(BSemaphore *jobWait) {
	pthread_mutex_lock(&jobWait->mutex);
	jobWait->jobFlag++;
	pthread_cond_signal(&jobWait->cond);
	pthread_mutex_unlock(&jobWait->mutex);
}

void bSemaphoreWait(BSemaphore *jobWait) {
	pthread_mutex_lock(&jobWait->mutex);
	while (jobWait->jobFlag < 1)
		pthread_cond_wait(&jobWait->cond, &jobWait->mutex);
	jobWait->jobFlag--;
	pthread_mutex_unlock(&jobWait->mutex);
}

// Array Semaphore
void semaphoreInit(Semaphore *slotWait) {
	pthread_mutex_init(&(slotWait->mutex), NULL);
	pthread_cond_init(&(slotWait->cond), NULL);
	for (int i = 0; i < 10; i++)
		slotWait->slots[i] = 1;
}

void semaphoreOpen(Semaphore *slotWait) {
	pthread_mutex_lock(&slotWait->mutex);
	for (int i = 0; i < 10; i++)
		slotWait->slots[i] = 1;
	pthread_cond_signal(&slotWait->cond);
	pthread_mutex_unlock(&slotWait->mutex);
}

void semaphoreSignal(Semaphore *slotWait, int slot) {
	pthread_mutex_lock(&slotWait->mutex);
	slotWait->slots[slot] = 1;
	pthread_cond_signal(&slotWait->cond);
	pthread_mutex_unlock(&slotWait->mutex);
}

void semaphoreWait(Semaphore *slotWait, int slot) {
	pthread_mutex_lock(&slotWait->mutex);
	while (slotWait->slots[slot] == 0)
		pthread_cond_wait(&slotWait->cond, &slotWait->mutex);
	slotWait->slots[slot] = 0;
	pthread_mutex_unlock(&slotWait->mutex);
}
#else

void bSemaphoreWait(HANDLE *jobWait) {
    WaitForSingleObject(*jobWait, INFINITE);
}

void bSemaphoreSignal(HANDLE *jobWait) {
	ReleaseSemaphore(*jobWait, 1, NULL);
}

void semaphoreInit(Semaphore *slotWait) {
	slotWait->semaphore = CreateSemaphore(NULL, 0, 1, NULL);
	slotWait->mutex = CreateMutex(NULL, FALSE, NULL);
	for (int i = 0; i < 10; i++)
		slotWait->slots[i] = 1;
}

void semaphoreSignal(Semaphore *slotWait, int slot) {
	slotWait->slots[slot] = 1;
	ReleaseSemaphore(slotWait->semaphore, 1, NULL);
}

void semaphoreWait(Semaphore *slotWait, int slot) {
	WaitForSingleObject(slotWait->mutex, INFINITE);
	while (slotWait->slots[slot] == 0) {
		ReleaseSemaphore(slotWait->semaphore, 1, NULL);
		WaitForSingleObject(slotWait->semaphore, INFINITE);
	}
	slotWait->slots[slot] = 0;
	ReleaseMutex(slotWait->mutex);
}
#endif
