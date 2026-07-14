# AI Orchestrator v0.2

AI Orchestrator — Khazar System Distribution üçün mərkəzi AI koordinasiya daemonudur.
Təbii dil sorğularını qəbul edir, emal edir və uyğun agentlərə yönləndirir. Özü **heç vaxt** əməliyyat icra etmir.

## Arxitektura

```
┌──────────┐
│  Client  │
└────┬─────┘
     │ Unix Domain Socket (JSON-RPC)
┌────▼──────────────────────────────────────────────────────────┐
│                     AI Orchestrator                            │
│  ┌─────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │   IPC   │─▶│ Protocol │─▶│  Router  │─▶│  LLM     │       │
│  │ (epoll) │  │ (JSON)   │  │ (regex)  │  │ (GGUF)   │       │
│  └─────────┘  └──────────┘  └────┬─────┘  └──────────┘       │
│                                  │                            │
│  ┌──────────┐  ┌──────────┐     ▼                            │
│  │ Registry │  │  Policy  │ ┌──────────┐  ┌──────────┐       │
│  │ (agents) │  │ (Zero    │ │Scheduler │  │ Recovery │       │
│  └──────────┘  │  Trust)  │ │(threads) │  │(circuit  │       │
│  ┌──────────┐  └──────────┘ └────┬─────┘  │ breaker) │       │
│  │ Session  │                    │        └──────────┘       │
│  │(context) │                    ▼                            │
│  └──────────┘              ┌──────────┐                      │
│  ┌──────────┐  ┌──────────┐│  Agent   │──►Response to Client │
│  │ Metrics  │  │  Config  ││  Proxy   │                      │
│  └──────────┘  └──────────┘└──────────┘                      │
│  ┌───────────────────────────────────────────────────────┐   │
│  │                     Logger                             │   │
│  └───────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
     │
┌────▼──────────────────────────────────────────────────────────┐
│                    Agent Layer                                 │
│  ┌──────────────────┐  ┌────────────────┐  ┌────────────────┐ │
│  │  desktop-agent   │  │  package-agent │  │  network-agent │ │
│  │  DBus/xdg-open   │  │  APT/Pacman/   │  │  NetworkManager│ │
│  │  Notifications   │  │  Flatpak       │  │  nmcli         │ │
│  └──────────────────┘  └────────────────┘  └────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

## Modullar

### include/ — Ümumi tiplər
Structlar, enumlar (log level, status, error code, priority), sabitlər.

### logger/ — Jurnal sistemi
Thread-safe, 6 səviyyə (TRACE..FATAL). Format: `2026-07-14T21:15:31Z INFO module mesaj`

### config/ — Konfiqurasiya
TOML parser. Parametrlər: `socket_path`, `max_connections`, `request_timeout_ms`, `enable_metrics`, `worker_threads`, `log_level`, `log_file`.

### protocol/ — JSON-RPC
Mesaj formatları: request, response, error, ping, register.

### ipc/ — Unix Domain Socket
epoll əsaslı, non-blocking I/O. 256 paralel bağlantı. Socket: `/run/ai-orchestrator.sock` (0660).

### registry/ — Agent qeydiyyatı
Agent adı, versiya, socket yolu, capability-lər. Capability əsaslı axtarış.

### router/ — Intent klassifikatoru
Regex (POSIX) əsaslı 30 qayda. Türkçe + İngiliscə. Nümunə: `ac`, `aç`, `open` → `open_application`. Word boundary mühafizəsi var.

### policy/ — Təhlükəsizlik mühərriki
Zero Trust: default deny. Yalnız açıq allow qaydaları keçər. Hər action/agent/capability üçü ayrıca yoxlama.

### llm/ — LLM Pipeline
GGUF model inferensi üçün interface. `llm-inference` binary-si ayrıca proses olaraq işləyir. Regex tapmadıqda LLM-ə düşür. Model yoxdursa regex fallback.

### scheduler/ — Thread hovuzu
4 worker thread. Prioritet növbəsi (CRITICAL=0, HIGH=1, NORMAL=2, BACKGROUND=3).

### session/ — Sessiya meneceri
Default 300s timeout. Tarixçə (50 giriş). Referans həlli üçün kontekst.

### metrics/ — Metrikalar
Latency, queue size, agent failures, policy denials, active connections.

### src/recovery.c — Bərpa mexanizmi
Circuit breaker pattern. Exponential backoff. Avtomatik bərpa.

### lib/agent_client.h — Agent kitabxanası
Agentlərin orchestrator-a qoşulması üçün: connect, register, heartbeat, request/response.

### Agents

| Agent | Dil | Capabilities | Texnologiya |
|-------|-----|-------------|-------------|
| desktop-agent | C | open_application, close_application, notifications | DBus, xdg-open, pkill |
| package-agent | C | install_package, remove_package, search_package | APT, Pacman, Flatpak |
| network-agent | C | network_management | nmcli (NetworkManager) |

Hər agent `lib/agent_client` ilə orchestrator-a qoşulur, qeydiyyatdan keçir, sorğu gözləyir, cavab qaytarır. Bağlantı qoparsa avtomatik reconnect edir.

## Request Lifecycle

```
Client → IPC (epoll) → Protocol (JSON parse) → Router (regex match)
  → LLM (if no regex match, GGUF inference) → Policy (allow/deny check)
  → Registry (find agent by capability) → Scheduler (queue + thread)
  → Agent Proxy (forward via UDS) → Agent → Response → Client
```

Hər addım metrics və logger tərəfindən izlənir. Session konteksti saxlanır.

## Thread Model

| Thread | Məsuliyyət |
|--------|------------|
| Main | epoll event loop, accept |
| Worker pool (4) | parsing, routing, policy |
| Agent I/O | Ayrıca thread pool |

## Error Recovery

- **Circuit breaker**: 3 ardıcıl failure → circuit open
- **Exponential backoff**: 500ms → 1s → 2s → 4s → max 10s
- **Auto-recovery**: 10 saniyədən sonra circuit avtomatik bağlanır
- **Agent reconnect**: Agent qoparsa 5 dəfə retry edir

## Quraşdırma

```bash
# Build everything
make

# Unit tests
make tests

# Integration tests
cd tests && ./test_integration.sh

# Install (root)
sudo make install

# Systemd ilə işə salma
sudo systemctl enable --now ai-orchestrator.socket
sudo systemctl enable --now ai-orchestrator.service
sudo systemctl enable --now ai-orchestrator-agent@desktop-agent
sudo systemctl enable --now ai-orchestrator-agent@package-agent
sudo systemctl enable --now ai-orchestrator-agent@network-agent
```

## Test

```bash
# Ping
echo '{"type":"ping"}' | socat - UNIX-CONNECT:/run/ai-orchestrator.sock
# → {"status":"alive"}

# Intent routing
echo '{"type":"request","query":"firefox aç"}' | socat - UNIX-CONNECT:/run/ai-orchestrator.sock
# → {"id":0,"status":"success","payload":{"message":"routed to open_application","agent":"desktop-agent"}}
```

## Asılılıqlar

- C11 compiler, POSIX threads, Linux epoll
- Agentlər üçün: xdg-utils, dbus, nmcli, apt/pacman/flatpak
- LLM inference üçün: GGUF model + llama.cpp (opsional)

## Milestones

### ✅ Milestone 1 — MVP
Unix socket, ping/pong, modul init/cleanup.

### ✅ Milestone 2 — Request Routing
Regex intent classification, policy check, agent selection, mock response.

### Milestone 3 — LLM Integration
GGUF model inferensi, LLM intent fallback, confidence scoring.

### Milestone 4 — Real Agents
Desktop (DBus), Package (APT/Pacman), Network (nmcli) agentləri.

### Milestone 5 — Production
Systemd, logrotate, monitoring, error recovery, full test suite.
