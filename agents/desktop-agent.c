#define _POSIX_C_SOURCE 200809L
#include "../lib/agent_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define AGENT_NAME "desktop-agent"
#define AGENT_VERSION "0.2"

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static int exec_xdg_open(const char *target) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s' 2>/dev/null &", target);
    return system(cmd);
}

static int exec_dbus_call(const char *method, const char *arg) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "dbus-send --session --dest=org.freedesktop.Notifications "
        "--type=method_call --print-reply "
        "/org/freedesktop/Notifications "
        "org.freedesktop.Notifications.%s "
        "string:'AI-Orchestrator' uint32:0 string:'dbus' string:'%s' "
        "string:'urgency=normal' int32:5000 2>/dev/null",
        method, arg);
    return system(cmd);
}

static int open_application(const char *target) {
    printf("[desktop] opening: %s\n", target);

    if (exec_xdg_open(target) == 0) {
        printf("[desktop] opened via xdg-open\n");
        return 0;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s 2>/dev/null &", target);
    if (system(cmd) == 0) {
        printf("[desktop] launched: %s\n", target);
        return 0;
    }

    return -1;
}

static int close_application(const char *target) {
    printf("[desktop] closing: %s\n", target);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "pkill -f '%s' 2>/dev/null", target);
    return system(cmd);
}

static int send_notification(const char *message) {
    printf("[desktop] notification: %s\n", message);
    exec_dbus_call("Notify", message);
    return 0;
}

static int process_request(const char *query, char *response, size_t resp_size) {
    char lower[4096];
    int i = 0;
    for (const char *p = query; *p && i < (int)sizeof(lower) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z') lower[i++] = *p + 32;
        else lower[i++] = *p;
    }
    lower[i] = '\0';

    char *last_word = NULL;
    char *target = NULL;
    char *save = NULL;
    char *tok = strtok_r(lower, " ", &save);
    while (tok) {
        if (strcmp(tok, "ac") != 0 && strcmp(tok, "aç") != 0 &&
            strcmp(tok, "bagla") != 0 && strcmp(tok, "bağla") != 0 &&
            strcmp(tok, "bahla") != 0 &&
            strcmp(tok, "open") != 0 && strcmp(tok, "close") != 0 &&
            strcmp(tok, "launch") != 0 && strcmp(tok, "kill") != 0 &&
            strcmp(tok, "start") != 0 && strcmp(tok, "stop") != 0 &&
            strcmp(tok, "run") != 0 && strcmp(tok, "quit") != 0 &&
            strcmp(tok, "bildirim") != 0 && strcmp(tok, "bildiriş") != 0 &&
            strcmp(tok, "notify") != 0 && strcmp(tok, "notification") != 0 &&
            strcmp(tok, "durdur") != 0) {
            target = tok;
        }
        last_word = tok;
        tok = strtok_r(NULL, " ", &save);
    }

    if (strstr(lower, "bildirim") || strstr(lower, "bildiriş") ||
        strstr(lower, "notify") || strstr(lower, "notification")) {
        send_notification(target ? target : "bildirim");
        snprintf(response, resp_size, "notification sent");
        return 0;
    }

    if (strstr(lower, "bagla") || strstr(lower, "bağla") || strstr(lower, "bahla") ||
        strstr(lower, "close") || strstr(lower, "kill") || strstr(lower, "stop") ||
        strstr(lower, "quit") || strstr(lower, "durdur")) {
        if (target) {
            close_application(target);
            snprintf(response, resp_size, "application %s closed", target);
        } else {
            snprintf(response, resp_size, "no application specified to close");
        }
        return 0;
    }

    if (target) {
        open_application(target);
        snprintf(response, resp_size, "application %s opened", target);
    } else {
        snprintf(response, resp_size, "no application specified");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char *socket_path = "/run/ai-orchestrator.sock";
    if (argc > 1) socket_path = argv[1];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[desktop] Desktop Agent v%s starting...\n", AGENT_VERSION);

    agent_client_t *client = agent_client_init(socket_path);
    if (!client) {
        fprintf(stderr, "[desktop] failed to initialize agent client\n");
        return 1;
    }

    int retry = 0;
    while (running && retry < 5) {
        if (agent_client_connect(client) == 0) break;
        retry++;
        printf("[desktop] retry %d/5...\n", retry);
        sleep(2);
    }

    if (retry >= 5) {
        fprintf(stderr, "[desktop] could not connect to orchestrator\n");
        agent_client_cleanup(client);
        return 1;
    }

    const char *caps[] = {"open_application", "close_application", "notifications"};
    agent_client_register(client, AGENT_NAME, AGENT_VERSION, caps, 3);

    printf("[desktop] ready, waiting for requests...\n");

    while (running) {
        char buf[65536];
        int n = agent_client_read_request(client, buf, sizeof(buf), 1000);
        if (n < 0) {
            printf("[desktop] connection lost, reconnecting...\n");
            agent_client_disconnect(client);
            sleep(2);
            if (agent_client_connect(client) == 0) {
                agent_client_register(client, AGENT_NAME, AGENT_VERSION, caps, 3);
            }
            continue;
        }

        if (n == 0) continue;

        printf("[desktop] request: %s\n", buf);

        const char *query = strstr(buf, "\"query\"");
        if (query) {
            query = strchr(query, ':');
            if (query) {
                while (*query && *query != '"') query++;
                if (*query == '"') {
                    query++;
                    char q[4096];
                    int qi = 0;
                    while (*query && *query != '"' && qi < (int)sizeof(q) - 1)
                        q[qi++] = *query++;
                    q[qi] = '\0';

                    char response[4096];
                    process_request(q, response, sizeof(response));
                    agent_client_send_response(client, "success", response);
                }
            }
        }

        agent_client_send_heartbeat(client);
    }

    agent_client_cleanup(client);
    printf("[desktop] shutting down\n");
    return 0;
}
