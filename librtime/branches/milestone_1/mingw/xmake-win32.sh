#!/bin/sh

PREFIX="/usr/local/cross-tools"
TARGET=i386-mingw32msvc

PATH="$PREFIX/bin:$PREFIX/$TARGET/bin:$PATH"
export PATH

if test -z $prefix; then
	prefix="$PREFIX/$TARGET"
	export prefix
fi

if test -z $dx8_include; then
	dx8_include="$PREFIX/$TARGET/include/dx8"
	export dx8_include
fi

if test -d "$PREFIX/$TARGET/lib/pkgconfig"; then
	PKG_CONFIG_PATH="$PREFIX/$TARGET/lib/pkgconfig"
	export PKG_CONFIG_PATH
fi

exec make $*
