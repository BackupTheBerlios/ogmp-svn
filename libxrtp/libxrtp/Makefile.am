# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
lib_LTLIBRARIES = libxrtp.la
libxrtp_la_SOURCES = udp.c media.c packet.c pipeline.c \
			portman.c psched_simple.c session.c xrtp.c

pkglibdir = $(libdir)/ogmp/modules/
libxrtp_la_LDFLAGS = @TMD_CFLAGS@ -no-undefines -version-info 0:1:0
libxrtp_la_LIBADD = @TMD_LDFLAGS@

pkgincludedir = $(includedir)/xrtp
pkginclude_HEADERS = xrtp.h session.h psched.h pipeline.h handler.h \
			packet.h packet_const.h connect.h media.h const.h 


