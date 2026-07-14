#include "scheduler.h"
#include "../logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#define MODULE "scheduler"
#define MAX_QUEUE 1024

typedef struct {
    request_t req;
    scheduler_callback_t callback;
    void *ctx;
    bool active;
} queue_entry_t;

struct scheduler {
    queue_entry_t queue[MAX_QUEUE];
    int queue_head;
    int queue_tail;
    int queue_count;
    int num_workers;
    int timeout_ms;
    bool running;
    pthread_t *workers;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static void *worker_thread(void *arg) {
    scheduler_t *sched = (scheduler_t *)arg;

    while (sched->running) {
        pthread_mutex_lock(&sched->mutex);

        while (sched->queue_count == 0 && sched->running) {
            pthread_cond_wait(&sched->cond, &sched->mutex);
        }

        if (!sched->running) {
            pthread_mutex_unlock(&sched->mutex);
            break;
        }

        queue_entry_t entry = sched->queue[sched->queue_head];
        sched->queue_head = (sched->queue_head + 1) % MAX_QUEUE;
        sched->queue_count--;

        pthread_mutex_unlock(&sched->mutex);

        response_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.id = entry.req.id;
        resp.status = STATUS_SUCCESS;
        snprintf(resp.payload, sizeof(resp.payload), "request processed by scheduler");

        if (entry.callback) {
            entry.callback(&entry.req, &resp, entry.ctx);
        }
    }

    return NULL;
}

scheduler_t *scheduler_init(int num_workers, int timeout_ms) {
    scheduler_t *sched = calloc(1, sizeof(scheduler_t));
    if (!sched) return NULL;

    sched->num_workers = num_workers > 0 ? num_workers : MAX_WORKER_THREADS;
    sched->timeout_ms = timeout_ms > 0 ? timeout_ms : REQUEST_TIMEOUT_MS;
    sched->queue_head = 0;
    sched->queue_tail = 0;
    sched->queue_count = 0;
    sched->running = false;

    pthread_mutex_init(&sched->mutex, NULL);
    pthread_cond_init(&sched->cond, NULL);

    sched->workers = calloc(sched->num_workers, sizeof(pthread_t));

    log_info(MODULE, "scheduler initialized with %d workers", sched->num_workers);
    return sched;
}

int scheduler_enqueue(scheduler_t *sched, request_t *req, scheduler_callback_t cb, void *ctx) {
    if (!sched || !req) return -1;

    pthread_mutex_lock(&sched->mutex);

    if (sched->queue_count >= MAX_QUEUE) {
        pthread_mutex_unlock(&sched->mutex);
        log_warn(MODULE, "queue full, request rejected");
        return -1;
    }

    sched->queue[sched->queue_tail].req = *req;
    sched->queue[sched->queue_tail].callback = cb;
    sched->queue[sched->queue_tail].ctx = ctx;
    sched->queue[sched->queue_tail].active = true;
    sched->queue_tail = (sched->queue_tail + 1) % MAX_QUEUE;
    sched->queue_count++;

    pthread_cond_signal(&sched->cond);
    pthread_mutex_unlock(&sched->mutex);

    log_debug(MODULE, "request %llu enqueued", (unsigned long long)req->id);
    return 0;
}

void scheduler_start(scheduler_t *sched) {
    if (!sched || sched->running) return;

    sched->running = true;
    for (int i = 0; i < sched->num_workers; i++) {
        pthread_create(&sched->workers[i], NULL, worker_thread, sched);
    }
    log_info(MODULE, "scheduler started");
}

void scheduler_stop(scheduler_t *sched) {
    if (!sched || !sched->running) return;

    sched->running = false;
    pthread_cond_broadcast(&sched->cond);

    for (int i = 0; i < sched->num_workers; i++) {
        pthread_join(sched->workers[i], NULL);
    }
    log_info(MODULE, "scheduler stopped");
}

void scheduler_cleanup(scheduler_t *sched) {
    if (!sched) return;
    scheduler_stop(sched);
    pthread_mutex_destroy(&sched->mutex);
    pthread_cond_destroy(&sched->cond);
    free(sched->workers);
    free(sched);
    log_info(MODULE, "scheduler cleaned up");
}
