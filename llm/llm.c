#include "llm.h"
#include "../logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

#define MODULE "llm"

struct llm_engine {
    char model_path[MODEL_PATH_MAX];
    char binary_path[MODEL_PATH_MAX];
    bool available;
    pthread_mutex_t mutex;
};

llm_engine_t *llm_init(const char *model_path, const char *inference_binary) {
    llm_engine_t *eng = calloc(1, sizeof(llm_engine_t));
    if (!eng) return NULL;

    pthread_mutex_init(&eng->mutex, NULL);

    if (model_path)
        snprintf(eng->model_path, sizeof(eng->model_path), "%s", model_path);
    if (inference_binary)
        snprintf(eng->binary_path, sizeof(eng->binary_path), "%s", inference_binary);
    else
        snprintf(eng->binary_path, sizeof(eng->binary_path), "%s", "llm-inference");

    if (access(eng->binary_path, X_OK) == 0 && access(eng->model_path, R_OK) == 0) {
        eng->available = true;
        log_info(MODULE, "LLM engine initialized: model=%s binary=%s", eng->model_path, eng->binary_path);
    } else {
        eng->available = false;
        log_warn(MODULE, "LLM not available: model=%s binary=%s (fallback to regex only)",
                 eng->model_path, eng->binary_path);
    }

    return eng;
}

static int run_inference(llm_engine_t *eng, const char *query, char *output, size_t out_len) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        log_error(MODULE, "pipe failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        log_error(MODULE, "fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execl(eng->binary_path, eng->binary_path,
              "--model", eng->model_path,
              "--prompt", query,
              "--format", "json",
              (char *)NULL);

        _exit(1);
    }

    close(pipefd[1]);

    ssize_t total = 0;
    ssize_t n;
    while (total < (ssize_t)out_len - 1 && (n = read(pipefd[0], output + total, out_len - 1 - total)) > 0) {
        total += n;
    }
    output[total] = '\0';

    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }

    log_error(MODULE, "inference failed with status %d", status);
    return -1;
}

static int parse_intent(const char *json, llm_intent_t *intent) {
    memset(intent, 0, sizeof(*intent));

    const char *p;

    p = strstr(json, "\"action\"");
    if (p && (p = strchr(p, ':'))) {
        while (*p && *p != '"') p++;
        if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < (int)sizeof(intent->action) - 1)
                intent->action[i++] = *p++;
        }
    }

    p = strstr(json, "\"target\"");
    if (p && (p = strchr(p, ':'))) {
        while (*p && *p != '"') p++;
        if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < (int)sizeof(intent->target) - 1)
                intent->target[i++] = *p++;
        }
    }

    p = strstr(json, "\"confidence\"");
    if (p && (p = strchr(p, ':'))) {
        p++;
        intent->confidence = strtof(p, NULL);
    }

    return 0;
}

int llm_classify(llm_engine_t *eng, const char *query, llm_intent_t *intent) {
    if (!eng || !query || !intent) return -1;

    pthread_mutex_lock(&eng->mutex);

    if (!eng->available) {
        pthread_mutex_unlock(&eng->mutex);
        return -1;
    }

    char output[LLM_MAX_OUTPUT];
    if (run_inference(eng, query, output, sizeof(output)) < 0) {
        pthread_mutex_unlock(&eng->mutex);
        return -1;
    }

    parse_intent(output, intent);

    pthread_mutex_unlock(&eng->mutex);

    log_debug(MODULE, "classified: action=%s target=%s confidence=%.2f",
              intent->action, intent->target, intent->confidence);
    return 0;
}

bool llm_is_available(llm_engine_t *eng) {
    return eng && eng->available;
}

void llm_cleanup(llm_engine_t *eng) {
    if (!eng) return;
    pthread_mutex_destroy(&eng->mutex);
    free(eng);
    log_info(MODULE, "LLM engine cleaned up");
}
