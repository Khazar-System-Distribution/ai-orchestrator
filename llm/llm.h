#ifndef LLM_H
#define LLM_H

#include "../include/common.h"

#define LLM_MAX_OUTPUT 4096
#define MODEL_PATH_MAX 512

typedef struct {
    char action[128];
    char target[256];
    char parameters[1024];
    float confidence;
} llm_intent_t;

typedef struct llm_engine llm_engine_t;

llm_engine_t *llm_init(const char *model_path, const char *inference_binary);
int           llm_classify(llm_engine_t *eng, const char *query, llm_intent_t *intent);
bool          llm_is_available(llm_engine_t *eng);
void          llm_cleanup(llm_engine_t *eng);

#endif
