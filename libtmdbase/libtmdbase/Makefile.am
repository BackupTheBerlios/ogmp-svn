# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

SUBDIRS = libsdl memwatch

CPPFLAGS = -I$(top_srcdir)
 
lib_LTLIBRARIES = libtmdbase.la
libtmdbase_la_SOURCES = buffer.c buffer.h byteorder.h catalog.c catalog.h \
	config.c config.h inet.h io.h list.c list.h loader.c loader.h md5.c md5.h os.h \
	socket.h socket_posix.c spinqueue.c spinqueue.h ui.h ui_stub.c util.h xmalloc.c \
	xmalloc.h xmap.c xmap.h xstring.c xstring.h xthread.h timer.h 
	#utf8.c utf8.h

libtmdbase_la_LDFLAGS = -no-undefines -version-info 0:1:0
libtmdbase_la_LIBADD = libsdl/libxth.la -ldl

pkgincludedir = $(includedir)/timedia
pkginclude_HEADERS = buffer.h byteorder.h catalog.h config.h inet.h io.h \
		list.h loader.h md5.h os.h socket.h spinqueue.c spinqueue.h \
		ui.h util.h xmalloc.h xmap.h xstring.h xthread.h timer.h utf8.h

