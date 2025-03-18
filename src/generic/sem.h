

#pragma once

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

#ifdef __APPLE__
typedef dispatch_semaphore_t SEMAPHORE_TYPE;
#else
typedef sem_t SEMAPHORE_TYPE;
#endif

#include <stdbool.h>
#include <stdint.h>

int comp_sem_init(SEMAPHORE_TYPE* sem, uint32_t value, bool shared);

int comp_sem_wait(SEMAPHORE_TYPE* sem);

int comp_sem_post(SEMAPHORE_TYPE* sem);

int comp_sem_destroy(SEMAPHORE_TYPE* sem);
