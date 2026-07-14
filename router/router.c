#include "router.h"
#include "../logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define MODULE "router"

struct router {
    registry_t *registry;
    rule_t *rules;
    int rule_count;
    pthread_mutex_t mutex;
};

router_t *router_init(registry_t *reg) {
    router_t *rt = calloc(1, sizeof(router_t));
    if (!rt) return NULL;

    rt->registry = reg;
    rt->rules = NULL;
    rt->rule_count = 0;
    pthread_mutex_init(&rt->mutex, NULL);

    router_add_rule(rt, "a[cç]|open|launch|start|run", "open_application");
    router_add_rule(rt, "ba[ghğ]la|close|kill|durdur|stop|quit", "close_application");
    router_add_rule(rt, "qur|install|kur|y[üu]kle", "install_package");
    router_add_rule(rt, "sil|remove|kald[ıi]r|uninstall", "remove_package");
    router_add_rule(rt, "ara|search|find|lookup", "search_package");
    router_add_rule(rt, "wifi|wi-fi|network", "network_management");
    router_add_rule(rt, "bildiri[mş]|notification|notify", "notifications");

    log_info(MODULE, "router initialized with %d rules", rt->rule_count);
    return rt;
}

int router_add_rule(router_t *rt, const char *pattern, const char *capability) {
    if (!rt || !pattern || !capability) return -1;

    rule_t *rule = calloc(1, sizeof(rule_t));
    if (!rule) return -1;

    strncpy(rule->pattern, pattern, sizeof(rule->pattern) - 1);
    strncpy(rule->capability, capability, sizeof(rule->capability) - 1);

    pthread_mutex_lock(&rt->mutex);
    rule->next = rt->rules;
    rt->rules = rule;
    rt->rule_count++;
    pthread_mutex_unlock(&rt->mutex);

    return 0;
}

static bool match_pattern(const char *query, const char *pattern) {
    if (!query || !pattern) return false;
    return strstr(query, pattern) != NULL;
}

int router_resolve(router_t *rt, const request_t *req, intent_t *intent) {
    if (!rt || !req || !intent) return -1;

    memset(intent, 0, sizeof(*intent));

    char query_lower[MAX_MESSAGE_SIZE];
    int idx = 0;
    for (const char *p = req->query; *p && idx < (int)sizeof(query_lower) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z')
            query_lower[idx++] = *p + 32;
        else
            query_lower[idx++] = *p;
    }
    query_lower[idx] = '\0';

    pthread_mutex_lock(&rt->mutex);

    for (rule_t *r = rt->rules; r; r = r->next) {
        if (match_pattern(query_lower, r->pattern)) {
            snprintf(intent->action, sizeof(intent->action), "%s", r->pattern);
            snprintf(intent->required_capability, sizeof(intent->required_capability), "%s", r->capability);
            snprintf(intent->target, sizeof(intent->target), "%s", query_lower);
            pthread_mutex_unlock(&rt->mutex);
            log_debug(MODULE, "resolved query to capability: %s", r->capability);
            return 0;
        }
    }

    pthread_mutex_unlock(&rt->mutex);
    log_debug(MODULE, "no matching rule for query: %s", req->query);
    return -1;
}

void router_cleanup(router_t *rt) {
    if (!rt) return;

    rule_t *r = rt->rules;
    while (r) {
        rule_t *next = r->next;
        free(r);
        r = next;
    }

    pthread_mutex_destroy(&rt->mutex);
    free(rt);
    log_info(MODULE, "router cleaned up");
}
