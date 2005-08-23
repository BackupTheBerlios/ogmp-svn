# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = eXosipua.la
eXosipua_la_SOURCES = eXosipua.c jcalls.c jinsubscriptions.c jsubscriptions.c jauth.c
eXosipua_la_LDFLAGS = -avoid-version -module
eXosipua_la_LIBADD = -losip2 -losipparser2 -leXosip $(top_builddir)/libogmp/libogmp.la
