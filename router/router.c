#include "router.h"
#include "../logger/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <regex.h>

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

    router_add_rule(rt, "(^|[[:space:]])ac($|[[:space:]])", "open_application");
    router_add_rule(rt, "(^|[[:space:]])aç($|[[:space:]])", "open_application");
    router_add_rule(rt, "(^|[[:space:]])open($|[[:space:]])", "open_application");
    router_add_rule(rt, "(^|[[:space:]])launch($|[[:space:]])", "open_application");
    router_add_rule(rt, "(^|[[:space:]])start($|[[:space:]])", "open_application");
    router_add_rule(rt, "(^|[[:space:]])run($|[[:space:]])", "open_application");
    router_add_rule(rt, "(^|[[:space:]])bagla($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])bahla($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])bağla($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])close($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])kill($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])stop($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])quit($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])durdur($|[[:space:]])", "close_application");
    router_add_rule(rt, "(^|[[:space:]])qur($|[[:space:]])", "install_package");
    router_add_rule(rt, "(^|[[:space:]])kur($|[[:space:]])", "install_package");
    router_add_rule(rt, "(^|[[:space:]])install($|[[:space:]])", "install_package");
    router_add_rule(rt, "(^|[[:space:]])yukle($|[[:space:]])", "install_package");
    router_add_rule(rt, "(^|[[:space:]])yükle($|[[:space:]])", "install_package");
    router_add_rule(rt, "(^|[[:space:]])sil($|[[:space:]])", "remove_package");
    router_add_rule(rt, "(^|[[:space:]])remove($|[[:space:]])", "remove_package");
    router_add_rule(rt, "(^|[[:space:]])kaldir($|[[:space:]])", "remove_package");
    router_add_rule(rt, "(^|[[:space:]])kaldır($|[[:space:]])", "remove_package");
    router_add_rule(rt, "(^|[[:space:]])ara($|[[:space:]])", "search_package");
    router_add_rule(rt, "(^|[[:space:]])search($|[[:space:]])", "search_package");
    router_add_rule(rt, "(^|[[:space:]])find($|[[:space:]])", "search_package");
    router_add_rule(rt, "(^|[[:space:]])wifi($|[[:space:]])", "network_management");
    router_add_rule(rt, "(^|[[:space:]])network($|[[:space:]])", "network_management");
    router_add_rule(rt, "(^|[[:space:]])bildirim($|[[:space:]])", "notifications");
    router_add_rule(rt, "(^|[[:space:]])bildiriş($|[[:space:]])", "notifications");

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

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB);
    if (ret != 0) {
        char errbuf[256];
        regerror(ret, &regex, errbuf, sizeof(errbuf));
        log_warn(MODULE, "regex compile failed: %s -> %s", pattern, errbuf);
        return false;
    }

    ret = regexec(&regex, query, 0, NULL, 0);
    regfree(&regex);

    return ret == 0;
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
