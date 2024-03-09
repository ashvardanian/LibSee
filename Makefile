# Define the compiler
CC := gcc

# Source file
SRC := libsee.c

# Object file
OBJ := $(SRC:.c=.o)

# Output library name
LIB_NAME := libsee

# Detect OS (Linux, Darwin (macOS), Windows, etc)
UNAME_S := $(shell uname -s)

# Default to release build
DEBUG ?= 0

# Set the flags for debug and release builds
ifeq ($(DEBUG), 1)
    CFLAGS += -g -DDEBUG
else
    CFLAGS += -O2
endif

ifeq ($(UNAME_S), Linux)
    # Linux
    LIB_EXT := .so
    CFLAGS += -fPIC -nostdlib -nostartfiles
    LDFLAGS := -shared
endif
ifeq ($(UNAME_S), Darwin)
    # macOS
    LIB_EXT := .dylib
    CFLAGS += -fPIC -nostdlib -nostartfiles
    LDFLAGS := -dynamiclib
endif
ifeq ($(OS), Windows_NT)
    # Windows
    LIB_EXT := .dll
    CFLAGS += -nostdlib -nostartfiles
    LDFLAGS := -shared
endif

# Output library
LIB := $(LIB_NAME)$(LIB_EXT)

# Compilation rules
all: $(LIB)

debug:
	$(MAKE) all DEBUG=1

$(LIB): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB)

.PHONY: all clean debug
