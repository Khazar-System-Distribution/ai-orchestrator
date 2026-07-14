#include "../policy/policy.h"
#include "../logger/logger.h"
#include <stdio.h>
#include <assert.h>

int main(void) {
    logger_init(LOG_TRACE);
    logger_set_level(LOG_WARN);

    policy_engine_t *pe = policy_init();
    assert(pe != NULL);

    assert(policy_check(pe, "open_application", "desktop-agent", "open_application") == 0);
    fprintf(stderr, "policy: allowed action OK\n");

    assert(policy_check(pe, "unknown_action", "desktop-agent", "unknown") != 0);
    fprintf(stderr, "policy: denied unknown action OK\n");

    assert(policy_check(pe, "hack_system", "hacker-agent", "hack") != 0);
    fprintf(stderr, "policy: default deny OK\n");

    policy_add_rule(pe, POLICY_EFFECT_ALLOW, "custom_action", "custom-agent", "custom_cap");
    assert(policy_check(pe, "custom_action", "custom-agent", "custom_cap") == 0);
    fprintf(stderr, "policy: custom rule added and checked OK\n");

    policy_cleanup(pe);
    logger_cleanup();
    fprintf(stderr, "policy: ALL TESTS PASSED\n");
    return 0;
}
