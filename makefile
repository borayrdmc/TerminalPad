# Compiler
CC = gcc

# Source files
COMMON_SRCS = src/main.c \
              src/text-engine/buffer/buffer.c \
              src/text-engine/clipboard/clipboard.c \
              src/text-engine/cursor/cursor.c \
              src/text-engine/selection/selection.c \
              src/search/search.c \
              src/ui/renderer.c \
              src/file-manager/file_manager.c \
              src/undo/undo_stack.c

# Platform detection
ifeq ($(OS), Windows_NT)
    TARGET = TerminalPad.exe
    SRCS   = $(COMMON_SRCS) src/terminal/terminal_windows.c
    CFLAGS = -Wall -Wextra -g -D_WIN32 -I src
    LIBS   = -lkernel32 -luser32
    RM     = del /Q
else
    TARGET = TerminalPad
    SRCS   = $(COMMON_SRCS) src/terminal/terminal_linux.c
    CFLAGS = -Wall -Wextra -g -I src
    LIBS   =
    RM     = rm -f
endif

# Targets
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	$(RM) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
