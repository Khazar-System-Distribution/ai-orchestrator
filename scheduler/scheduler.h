#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../include/common.h"

typedef void (*scheduler_callback_t)(request_t *req, response_t *resp, void *ctx);

typedef struct scheduler scheduler_t;

scheduler_t *scheduler_init(int num_workers, int timeout_ms);
int          scheduler_enqueue(scheduler_t *sched, request_t *req, scheduler_callback_t cb, void *ctx);
void         scheduler_start(scheduler_t *sched);
void         scheduler_stop(scheduler_t *sched);
void         scheduler_cleanup(scheduler_t *sched);

#endif
