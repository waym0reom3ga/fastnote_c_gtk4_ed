# FastNote C/GTK4 Edition — Build system

CC = ccache gcc
CFLAGS = -Wall -Wextra $(shell pkg-config --cflags gtk4)
LDFLAGS = $(shell pkg-config --libs gtk4)

SRCDIR = src
BUILDDIR = build
TARGET = fastnote_c_gtk4

SOURCES = $(filter-out $(SRCDIR)/test_ui.c $(SRCDIR)/main.c,$(wildcard $(SRCDIR)/*.c))
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
GUI_OBJECTS = $(BUILDDIR)/main.o $(OBJECTS)

.PHONY: all clean test test-ui install

all: $(TARGET)

$(TARGET): $(GUI_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

test-ui: test_ui_bin
	./test_ui_bin

test_ui_bin: $(OBJECTS) $(SRCDIR)/test_ui.c
	$(CC) $(CFLAGS) -o $@ $(SRCDIR)/test_ui.c $(OBJECTS) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR) $(TARGET) test_ui_bin

test: all
	./$(TARGET) --version

install: all
	install -d /usr/local/bin
	install -m 755 $(TARGET) /usr/local/bin/