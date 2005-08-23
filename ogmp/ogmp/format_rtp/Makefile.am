# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = fmt_rtp.la
fmt_rtp_la_SOURCES = rtp_format.c
fmt_rtp_la_LDFLAGS = -avoid-version -module
fmt_rtp_la_LIBADD =  -lxrtp $(top_builddir)/libogmp/libogmp.la
