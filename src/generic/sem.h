

#pragma once

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

#ifdef __APPLE__
typedef dispatch_semaphore_t SemaphoreType;
#else
typedef sem_t SemaphoreType;
#endif

#include <stdbool.h>
#include <stdint.h>

int comp_sem_init(SemaphoreType* sem, uint32_t value, bool shared);

int comp_sem_wait(SemaphoreType* sem);

int comp_sem_post(SemaphoreType* sem);

int comp_sem_destroy(SemaphoreType* sem);
