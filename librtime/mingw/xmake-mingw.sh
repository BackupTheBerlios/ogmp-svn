#!/bin/sh

PREFIX="/usr/local/cross-tools"
TARGET=i386-mingw32msvc

PATH="$PREFIX/bin:$PREFIX/$TARGET/bin:$PATH"
export PATH

i386-mingw32msvc-gcc -o librtime.exe t_time.c spu_text.c timer_win32.c xthread_sdl.c \
 -I/home/hling/platform/i386-mingw32msvc/include        \
 -L/home/hling/platform/i386-mingw32msvc/lib -lSDL
