#include "include/common.h"
#include "logger/logger.h"
#include "config/config.h"
#include "protocol/protocol.h"
#include "ipc/ipc.h"
#include "registry/registry.h"
#include "router/router.h"
#include "scheduler/scheduler.h"
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
static scheduler_t       *g_scheduler = NULL;
static session_manager_t *g_session = NULL;
static metrics_t         *g_metrics = NULL;

static void handle_signal(int sig) {
    (void)sig;
    log_info(MODULE, "signal received, shutting down...");
    running = false;

    if (g_server) ipc_stop(g_server);
    if (g_scheduler) scheduler_stop(g_scheduler);
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

            double start = (double)clock() / CLOCKS_PER_SEC * 1000.0;

            intent_t intent;
            if (g_router && router_resolve(g_router, &req, &intent) == 0) {
                log_info(MODULE, "resolved intent: capability=%s", intent.required_capability);

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
                        "{\"id\":%llu,\"status\":\"success\",\"payload\":{\"message\":\"no agent available for %s\"}}",
                        (unsigned long long)req.id, intent.required_capability);
                }
            } else {
                char truncated_query[256];
                strncpy(truncated_query, req.query, sizeof(truncated_query) - 1);
                truncated_query[sizeof(truncated_query) - 1] = '\0';
                snprintf(response, sizeof(response),
                    "{\"id\":%llu,\"status\":\"success\",\"payload\":{\"message\":\"query received: %s\"}}",
                    (unsigned long long)req.id, truncated_query);
            }

            double elapsed = (double)clock() / CLOCKS_PER_SEC * 1000.0 - start;

            if (g_metrics) {
                metrics_record_request(g_metrics, elapsed, true);
            }

            ipc_send_response(client_fd, response, strlen(response));
            break;
        }

        case MSG_REGISTER: {
            protocol_build_response(
                &(response_t){.id = 0, .status = STATUS_SUCCESS, .payload = "agent registration endpoint"},
                response, sizeof(response));
            ipc_send_response(client_fd, response, strlen(response));
            break;
        }

        default:
            protocol_build_error(0, ERR_INVALID_REQUEST, response, sizeof(response));
            ipc_send_response(client_fd, response, strlen(response));
            break;
    }
}

int main(int argc, char *argv[]) {
    const char *config_path = NULL;

    if (argc > 1) {
        config_path = argv[1];
    }

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
    g_scheduler = scheduler_init(cfg.worker_threads, cfg.request_timeout_ms);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    scheduler_start(g_scheduler);

    g_server = ipc_init(cfg.socket_path, cfg.max_connections);
    if (!g_server) {
        log_fatal(MODULE, "failed to initialize IPC server");
    }

    log_info(MODULE, "AI Orchestrator v0.1 ready");
    log_info(MODULE, "listening on %s", cfg.socket_path);

    ipc_start(g_server, on_request_handler, NULL);

    log_info(MODULE, "shutting down...");

    ipc_cleanup(g_server);
    scheduler_cleanup(g_scheduler);
    session_cleanup(g_session);
    router_cleanup(g_router);
    registry_cleanup(g_registry);
    metrics_cleanup(g_metrics);
    logger_cleanup();

    return 0;
}
