CC ?= gcc
PLATFORM_OS ?= PLATFORM_OS_LINUX
PLATFORM_ARCHITECTURE ?= PLATFORM_ARCHITECTURE_AMD64

ifeq ($(OS),Windows_NT)
	PLATFORM_OS := PLATFORM_OS_WINDOWS
	ifeq ($(PROCESSOR_ARCHITEW6432),PLATFORM_ARCHITECTURE_AMD64)
		PLATFORM_ARCHITECTURE = PLATFORM_ARCHITECTURE_AMD64
	else
		ifeq ($(PROCESSOR_ARCHITECTURE),PLATFORM_ARCHITECTURE_AMD64)
			PLATFORM_ARCHITECTURE = PLATFORM_ARCHITECTURE_AMD64
		endif
		ifeq ($(PROCESSOR_ARCHITECTURE),x86)
			PLATFORM_ARCHITECTURE = PLATFORM_ARCHITECTURE_IA32
		endif
	endif
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		PLATFORM_OS := PLATFORM_OS_LINUX
	endif
	ifeq ($(UNAME_S),Darwin)
		PLATFORM_OS := PLATFORM_OS_MAC
	endif
	UNAME_P := $(shell uname -p)
	ifeq ($(UNAME_P),x86_64)
		PLATFORM_ARCHITECTURE = PLATFORM_ARCHITECTURE_AMD64
	endif
	ifneq ($(filter %86,$(UNAME_P)),)
		PLATFORM_ARCHITECTURE = PLATFORM_ARCHITECTURE_IA32
	endif
	ifneq ($(filter arm%,$(UNAME_P)),)
		PLATFORM_ARCHITECTURE = PLATFORM_ARCHITECTURE_ARM
	endif
endif

run: zenkari
	./zenkari

.PHONY: clean
clean:
	rm ./zenkari

libraylib.a:
	cd ./raylib/src && make raylib PLATFORM=PLATFORM_DESKTOP

LDFLAGS += -L ./raylib/src -lraylib -lm

ifeq ($(PLATFORM_OS),PLATFORM_OS_WINDOWS)
	LDFLAGS += -lopengl32 -lgdi32 -lwinmm
else
	LDFLAGS += -lGL -lpthread -ldl -lrt
endif

CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -std=c99
CFLAGS += -O0 -g3 -ggdb
CFLAGS += -I ./src -I ./raylib/src
CFLAGS += -DPLATFORM_OS=$(PLATFORM_OS)
CFLAGS += -DPLATFORM_ARCHITECTURE=$(PLATFORM_ARCHITECTURE)

zenkari: src/zenkari.c libraylib.a
	$(CC) $(CFLAGS) -o zenkari src/zenkari.c $(LDFLAGS)