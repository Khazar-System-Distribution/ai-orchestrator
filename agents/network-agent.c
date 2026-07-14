#define _POSIX_C_SOURCE 200809L
#include "../lib/agent_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define AGENT_NAME "network-agent"
#define AGENT_VERSION "0.1"

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static int nmcli_exec(const char *args, char *out, size_t out_len) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "nmcli %s 2>/dev/null", args ? args : "");

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    size_t total = 0;
    while (fgets(out + total, out_len - total, fp) && total < out_len - 1) {
        total = strlen(out);
    }

    return pclose(fp);
}

static int wifi_scan(char *out, size_t out_len) {
    printf("[network] scanning wifi...\n");
    system("nmcli device wifi rescan 2>/dev/null");
    return nmcli_exec("device wifi list --rescan no 2>/dev/null | head -10", out, out_len);
}

static int wifi_connect(const char *ssid) {
    printf("[network] connecting to wifi: %s\n", ssid);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "nmcli device wifi connect '%s' 2>/dev/null", ssid);
    return system(cmd);
}

static int wifi_disconnect(void) {
    printf("[network] disconnecting wifi\n");
    return system("nmcli device disconnect wlan0 2>/dev/null");
}

static int get_status(char *out, size_t out_len) {
    return nmcli_exec("general status", out, out_len);
}

static int process_request(const char *query, char *response, size_t resp_size) {
    char lower[4096];
    int i = 0;
    for (const char *p = query; *p && i < (int)sizeof(lower) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z') lower[i++] = *p + 32;
        else lower[i++] = *p;
    }
    lower[i] = '\0';

    if (strstr(lower, "scan") || (strstr(lower, "wifi") && strstr(lower, "ara"))) {
        char result[4096] = "";
        wifi_scan(result, sizeof(result));
        snprintf(response, resp_size, "wifi networks:\n%s", result);
        return 0;
    }

    if (strstr(lower, "connect") || strstr(lower, "baglan") || strstr(lower, "bağlan")) {
        char *target = NULL;
        char *save = NULL;
        char *tok = strtok_r(lower, " ", &save);
        while (tok) {
            if (strcmp(tok, "connect") != 0 && strcmp(tok, "baglan") != 0 && strcmp(tok, "bağlan") != 0 && strcmp(tok, "wifi") != 0)
                target = tok;
            tok = strtok_r(NULL, " ", &save);
        }
        if (target) {
            wifi_connect(target);
            snprintf(response, resp_size, "connecting to '%s'", target);
        } else {
            snprintf(response, resp_size, "which network to connect?");
        }
        return 0;
    }

    if (strstr(lower, "disconnect") || strstr(lower, "kes") || strstr(lower, "kəs")) {
        wifi_disconnect();
        snprintf(response, resp_size, "disconnected");
        return 0;
    }

    if (strstr(lower, "status") || strstr(lower, "durum") || strstr(lower, "durun") || strstr(lower, "wifi")) {
        char result[4096] = "";
        get_status(result, sizeof(result));
        snprintf(response, resp_size, "network status:\n%s", result);
        return 0;
    }

    snprintf(response, resp_size, "unknown network command");
    return 0;
}

int main(int argc, char *argv[]) {
    const char *socket_path = "/run/ai-orchestrator.sock";
    if (argc > 1) socket_path = argv[1];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[network] Network Agent v%s starting...\n", AGENT_VERSION);

    agent_client_t *client = agent_client_init(socket_path);
    if (!client) { fprintf(stderr, "[network] init failed\n"); return 1; }

    int retry = 0;
    while (running && retry < 5) {
        if (agent_client_connect(client) == 0) break;
        retry++;
        sleep(2);
    }
    if (retry >= 5) { agent_client_cleanup(client); return 1; }

    const char *caps[] = {"network_management"};
    agent_client_register(client, AGENT_NAME, AGENT_VERSION, caps, 1);

    printf("[network] ready, waiting for requests...\n");

    while (running) {
        char buf[65536];
        int n = agent_client_read_request(client, buf, sizeof(buf), 1000);
        if (n < 0) {
            printf("[network] reconnecting...\n");
            agent_client_disconnect(client);
            sleep(2);
            if (agent_client_connect(client) == 0)
                agent_client_register(client, AGENT_NAME, AGENT_VERSION, caps, 1);
            continue;
        }
        if (n == 0) continue;

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
    printf("[network] shutting down\n");
    return 0;
}
