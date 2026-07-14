CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -O2 -pthread -Wno-format-truncation
LDFLAGS := -pthread -lm
TARGET  := ai-orchestrator

SRCDIR  := src
INCDIR  := include

MODULES := logger config protocol ipc registry router scheduler session metrics

SRCS    := main.c \
           $(foreach m,$(MODULES),$m/$m.c)

OBJS    := $(SRCS:.c=.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I. -I$(INCDIR) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
	install -d $(DESTDIR)/etc
	install -m 644 config.toml.example $(DESTDIR)/etc/ai-orchestrator.toml

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)
	rm -f $(DESTDIR)/etc/ai-orchestrator.toml
