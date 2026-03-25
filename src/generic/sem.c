

#include "./sem.h"

// semaphore compatibility
// see
// https://stackoverflow.com/questions/27736618/why-are-sem-init-sem-getvalue-sem-destroy-deprecated-on-mac-os-x-and-w
// see https://www.unix.com/man_page/mojave/3/dispatch_semaphore_create/

int comp_sem_init(SemaphoreType* sem, uint32_t value, // NOLINT(totto-use-fixed-width-types-var)
                  bool shared) {
#ifdef __APPLE__
	*sem = dispatch_semaphore_create(value);
	(void)shared;
	return *sem == NULL ? -1 : 0;
#else
	return sem_init(sem, shared ? 0 : -1, value); // NOLINT(readability-implicit-bool-conversion)
#endif
}

int comp_sem_wait(SemaphoreType* sem) { // NOLINT(totto-use-fixed-width-types-var)

#ifdef __APPLE__
	return dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
#else
	return sem_wait(sem);
#endif
}

int comp_sem_post(SemaphoreType* sem) { // NOLINT(totto-use-fixed-width-types-var)

#ifdef __APPLE__
	return dispatch_semaphore_signal(*sem);
#else
	return sem_post(sem);
#endif
}

int comp_sem_destroy(SemaphoreType* sem) { // NOLINT(totto-use-fixed-width-types-var)

#ifdef __APPLE__
	dispatch_release(*sem);
	return 0;
#else
	return sem_destroy(sem);
#endif
}
