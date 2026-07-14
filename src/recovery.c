#include "../include/recovery.h"
#include "../logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define MODULE "recovery"

struct recovery_manager {
    agent_health_t *agents;
    int max_agents;
    int count;
    pthread_mutex_t mutex;
};

recovery_manager_t *recovery_init(int max_agents) {
    recovery_manager_t *rm = calloc(1, sizeof(recovery_manager_t));
    if (!rm) return NULL;

    rm->max_agents = max_agents > 0 ? max_agents : MAX_AGENTS;
    rm->agents = calloc(rm->max_agents, sizeof(agent_health_t));
    pthread_mutex_init(&rm->mutex, NULL);

    log_info(MODULE, "recovery manager initialized");
    return rm;
}

static agent_health_t *find_or_create(recovery_manager_t *rm, const char *name) {
    for (int i = 0; i < rm->count; i++) {
        if (strcmp(rm->agents[i].agent_name, name) == 0)
            return &rm->agents[i];
    }

    if (rm->count < rm->max_agents) {
        agent_health_t *h = &rm->agents[rm->count++];
        snprintf(h->agent_name, sizeof(h->agent_name), "%s", name);
        return h;
    }

    return NULL;
}

void recovery_record_failure(recovery_manager_t *rm, const char *agent_name) {
    if (!rm || !agent_name) return;

    pthread_mutex_lock(&rm->mutex);
    agent_health_t *h = find_or_create(rm, agent_name);
    if (h) {
        h->consecutive_failures++;
        h->last_failure = time(NULL);

        if (h->consecutive_failures >= MAX_RETRIES && !h->circuit_open) {
            h->circuit_open = true;
            h->circuit_open_since = time(NULL);
            log_warn(MODULE, "circuit opened for agent: %s", agent_name);
        }
    }
    pthread_mutex_unlock(&rm->mutex);
}

void recovery_record_success(recovery_manager_t *rm, const char *agent_name) {
    if (!rm || !agent_name) return;

    pthread_mutex_lock(&rm->mutex);
    agent_health_t *h = find_or_create(rm, agent_name);
    if (h) {
        h->consecutive_failures = 0;
        if (h->circuit_open) {
            h->circuit_open = false;
            log_info(MODULE, "circuit closed for agent: %s", agent_name);
        }
    }
    pthread_mutex_unlock(&rm->mutex);
}

int recovery_get_backoff_ms(recovery_manager_t *rm, const char *agent_name) {
    if (!rm || !agent_name) return 0;

    pthread_mutex_lock(&rm->mutex);
    agent_health_t *h = find_or_create(rm, agent_name);
    if (!h || h->consecutive_failures == 0) {
        pthread_mutex_unlock(&rm->mutex);
        return 0;
    }

    int backoff = INITIAL_BACKOFF_MS * (1 << (h->consecutive_failures - 1));
    if (backoff > MAX_BACKOFF_MS) backoff = MAX_BACKOFF_MS;
    pthread_mutex_unlock(&rm->mutex);

    return backoff;
}

bool recovery_is_available(recovery_manager_t *rm, const char *agent_name) {
    if (!rm || !agent_name) return true;

    pthread_mutex_lock(&rm->mutex);
    agent_health_t *h = find_or_create(rm, agent_name);
    if (!h || !h->circuit_open) {
        pthread_mutex_unlock(&rm->mutex);
        return true;
    }

    time_t now = time(NULL);
    if (now - h->circuit_open_since >= AGENT_TIMEOUT_SECS) {
        h->circuit_open = false;
        log_info(MODULE, "circuit auto-recovered for agent: %s", agent_name);
        pthread_mutex_unlock(&rm->mutex);
        return true;
    }

    pthread_mutex_unlock(&rm->mutex);
    return false;
}

void recovery_cleanup(recovery_manager_t *rm) {
    if (!rm) return;
    pthread_mutex_destroy(&rm->mutex);
    free(rm->agents);
    free(rm);
    log_info(MODULE, "recovery manager cleaned up");
}
