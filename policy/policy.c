#include "policy.h"
#include "../logger/logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define MODULE "policy"

struct policy_engine {
    policy_rule_t rules[MAX_RULES];
    int rule_count;
    policy_effect_t default_effect;
    pthread_mutex_t mutex;
};

policy_engine_t *policy_init(void) {
    policy_engine_t *pe = calloc(1, sizeof(policy_engine_t));
    if (!pe) return NULL;

    pthread_mutex_init(&pe->mutex, NULL);
    pe->rule_count = 0;
    pe->default_effect = POLICY_EFFECT_DENY;

    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "open_application", "desktop-agent", "open_application");
    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "close_application", "desktop-agent", "close_application");
    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "install_package", "package-agent", "install_package");
    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "remove_package", "package-agent", "remove_package");
    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "search_package", "package-agent", "search_package");
    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "network_management", "network-agent", "network_management");
    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "notifications", "desktop-agent", "notifications");

    log_info(MODULE, "policy engine initialized with %d rules (default: deny)", pe->rule_count);
    return pe;
}

int policy_add_rule(policy_engine_t *pe, policy_effect_t effect, const char *action, const char *agent, const char *capability) {
    if (!pe || !action) return -1;

    pthread_mutex_lock(&pe->mutex);

    if (pe->rule_count >= MAX_RULES) {
        pthread_mutex_unlock(&pe->mutex);
        log_error(MODULE, "max rules reached");
        return -1;
    }

    policy_rule_t *rule = &pe->rules[pe->rule_count++];
    rule->effect = effect;
    snprintf(rule->action, sizeof(rule->action), "%s", action);
    snprintf(rule->agent, sizeof(rule->agent), "%s", agent ? agent : "*");
    snprintf(rule->capability, sizeof(rule->capability), "%s", capability ? capability : "*");

    pthread_mutex_unlock(&pe->mutex);
    return 0;
}

int policy_check(policy_engine_t *pe, const char *action, const char *agent, const char *capability) {
    if (!pe || !action) return -1;

    pthread_mutex_lock(&pe->mutex);

    for (int i = 0; i < pe->rule_count; i++) {
        policy_rule_t *r = &pe->rules[i];

        bool action_match = (strcmp(r->action, "*") == 0) || (strcmp(r->action, action) == 0);
        bool agent_match = (strcmp(r->agent, "*") == 0) || (agent && strcmp(r->agent, agent) == 0);
        bool cap_match = (strcmp(r->capability, "*") == 0) || (capability && strcmp(r->capability, capability) == 0);

        if (action_match && agent_match && cap_match) {
            int result = (r->effect == POLICY_EFFECT_ALLOW) ? 0 : -1;
            char *effect_str = (r->effect == POLICY_EFFECT_ALLOW) ? "ALLOW" : "DENY";
            log_debug(MODULE, "%s: action=%s agent=%s capability=%s", effect_str, action, agent ? agent : "*", capability ? capability : "*");
            pthread_mutex_unlock(&pe->mutex);

            if (result != 0 && pe->default_effect == POLICY_EFFECT_DENY) {
                log_warn(MODULE, "policy denied: action=%s for agent=%s", action, agent ? agent : "unknown");
            }
            return result;
        }
    }

    pthread_mutex_unlock(&pe->mutex);

    if (pe->default_effect == POLICY_EFFECT_DENY) {
        log_warn(MODULE, "policy denied (default): action=%s for agent=%s", action, agent ? agent : "unknown");
        return -1;
    }

    return 0;
}

void policy_set_default_effect(policy_engine_t *pe, policy_effect_t effect) {
    if (!pe) return;
    pthread_mutex_lock(&pe->mutex);
    pe->default_effect = effect;
    pthread_mutex_unlock(&pe->mutex);
    log_info(MODULE, "default effect set to %s", effect == POLICY_EFFECT_ALLOW ? "ALLOW" : "DENY");
}

void policy_cleanup(policy_engine_t *pe) {
    if (!pe) return;
    pthread_mutex_destroy(&pe->mutex);
    free(pe);
    log_info(MODULE, "policy engine cleaned up");
}
