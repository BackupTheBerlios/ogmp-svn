# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = dev_portaudio.la dev_rtp.la

dev_portaudio_la_SOURCES = timed_pipe.c dev_portaudio.c
dev_portaudio_la_LDFLAGS = -avoid-version -module
dev_portaudio_la_LIBADD = -lportaudio $(top_builddir)/libogmp/libogmp.la

dev_rtp_la_SOURCES = dev_rtp.c
dev_rtp_la_LDFLAGS = -avoid-version -module
dev_rtp_la_LIBADD = -lxrtp $(top_builddir)/libogmp/libogmp.la
