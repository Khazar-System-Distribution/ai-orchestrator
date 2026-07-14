# AI Orchestrator v0.1

AI Orchestrator — Khazar System Distribution üçün mərkəzi AI koordinasiya daemonudur.

## Məqsəd

Bu daemon bütün istifadəçi sorğularının giriş nöqtəsidir. Təbii dil sorğularını qəbul edir, emal edir və uyğun agentlərə yönləndirir. Orchestrator özü **heç vaxt** əməliyyat icra etmir — o yalnız koordinasiya edir.

## Arxitektura

```
┌──────────┐
│  Client  │
└────┬─────┘
     │ Unix Domain Socket (JSON-RPC)
┌────▼──────────────────────────────────────────────────┐
│                  AI Orchestrator                       │
│  ┌─────────┐  ┌────────┐  ┌──────────┐  ┌──────────┐ │
│  │   IPC   │─▶│Protocol│─▶│  Router  │─▶│Scheduler │ │
│  └─────────┘  └────────┘  └────┬─────┘  └────┬─────┘ │
│                                │              │       │
│  ┌──────────┐  ┌──────────┐   │              │       │
│  │ Registry │  │  Config  │   │              │       │
│  └──────────┘  └──────────┘   │              │       │
│  ┌──────────┐  ┌──────────┐   │              │       │
│  │ Session  │  │ Metrics  │   │              │       │
│  └──────────┘  └──────────┘   │              │       │
│  ┌──────────┐                 │              │       │
│  │  Logger  │◀────────────────┴──────────────┴───────│
│  └──────────┘                                         │
└───────────────────────────────────────────────────────┘
     │
┌────▼──────────────────────────────────────────────────┐
│              Agent Layer (external)                    │
│  ┌────────────┐  ┌────────────┐  ┌──────────────────┐ │
│  │Desktop-Agent│  │Package-Agent│  │ Network-Agent   │ │
│  └────────────┘  └────────────┘  └──────────────────┘ │
└───────────────────────────────────────────────────────┘
```

## Modullar

### include/ — Ümumi tiplər və təriflər
Bütün modullar tərəfindən istifadə olunan məlumat strukturları, enumlar (log səviyyəsi, status, error code, priority), sabitlər (`MAX_CONNECTIONS`, `MAX_MESSAGE_SIZE` və s.) və request/response structları.

### logger/ — Jurnal sistemi
Mərkəzləşdirilmiş, thread-safe log sistemi. Altı səviyyə: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`. Log formatı:
```
2026-07-14T21:15:31Z INFO module mesaj
```
`FATAL` səviyyəsində avtomatik `exit()` çağırılır.

### config/ — Konfiqurasiya
Minimal TOML parser. Parametrlər: `socket_path`, `max_connections`, `request_timeout_ms`, `enable_metrics`, `worker_threads`, `log_level`, `log_file`. Default dəyərlər `common.h`-də müəyyən edilib.

### protocol/ — JSON-RPC protokolu
Sorğu, cavab və xəta mesajları üçün JSON serializasiya/deserializasiya. Format:
- Sorğu: `{"id":1,"type":"request","payload":{"query":"firefox aç"}}`
- Cavab: `{"id":1,"status":"success","payload":{"message":"..."}}`
- Xəta: `{"id":1,"status":"error","error_code":"AGENT_UNAVAILABLE"}`

### ipc/ — Unix Domain Socket serveri
**epoll** əsaslı, non-blocking I/O ilə UDS server. Xüsusiyyətlər:
- Socket yolu: `/run/ai-orchestrator.sock`
- Permission: `0660`
- Maksimum 256 paralel bağlantı
- Avtomatik təmizləmə (socket faylı silinir)

### registry/ — Agent qeydiyyat sistemi
Agentlər startup zamanı özlərini qeydiyyatdan keçirir. Capability (bacarıq) əsaslı axtarış. Hər agentin adı, versiyası, socket yolu və bacarıqları saxlanılır.

### router/ — Qayda əsaslı intent klassifikatoru
Regex pattern matching ilə sorğuları təsnif edir. Sadə əmrlər üçün LLM tələb olunmur. Nümunə qaydalar:
- `aç|open|launch` → `open_application`
- `qur|install` → `install_package`
- `wifi|network` → `network_management`

### scheduler/ — İşçi thread hovuzu
Prioritet növbəsi (`CRITICAL=0`, `HIGH=1`, `NORMAL=2`, `BACKGROUND=3`) ilə tapşırıqların icrasını idarə edir. Default 4 worker thread. Timeout mexanizmi daxildir.

### session/ — Sessiya meneceri
İstifadəçi sessiyalarının kontekstini və tarixçəsini saxlayır. Məsələn "onu bağla" sorğusundakı "onu" referansının nə olduğunu session manager müəyyən edir. Default timeout: 300 saniyə.

### metrics/ — Performans metrikaları
Toplanan metrikalar:
- request latency (ortalama/maksimum)
- success/failure count
- queue size
- agent failures
- policy denied count
- active connections

## IPC Dizaynı

| Parametr | Dəyər |
|----------|-------|
| Kommunikasiya | Unix Domain Socket |
| Format | JSON-RPC |
| Maksimum mesaj ölçüsü | 64 KB |
| Maksimum bağlantı | 256 |
| Request timeout | 3000 ms |

## Request Lifecycle

```
User → Orchestrator → Rule Engine → Intent Classifier → Policy Engine → Agent → Response
```

1. **IPC** sorğunu qəbul edir
2. **Protocol** JSON-u parse edir
3. **Router** intent-i müəyyən edir (əvvəlcə regex qaydaları, sonra LLM)
4. **Registry** uyğun agenti tapır
5. **Scheduler** sorğunu növbəyə qoyur
6. Worker thread agentə göndərir
7. **Session** tarixçəyə əlavə edir
8. **Metrics** məlumatları toplayır
9. Cavab istifadəçiyə qaytarılır

## Thread Model

| Thread | Məsuliyyət |
|--------|------------|
| Main thread | `accept()` + event loop (epoll) |
| Worker pool (4) | request parsing, routing, validation |
| Agent I/O | Ayrıca thread pool vasitəsilə |

## Zero Trust Architecture

Heç bir agentə tam etibar edilmir. Hər request:
1. Rule Engine-dən keçir
2. Intent Classifier tərəfindən təhlil edilir
3. Policy Engine icazələri yoxlayır
4. Yalnız ondan sonra agentə çatdırılır

## Local First Design

Bütün inference lokal həyata keçirilir. İnternet bağlantısı olmadan tam işləyir. Idle vəziyyətdə <50 MB RAM hədəfi.

## Quraşdırma

```bash
# Build
make

# Install (root)
sudo make install

# Konfiqurasiya
cp config.toml.example /etc/ai-orchestrator.toml
# Redaktə edin: socket yolu, log səviyyəsi, və s.

# İşə salma
ai-orchestrator /etc/ai-orchestrator.toml
```

## Test

```bash
echo '{"type":"ping"}' | socat - UNIX-CONNECT:/run/ai-orchestrator.sock
# Cavab: {"status":"alive"}
```

## Milestones

### Milestone 1 — MVP
- [x] Unix Domain Socket dinlənir
- [x] Ping → `{"status":"alive"}` cavabı
- [x] Bütün modullar init/cleanup edir

### Milestone 2 — Request Routing
- [ ] Regex əsaslı intent classification
- [ ] Agent seçimi (registry)
- [ ] Mock agent cavabı

### Milestone 3 — Real IPC
- [ ] Desktop agent ilə real inteqrasiya
- [ ] Policy Engine
- [ ] LLM inference pipeline

## Asılılıqlar

- C11 compiler (gcc/clang)
- POSIX thread (pthread)
- Linux kernel (epoll üçün)
- socat (test üçün)

## License

Bu layihə Khazar System Distribution-in tərkib hissəsidir.
