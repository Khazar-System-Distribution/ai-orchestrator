#ifndef POLICY_H
#define POLICY_H

#include "../include/common.h"

#define MAX_RULES 256
#define MAX_ACTIONS 64
#define POLICY_MAX_NAME 64

typedef enum {
    POLICY_EFFECT_ALLOW,
    POLICY_EFFECT_DENY
} policy_effect_t;

typedef struct {
    policy_effect_t effect;
    char action[MAX_ACTIONS];
    char agent[POLICY_MAX_NAME];
    char capability[MAX_ACTIONS];
} policy_rule_t;

typedef struct policy_engine policy_engine_t;

policy_engine_t *policy_init(void);
int policy_add_rule(policy_engine_t *pe, policy_effect_t effect, const char *action, const char *agent, const char *capability);
int policy_check(policy_engine_t *pe, const char *action, const char *agent, const char *capability);
void policy_set_default_effect(policy_engine_t *pe, policy_effect_t effect);
void policy_cleanup(policy_engine_t *pe);

#endif
