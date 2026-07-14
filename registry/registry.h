#ifndef REGISTRY_H
#define REGISTRY_H

#include "../include/common.h"

#define MAX_CAPABILITIES 32
#define MAX_NAME_LEN     64
#define MAX_SOCKET_LEN   128

typedef struct {
    char name[MAX_NAME_LEN];
    char version[16];
    char socket_path[MAX_SOCKET_LEN];
    char capabilities[MAX_CAPABILITIES][MAX_NAME_LEN];
    int  cap_count;
    bool alive;
    time_t last_heartbeat;
} agent_info_t;

typedef struct registry registry_t;

registry_t *registry_init(void);
int         registry_register(registry_t *reg, const agent_info_t *agent);
int         registry_unregister(registry_t *reg, const char *name);
agent_info_t *registry_find_by_capability(registry_t *reg, const char *capability);
agent_info_t *registry_find_by_name(registry_t *reg, const char *name);
void        registry_cleanup(registry_t *reg);

#endif
