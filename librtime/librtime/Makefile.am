# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
lib_LTLIBRARIES = librtime.la
librtime_la_SOURCES = timer_posix.c
librtime_la_LDFLAGS = -no-undefines -version-info 0:1:0
librtime_la_LIBADD = @TMD_LDFLAGS@

pkgincludedir = $(includedir)/timedia
pkginclude_HEADERS = timer.h

