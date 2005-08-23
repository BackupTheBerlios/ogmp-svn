# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
lib_LTLIBRARIES = libogmp.la
libogmp_la_SOURCES = media_control.c ogmp_source.c ogmp_ui.c ogmp_config.c \
			sipua.c rtp_cap.c phonebook.c ogmp_client.c \
			media_format.h ogmp.h sipua.h rtp_cap.h \
			phonebook.h format_rtp.h dev_rtp.h

libogmp_la_CFLAGS = @XRTP_CFLAGS@ -DOGMP_MODULEDIR=$(OGMP_MODULEDIR)
libogmp_la_LDFLAGS = -no-undefines -version-info 0:1:0
libogmp_la_LIBADD = @XRTP_LDFLAGS@ -losip2

#pkgincludedir = $(includedir)/ogmp
#pkginclude_HEADERS = media_format.h ogmp.h sipua.h rtp_cap.h \
#			phonebook.h format_rtp.h dev_rtp.h
