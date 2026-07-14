CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -pthread -Wno-format-truncation
LDFLAGS := -pthread -lm
TARGET  := ai-orchestrator
LLM_INFER := llm-inference
AGENTS  := desktop-agent package-agent network-agent

SRCDIR  := src
INCDIR  := include

ORCH_MODULES := logger config protocol ipc registry router policy scheduler session metrics llm
ORCH_SRCS    := main.c \
                $(foreach m,$(ORCH_MODULES),$m/$m.c) \
                src/recovery.c
ORCH_OBJS    := $(ORCH_SRCS:.c=.o)

AGENT_SRCS := $(addprefix agents/, $(addsuffix .c, $(AGENTS)))
AGENT_OBJS := $(AGENT_SRCS:.c=.o)

LIB_SRCS := lib/agent_client.c
LIB_OBJS := $(LIB_SRCS:.c=.o)

LLM_INFER_SRCS := llm/inference.c
LLM_INFER_OBJS := $(LLM_INFER_SRCS:.c=.o)

TEST_SRCS := tests/test_logger.c tests/test_protocol.c tests/test_registry.c tests/test_policy.c
TEST_BINS := $(TEST_SRCS:.c=)

.PHONY: all clean install uninstall agents tests test

all: $(TARGET) $(LLM_INFER) agents
	@echo "=== Build complete ==="

$(TARGET): $(ORCH_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  CC  $@"

$(LLM_INFER): $(LLM_INFER_OBJS)
	$(CC) $(CFLAGS) -o llm/$@ $^
	@echo "  CC  llm/$@"

agents: $(AGENTS)
	@true

$(AGENTS): %: agents/%.o $(LIB_OBJS) logger/logger.o
	$(CC) $(CFLAGS) -o agents/$@ $^ $(LDFLAGS)
	@echo "  CC  agents/$@"

%.o: %.c
	$(CC) $(CFLAGS) -I. -I$(INCDIR) -c -o $@ $<
	@echo "  CC  $<"

tests: $(TEST_BINS)
	@echo "=== Running tests ==="
	@for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t || exit 1; \
	done
	@echo "=== All unit tests passed ==="

tests/test_logger: tests/test_logger.o logger/logger.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_protocol: tests/test_protocol.o protocol/protocol.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_registry: tests/test_registry.o registry/registry.o logger/logger.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_policy: tests/test_policy.o policy/policy.o logger/logger.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(ORCH_OBJS) $(TARGET)
	rm -f $(LLM_INFER_OBJS) llm/$(LLM_INFER)
	rm -f $(AGENT_OBJS) $(addprefix agents/, $(AGENTS))
	rm -f $(LIB_OBJS)
	rm -f $(TEST_SRCS:.c=.o) $(TEST_BINS)
	@echo "  clean done"

install: $(TARGET) agents
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
	install -d $(DESTDIR)/usr/local/lib/ai-orchestrator/agents
	for a in $(AGENTS); do install -m 755 agents/$$a $(DESTDIR)/usr/local/lib/ai-orchestrator/agents/; done
	install -m 755 llm/$(LLM_INFER) $(DESTDIR)/usr/local/lib/ai-orchestrator/$(LLM_INFER)
	install -d $(DESTDIR)/etc
	install -m 644 config.toml.example $(DESTDIR)/etc/ai-orchestrator.toml
	install -d $(DESTDIR)/etc/systemd/system
	install -m 644 systemd/ai-orchestrator.service $(DESTDIR)/etc/systemd/system/
	install -m 644 systemd/ai-orchestrator.socket $(DESTDIR)/etc/systemd/system/
	install -m 644 systemd/ai-orchestrator-agent@.service $(DESTDIR)/etc/systemd/system/
	@echo "  install complete"

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	for a in $(AGENTS); do rm -f $(DESTDIR)/usr/local/lib/ai-orchestrator/agents/$$a; done
	rm -f $(DESTDIR)/usr/local/lib/ai-orchestrator/$(LLM_INFER)
	rm -f $(DESTDIR)/etc/ai-orchestrator.toml
	rm -f $(DESTDIR)/etc/systemd/system/ai-orchestrator.service
	rm -f $(DESTDIR)/etc/systemd/system/ai-orchestrator.socket
	rm -f $(DESTDIR)/etc/systemd/system/ai-orchestrator-agent@.service
	@echo "  uninstall complete"
