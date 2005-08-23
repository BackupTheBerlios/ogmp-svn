# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = eXosipua2.la
eXosipua2_la_SOURCES = eXosipua.c jcalls.c jauth.c
eXosipua2_la_LDFLAGS = -avoid-version -module
eXosipua2_la_LIBADD = -losip2 -losipparser2 -leXosip2 $(top_builddir)/libogmp/libogmp.la
