#include "thread_pool.h"
#include "request_handler.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct thread_pool {
    int num_threads;
    pthread_t *threads;

    int queue[1024];
    int front;
    int back;
    int size;

    int shutting_down;

    pthread_mutex_t m;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

static void *worker(void *arg) {
    thread_pool_t *pool = arg;
    while (1) {

        pthread_mutex_lock(&pool->m);

        // wait for work OR shutdown
        while (pool->size == 0 && !pool->shutting_down) {
            pthread_cond_wait(&pool->not_empty, &pool->m);
        }

        if (pool->shutting_down && pool->size == 0) {
            pthread_mutex_unlock(&pool->m);
            return NULL; // exit thread
        }

        int fd = pool->queue[pool->front];
        pool->front = (pool->front + 1) % 1024;
        pool->size--;

        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->m);

        handle_request(fd);
    }
}

thread_pool_t *thread_pool_create(int num_threads) {
    thread_pool_t *pool = malloc(sizeof(*pool));
    if (!pool)
        return NULL;

    pool->num_threads = num_threads;
    pool->threads = calloc(num_threads, sizeof(pthread_t));

    pool->front = pool->back = pool->size = 0;
    pthread_mutex_init(&pool->m, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);
    pool->shutting_down = 0;

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker, pool);
    }

    return pool;
}

void thread_pool_add(thread_pool_t *pool, int client_fd) {
    pthread_mutex_lock(&pool->m);

    if (pool->shutting_down) {
        pthread_mutex_unlock(&pool->m);
        close(client_fd);
        return;
    }

    while (pool->size == 1024 && !pool->shutting_down) {
        pthread_cond_wait(&pool->not_full, &pool->m);
    }

    if (pool->shutting_down) {
        pthread_mutex_unlock(&pool->m);
        close(client_fd);
        return;
    }

    pool->queue[pool->back] = client_fd;
    pool->back = (pool->back + 1) % 1024;
    pool->size++;

    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->m);
}

void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool)
        return;
    free(pool->threads);
    free(pool);
}

void thread_pool_shutdown(thread_pool_t *pool) {
    pthread_mutex_lock(&pool->m);

    pool->shutting_down = 1;

    pthread_cond_broadcast(&pool->not_empty);
    pthread_cond_broadcast(&pool->not_full);

    pthread_mutex_unlock(&pool->m);

    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
}
