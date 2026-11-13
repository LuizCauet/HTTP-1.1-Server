#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "response.h"
#include "request.h"

typedef struct thread_pool thread_pool_t;

thread_pool_t *thread_pool_create(int num_threads);
void thread_pool_destroy(thread_pool_t *pool);
void thread_pool_shutdown(thread_pool_t *pool);
void thread_pool_add(thread_pool_t *pool, int client_fd);

#endif
