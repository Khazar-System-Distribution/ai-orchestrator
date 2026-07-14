#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>

#define SOCKET_PATH "/run/ai-orchestrator.sock"
#define AGENT_NAME "desktop-agent"
#define AGENT_VERSION "0.1"

static volatile int running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static int send_register_request(int fd) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{"
        "\"type\":\"register\","
        "\"name\":\"%s\","
        "\"version\":\"%s\","
        "\"capabilities\":[\"open_application\",\"close_application\",\"notifications\"]"
        "}", AGENT_NAME, AGENT_VERSION);

    ssize_t n = write(fd, buf, strlen(buf));
    if (n < 0) {
        perror("write register");
        return -1;
    }

    n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        printf("[agent] register response: %s\n", buf);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *socket_path = SOCKET_PATH;
    if (argc > 1) {
        socket_path = argv[1];
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[agent] Desktop Agent v%s starting...\n", AGENT_VERSION);
    printf("[agent] connecting to %s\n", socket_path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    printf("[agent] connected to orchestrator at %s\n", socket_path);

    if (send_register_request(fd) < 0) {
        close(fd);
        return 1;
    }

    printf("[agent] registered as %s, waiting for requests...\n", AGENT_NAME);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};

        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (running) perror("select");
            break;
        }

        if (ret == 0) continue;

        char buf[65536];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            printf("[agent] orchestrator disconnected\n");
            break;
        }

        buf[n] = '\0';
        printf("[agent] received: %s\n", buf);

        char response[1024];
        snprintf(response, sizeof(response),
            "{"
            "\"status\":\"success\","
            "\"payload\":{\"message\":\"desktop-agent executed\"}"
            "}");

        write(fd, response, strlen(response));
        printf("[agent] sent response\n");
    }

    close(fd);
    printf("[agent] shutting down\n");
    return 0;
}
