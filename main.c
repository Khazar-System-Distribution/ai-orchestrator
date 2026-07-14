#include "include/common.h"
#include "logger/logger.h"
#include "config/config.h"
#include "protocol/protocol.h"
#include "ipc/ipc.h"
#include "registry/registry.h"
#include "router/router.h"
#include "session/session.h"
#include "metrics/metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MODULE "main"

static volatile bool running = true;

static config_t          *g_config = NULL;
static ipc_server_t      *g_server = NULL;
static registry_t        *g_registry = NULL;
static router_t          *g_router = NULL;
static session_manager_t *g_session = NULL;
static metrics_t         *g_metrics = NULL;

static void handle_signal(int sig) {
    (void)sig;
    log_info(MODULE, "signal received, shutting down...");
    running = false;
    if (g_server) ipc_stop(g_server);
}

static void on_request_handler(int client_fd, const char *data, size_t len, void *ctx) {
    (void)ctx;

    message_type_t type;
    request_t req;
    char response[512];

    if (protocol_parse(data, len, &type, &req) < 0) {
        protocol_build_error(0, ERR_INVALID_REQUEST, response, sizeof(response));
        ipc_send_response(client_fd, response, strlen(response));
        return;
    }

    switch (type) {
        case MSG_PING: {
            protocol_build_ping_response(response, sizeof(response));
            ipc_send_response(client_fd, response, strlen(response));
            log_debug(MODULE, "ping response sent");
            break;
        }

        case MSG_REQUEST: {
            log_info(MODULE, "request received: %s", req.query);

            intent_t intent;
            if (g_router && router_resolve(g_router, &req, &intent) == 0) {
                log_info(MODULE, "resolved: capability=%s", intent.required_capability);

                agent_info_t *agent = NULL;
                if (g_registry) {
                    agent = registry_find_by_capability(g_registry, intent.required_capability);
                }

                if (agent) {
                    snprintf(response, sizeof(response),
                        "{\"id\":%llu,\"status\":\"success\",\"payload\":{\"message\":\"routed to %s\",\"agent\":\"%s\"}}",
                        (unsigned long long)req.id, intent.required_capability, agent->name);
                } else {
                    snprintf(response, sizeof(response),
                        "{\"id\":%llu,\"status\":\"success\",\"payload\":{\"message\":\"no agent for %s\"}}",
                        (unsigned long long)req.id, intent.required_capability);
                }
            } else {
                snprintf(response, sizeof(response),
                    "{\"id\":%llu,\"status\":\"success\",\"payload\":{\"message\":\"query received\"}}",
                    (unsigned long long)req.id);
            }

            if (g_metrics) {
                metrics_record_request(g_metrics, 0, true);
            }

            ipc_send_response(client_fd, response, strlen(response));
            break;
        }

        case MSG_REGISTER: {
            agent_info_t agent;
            memset(&agent, 0, sizeof(agent));

            const char *name_p = strstr(data, "\"name\"");
            if (name_p) {
                name_p = strchr(name_p, ':');
                if (name_p) {
                    while (*name_p && *name_p != '"') name_p++;
                    if (*name_p == '"') {
                        name_p++;
                        int i = 0;
                        while (*name_p && *name_p != '"' && i < MAX_NAME_LEN - 1)
                            agent.name[i++] = *name_p++;
                    }
                }
            }

            const char *ver_p = strstr(data, "\"version\"");
            if (ver_p) {
                ver_p = strchr(ver_p, ':');
                if (ver_p) {
                    while (*ver_p && *ver_p != '"') ver_p++;
                    if (*ver_p == '"') {
                        ver_p++;
                        int i = 0;
                        while (*ver_p && *ver_p != '"' && i < 15)
                            agent.version[i++] = *ver_p++;
                    }
                }
            }

            const char *caps_p = strstr(data, "\"capabilities\"");
            if (caps_p) {
                caps_p = strchr(caps_p, '[');
                if (caps_p) {
                    caps_p++;
                    while (*caps_p && *caps_p != ']' && agent.cap_count < MAX_CAPABILITIES) {
                        while (*caps_p && *caps_p != '"') caps_p++;
                        if (*caps_p == '"') {
                            caps_p++;
                            int i = 0;
                            while (*caps_p && *caps_p != '"' && i < MAX_NAME_LEN - 1)
                                agent.capabilities[agent.cap_count][i++] = *caps_p++;
                            agent.cap_count++;
                            if (*caps_p) caps_p++;
                        }
                        while (*caps_p && *caps_p != ',' && *caps_p != ']') caps_p++;
                        if (*caps_p == ',') caps_p++;
                    }
                }
            }

            agent.alive = true;
            agent.last_heartbeat = time(NULL);

            if (agent.name[0]) {
                registry_register(g_registry, &agent);
                snprintf(response, sizeof(response),
                    "{\"status\":\"success\",\"payload\":{\"message\":\"agent %s registered\"}}",
                    agent.name);
            } else {
                snprintf(response, sizeof(response),
                    "{\"status\":\"error\",\"error_code\":\"INVALID_REGISTRATION\"}");
            }

            ipc_send_response(client_fd, response, strlen(response));
            break;
        }

        default:
            protocol_build_error(0, ERR_INVALID_REQUEST, response, sizeof(response));
            ipc_send_response(client_fd, response, strlen(response));
            break;
    }
}

static void cleanup(void) {
    ipc_cleanup(g_server);
    session_cleanup(g_session);
    router_cleanup(g_router);
    registry_cleanup(g_registry);
    metrics_cleanup(g_metrics);
    logger_cleanup();
}

int main(int argc, char *argv[]) {
    const char *config_path = NULL;
    if (argc > 1) config_path = argv[1];

    logger_init(LOG_INFO);
    log_info(MODULE, "AI Orchestrator v0.1 starting...");

    config_t cfg;
    config_load(config_path, &cfg);
    g_config = &cfg;
    logger_set_level(cfg.log_level);
    config_print(g_config);

    g_metrics = metrics_init(cfg.enable_metrics);
    g_registry = registry_init();
    g_router = router_init(g_registry);
    g_session = session_init(MAX_SESSIONS, SESSION_TIMEOUT);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    g_server = ipc_init(cfg.socket_path, cfg.max_connections);
    if (!g_server) {
        log_fatal(MODULE, "failed to initialize IPC server");
    }

    log_info(MODULE, "AI Orchestrator v0.1 ready");
    log_info(MODULE, "listening on %s", cfg.socket_path);

    ipc_start(g_server, on_request_handler, NULL);

    log_info(MODULE, "shutting down...");
    cleanup();
    return 0;
}
