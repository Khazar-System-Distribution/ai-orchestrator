# llm — LLM Intent Classification pipeline

- Orchestrator tərəfindən çağırılan inference subprocess-in idarə olunması.
- Regex ilə tapılmayan sorğuları GGUF modeli ilə təsnif edir.
- `inference.c` — ayrıca binary olaraq işləyir, orchestrator onu fork/exec edir.
- İşləmə prinsipi: query → subprocess → JSON intent → orchestrator.
- Model mövcud deyilsə avtomatik regex fallback-ə keçir.
