#ifndef RECOVERY_H
#define RECOVERY_H

#include "common.h"

#define MAX_RETRIES 3
#define INITIAL_BACKOFF_MS 500
#define MAX_BACKOFF_MS 10000
#define AGENT_TIMEOUT_SECS 10

typedef struct {
    char agent_name[64];
    int  consecutive_failures;
    time_t last_failure;
    bool  circuit_open;
    time_t circuit_open_since;
} agent_health_t;

typedef struct recovery_manager recovery_manager_t;

recovery_manager_t *recovery_init(int max_agents);
void  recovery_record_failure(recovery_manager_t *rm, const char *agent_name);
void  recovery_record_success(recovery_manager_t *rm, const char *agent_name);
int   recovery_get_backoff_ms(recovery_manager_t *rm, const char *agent_name);
bool  recovery_is_available(recovery_manager_t *rm, const char *agent_name);
void  recovery_cleanup(recovery_manager_t *rm);

#endif
