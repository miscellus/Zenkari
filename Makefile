run: zenkari
	./zenkari

libraylib.a:
	cd ./raylib/src && make raylib PLATFORM=PLATFORM_DESKTOP

zenkari: zenkari.c libraylib.a
	gcc -Wall -Wextra -pedantic -std=c99 -O0 -ggdb -I ./raylib/src -o zenkari zenkari.c -L ./raylib/src -lraylib -lraylib -lGL -lm -lpthread -ldl -lrt
