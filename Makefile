CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -pthread -Wno-format-truncation
LDFLAGS := -pthread -lm
TARGET  := ai-orchestrator

INCDIR  := include

MODULES := logger config protocol ipc registry router session metrics
SRCS    := main.c $(foreach m,$(MODULES),$m/$m.c)
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean install uninstall

all: $(TARGET)
	@echo "=== Build complete ==="

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  LD  $@"

%.o: %.c
	$(CC) $(CFLAGS) -I. -I$(INCDIR) -c -o $@ $<
	@echo "  CC  $<"

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "  clean done"

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
	install -d $(DESTDIR)/etc
	install -m 644 config.toml.example $(DESTDIR)/etc/ai-orchestrator.toml
	@echo "  install complete"

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	rm -f $(DESTDIR)/etc/ai-orchestrator.toml
	@echo "  uninstall complete"
