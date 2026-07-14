#define _POSIX_C_SOURCE 200809L
#include "../lib/agent_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define AGENT_NAME "package-agent"
#define AGENT_VERSION "0.1"

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

typedef enum {
    PM_APT,
    PM_PACMAN,
    PM_FLATPAK,
    PM_UNKNOWN
} package_manager_t;

static package_manager_t detect_pm(void) {
    if (access("/usr/bin/apt", X_OK) == 0) return PM_APT;
    if (access("/usr/bin/pacman", X_OK) == 0) return PM_PACMAN;
    if (access("/usr/bin/flatpak", X_OK) == 0) return PM_FLATPAK;
    return PM_UNKNOWN;
}

static const char *pm_name(package_manager_t pm) {
    switch (pm) {
        case PM_APT: return "apt";
        case PM_PACMAN: return "pacman";
        case PM_FLATPAK: return "flatpak";
        default: return "unknown";
    }
}

static int install_package(const char *name) {
    package_manager_t pm = detect_pm();
    if (pm == PM_UNKNOWN) return -1;

    printf("[package] installing '%s' via %s\n", name, pm_name(pm));

    char cmd[1024];
    switch (pm) {
        case PM_APT:
            snprintf(cmd, sizeof(cmd), "apt-get install -y '%s' 2>/dev/null", name);
            break;
        case PM_PACMAN:
            snprintf(cmd, sizeof(cmd), "pacman -S --noconfirm '%s' 2>/dev/null", name);
            break;
        case PM_FLATPAK:
            snprintf(cmd, sizeof(cmd), "flatpak install -y '%s' 2>/dev/null", name);
            break;
        default:
            return -1;
    }

    return system(cmd);
}

static int remove_package(const char *name) {
    package_manager_t pm = detect_pm();
    if (pm == PM_UNKNOWN) return -1;

    printf("[package] removing '%s' via %s\n", name, pm_name(pm));

    char cmd[1024];
    switch (pm) {
        case PM_APT:
            snprintf(cmd, sizeof(cmd), "apt-get remove -y '%s' 2>/dev/null", name);
            break;
        case PM_PACMAN:
            snprintf(cmd, sizeof(cmd), "pacman -R --noconfirm '%s' 2>/dev/null", name);
            break;
        case PM_FLATPAK:
            snprintf(cmd, sizeof(cmd), "flatpak uninstall -y '%s' 2>/dev/null", name);
            break;
        default:
            return -1;
    }

    return system(cmd);
}

static int search_package(const char *name, char *out, size_t out_len) {
    package_manager_t pm = detect_pm();
    if (pm == PM_UNKNOWN) return -1;

    printf("[package] searching '%s' via %s\n", name, pm_name(pm));

    char cmd[1024];
    switch (pm) {
        case PM_APT:
            snprintf(cmd, sizeof(cmd), "apt-cache search '%s' 2>/dev/null | head -5", name);
            break;
        case PM_PACMAN:
            snprintf(cmd, sizeof(cmd), "pacman -Ss '%s' 2>/dev/null | head -5", name);
            break;
        case PM_FLATPAK:
            snprintf(cmd, sizeof(cmd), "flatpak search '%s' 2>/dev/null | head -5", name);
            break;
        default:
            return -1;
    }

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    char buf[4096];
    size_t total = 0;
    while (fgets(buf, sizeof(buf), fp) && total < out_len - 1) {
        size_t len = strlen(buf);
        if (total + len < out_len - 1) {
            memcpy(out + total, buf, len);
            total += len;
        }
    }
    out[total] = '\0';

    return pclose(fp);
}

static int process_request(const char *query, char *response, size_t resp_size) {
    char lower[4096];
    int i = 0;
    for (const char *p = query; *p && i < (int)sizeof(lower) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z') lower[i++] = *p + 32;
        else lower[i++] = *p;
    }
    lower[i] = '\0';

    char *target = NULL;
    char *save = NULL;
    char *tok = strtok_r(lower, " ", &save);
    while (tok) {
        if (strcmp(tok, "qur") != 0 && strcmp(tok, "kur") != 0 &&
            strcmp(tok, "install") != 0 && strcmp(tok, "yukle") != 0 &&
            strcmp(tok, "yükle") != 0 &&
            strcmp(tok, "sil") != 0 && strcmp(tok, "remove") != 0 &&
            strcmp(tok, "kaldir") != 0 && strcmp(tok, "kaldır") != 0 &&
            strcmp(tok, "uninstall") != 0 &&
            strcmp(tok, "ara") != 0 && strcmp(tok, "search") != 0 &&
            strcmp(tok, "find") != 0 && strcmp(tok, "lookup") != 0) {
            target = tok;
        }
        tok = strtok_r(NULL, " ", &save);
    }

    if (strstr(lower, "ara") || strstr(lower, "search") || strstr(lower, "find")) {
        if (target) {
            char result[4096] = "";
            search_package(target, result, sizeof(result));
            snprintf(response, resp_size, "search results for %s:\n%s", target, result);
        } else {
            snprintf(response, resp_size, "what package to search?");
        }
        return 0;
    }

    if (strstr(lower, "sil") || strstr(lower, "remove") || strstr(lower, "kaldir") || strstr(lower, "uninstall")) {
        if (target) {
            remove_package(target);
            snprintf(response, resp_size, "package '%s' removed", target);
        } else {
            snprintf(response, resp_size, "what package to remove?");
        }
        return 0;
    }

    if (target) {
        install_package(target);
        snprintf(response, resp_size, "package '%s' installed via %s", target, pm_name(detect_pm()));
    } else {
        package_manager_t pm = detect_pm();
        snprintf(response, resp_size, "package manager: %s (no package specified)", pm_name(pm));
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char *socket_path = "/run/ai-orchestrator.sock";
    if (argc > 1) socket_path = argv[1];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[package] Package Agent v%s starting...\n", AGENT_VERSION);
    printf("[package] detected: %s\n", pm_name(detect_pm()));

    agent_client_t *client = agent_client_init(socket_path);
    if (!client) { fprintf(stderr, "[package] init failed\n"); return 1; }

    int retry = 0;
    while (running && retry < 5) {
        if (agent_client_connect(client) == 0) break;
        retry++;
        sleep(2);
    }
    if (retry >= 5) { agent_client_cleanup(client); return 1; }

    const char *caps[] = {"install_package", "remove_package", "search_package"};
    agent_client_register(client, AGENT_NAME, AGENT_VERSION, caps, 3);

    printf("[package] ready, waiting for requests...\n");

    while (running) {
        char buf[65536];
        int n = agent_client_read_request(client, buf, sizeof(buf), 1000);
        if (n < 0) {
            printf("[package] reconnecting...\n");
            agent_client_disconnect(client);
            sleep(2);
            if (agent_client_connect(client) == 0)
                agent_client_register(client, AGENT_NAME, AGENT_VERSION, caps, 3);
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
    printf("[package] shutting down\n");
    return 0;
}
