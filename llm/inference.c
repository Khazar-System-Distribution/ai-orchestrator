#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_json_intent(const char *action, const char *target, float confidence) {
    printf("{\"action\":\"%s\",\"target\":\"%s\",\"confidence\":%.2f}\n",
           action ? action : "unknown",
           target ? target : "",
           confidence);
}

static const char *classify_with_rules(const char *query) {
    if (!query) return "unknown";

    char lower[4096];
    int i = 0;
    for (const char *p = query; *p && i < (int)sizeof(lower) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z')
            lower[i++] = *p + 32;
        else if ((*p >= 'a' && *p <= 'z') || *p == ' ')
            lower[i++] = *p;
    }
    lower[i] = '\0';

    const char *target = NULL;

    const char *words[] = {"firefox", "chrome", "terminal", "calculator",
                           "gimp", "libreoffice", "vlc", "nautilus",
                           "gedit", "kate", "code", "spotify", NULL};

    char *last_word = NULL;
    char *save = NULL;
    char *token = strtok_r(lower, " ", &save);
    while (token) {
        for (int w = 0; words[w]; w++) {
            if (strcmp(token, words[w]) == 0) {
                target = words[w];
            }
        }
        last_word = token;
        token = strtok_r(NULL, " ", &save);
    }

    if (target) return target;

    if (last_word && (strcmp(last_word, "ac") == 0 || strcmp(last_word, "aç") == 0))
        return NULL;

    if (last_word) return last_word;

    return NULL;
}

static const char *extract_action(const char *query) {
    if (!query) return "unknown";

    char lower[4096];
    int i = 0;
    for (const char *p = query; *p && i < (int)sizeof(lower) - 1; p++) {
        if (*p >= 'A' && *p <= 'Z')
            lower[i++] = *p + 32;
        else if ((*p >= 'a' && *p <= 'z') || *p == ' ')
            lower[i++] = *p;
    }
    lower[i] = '\0';

    if (strstr(lower, "ac") || strstr(lower, "aç") ||
        strstr(lower, "open") || strstr(lower, "launch") ||
        strstr(lower, "start") || strstr(lower, "run"))
        return "open_application";

    if (strstr(lower, "bagla") || strstr(lower, "bağla") || strstr(lower, "bahla") ||
        strstr(lower, "close") || strstr(lower, "kill") || strstr(lower, "stop"))
        return "close_application";

    if (strstr(lower, "install") || strstr(lower, "qur") || strstr(lower, "kur"))
        return "install_package";

    if (strstr(lower, "remove") || strstr(lower, "sil") || strstr(lower, "kaldir"))
        return "remove_package";

    if (strstr(lower, "search") || strstr(lower, "ara") || strstr(lower, "find"))
        return "search_package";

    if (strstr(lower, "wifi") || strstr(lower, "network"))
        return "network_management";

    if (strstr(lower, "notify") || strstr(lower, "bildirim"))
        return "notifications";

    return "query";
}

int main(int argc, char *argv[]) {
    const char *model = NULL;
    const char *prompt = NULL;
    const char *format = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            model = argv[++i];
        else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc)
            prompt = argv[++i];
        else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc)
            format = argv[++i];
    }

    (void)model;
    (void)format;

    if (!prompt) {
        fprintf(stderr, "Usage: %s --model <path> --prompt <text> [--format json]\n", argv[0]);
        return 1;
    }

    const char *action = extract_action(prompt);
    const char *target = classify_with_rules(prompt);

    float confidence = 0.85f;

    if (target) {
        print_json_intent(action, target, confidence);
    } else {
        char query_copy[4096];
        snprintf(query_copy, sizeof(query_copy), "%s", prompt);
        char *last = NULL;
        char *save = NULL;
        char *tok = strtok_r(query_copy, " ", &save);
        while (tok) { last = tok; tok = strtok_r(NULL, " ", &save); }
        if (last && (strcmp(last, "ac") == 0 || strcmp(last, "aç") == 0)) {
            tok = strtok_r(query_copy, " ", &save);
            print_json_intent(action, tok ? tok : "unknown", 0.75f);
        } else {
            print_json_intent(action, prompt, 0.50f);
        }
    }

    return 0;
}
