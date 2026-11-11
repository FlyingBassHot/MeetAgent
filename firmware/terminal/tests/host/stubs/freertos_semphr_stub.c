/*
 * Host-test shim for the small FreeRTOS semaphore subset used by ESPUI.
 * Single-threaded tests only; real locking is provided by FreeRTOS on target.
 */
#include "freertos/semphr.h"

#include <stdlib.h>

typedef struct {
    int locked;
} host_sem_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    host_sem_t *sem = calloc(1, sizeof(*sem));
    return (SemaphoreHandle_t)sem;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    return xSemaphoreCreateMutex();
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t wait)
{
    (void)wait;
    host_sem_t *sem = (host_sem_t *)semaphore;
    if (sem == NULL || sem->locked) {
        return pdFALSE;
    }
    sem->locked = 1;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    host_sem_t *sem = (host_sem_t *)semaphore;
    if (sem == NULL) {
        return pdFALSE;
    }
    sem->locked = 0;
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    free(semaphore);
}
