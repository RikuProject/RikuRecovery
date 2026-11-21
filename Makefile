# Хостовая сборка:        make
# Статическая для образа: make STATIC=1
# Кросс-сборка (NDK):     make CC=armv7a-linux-androideabi21-clang STATIC=1
#                         make CC=aarch64-linux-android21-clang   STATIC=1

CC      ?= gcc
CFLAGS  ?= -O2
CFLAGS  += -std=gnu11 -Wall -Wextra -Wformat=2
ifeq ($(STATIC),1)
LDFLAGS += -static
endif

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

all: build/recovery

build/recovery: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

build/%.o: src/%.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean

# Тесты без железа: разбор тач-событий
test: build/test_touch
	./build/test_touch

build/test_touch: tests/test_touch.c src/input.c src/input.h src/log.c src/log.h | build
	$(CC) $(CFLAGS) -Isrc -o $@ tests/test_touch.c src/log.c

.PHONY: test
