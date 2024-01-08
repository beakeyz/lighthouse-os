#ifndef __ANIVA_SYNC_SEMAPHORE__
#define __ANIVA_SYNC_SEMAPHORE__

#include <libk/stddef.h>

struct semaphore;

struct semaphore* create_semaphore(uint32_t limit, uint32_t value, uint32_t wait_capacity);
void destroy_semaphore(struct semaphore* sem);

int sem_wait(struct semaphore* sem, void* waiter);
int sem_post(struct semaphore* sem);

bool semaphore_is_binary(struct semaphore* sem);

#endif // !__ANIVA_SYNC_SEMAPHORE__
