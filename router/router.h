#ifndef ROUTER_H
#define ROUTER_H

#include "../include/common.h"
#include "../registry/registry.h"

typedef struct {
    char action[64];
    char target[128];
    char required_capability[64];
    char parameters[MAX_MESSAGE_SIZE];
} intent_t;

typedef struct router router_t;

router_t *router_init(registry_t *reg);
int       router_resolve(router_t *rt, const request_t *req, intent_t *intent);
void      router_cleanup(router_t *rt);

#endif
