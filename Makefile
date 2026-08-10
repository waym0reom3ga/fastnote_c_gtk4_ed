# FastNote C/GTK4 Edition — Build system

CC = gcc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags gtk4)
LDFLAGS = $(shell pkg-config --libs gtk4)

SRCDIR = src
BUILDDIR = build
TARGET = fastnote-c-gtk4

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

test: all
	./$(TARGET) --version
	./$(TARGET) --headless --selftest

install: all
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/
