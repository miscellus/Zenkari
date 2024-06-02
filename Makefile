PLATFORM_OS ?= PLATORM_LINUX
PLATFORM_ARCHITECTURE ?= AMD64

ifeq ($(OS),Windows_NT)
    PLATFORM_OS = PLATORM_WINDOWS
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        PLATFORM_ARCHITECTURE = AMD64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            PLATFORM_ARCHITECTURE = AMD64
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            PLATFORM_ARCHITECTURE = IA32
        endif
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM_OS = PLATORM_WINDOWS
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM_OS = PLATORM_MACOS
    endif
    UNAME_P := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
        PLATFORM_ARCHITECTURE = AMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        PLATFORM_ARCHITECTURE = IA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
        PLATFORM_ARCHITECTURE = ARM
    endif
endif

CFLAGS += -DPLATFORM_OS=$(PLATFORM_OS)
CFLAGS += -DPLATFORM_ARCHITECTURE=$(PLATFORM_ARCHITECTURE)

run: zenkari
	./zenkari

libraylib.a:
	cd ./raylib/src && make raylib PLATFORM=PLATFORM_DESKTOP

zenkari: zenkari.c libraylib.a
	gcc -Wall -Wextra -pedantic -std=c99 -O0 -ggdb -I ./raylib/src -o zenkari zenkari.c -L ./raylib/src -lraylib -lraylib -lGL -lm -lpthread -ldl -lrt
