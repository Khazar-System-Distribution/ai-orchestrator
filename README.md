# AI Orchestrator

Khazar System Distribution AI platformasinin merkezi koordinasiya daemoni.

Butun istifadeci sorgulari ilk olaraq bu servis terefinden qebul edilir ve emal olunur.

Orchestrator hec vaxt emeliyyatlari ozu icra etmir. O yalniz koordinasiya edir.

## Funksiyalar

* Unix Domain Socket server
* Request router
* Session manager
* Agent discovery
* Agent communication
* JSON RPC server
* Logging
* Metrics

## Modullar

| Modul | Fayl(lar) | Funksiya |
|-------|-----------|----------|
| ipc/ | ipc.c, ipc.h | epoll esasli UDS server, 256 connection |
| protocol/ | protocol.c, protocol.h | JSON RPC message parse/build |
| router/ | router.c, router.h | Keyword esasli request dispatcher |
| registry/ | registry.c, registry.h | Agent qeydiyyati ve capability lookup |
| session/ | session.c, session.h | Istifadeci sessiyalari, context, history |
| logger/ | logger.c, logger.h | Thread-safe log (TRACE..FATAL) |
| metrics/ | metrics.c, metrics.h | Performans metrikalari |
| config/ | config.c, config.h | TOML konfiqurasiya |

## Asiliqliqlar

- C11 compiler (gcc/clang)
- POSIX threads
- Linux kernel (epoll)

## Build

```bash
make
```

## Test

```bash
echo '{"type":"ping"}' | socat - UNIX-CONNECT:/run/ai-orchestrator.sock
# {"status":"alive"}
```

## Install

```bash
sudo make install
ai-orchestrator /etc/ai-orchestrator.toml
```
