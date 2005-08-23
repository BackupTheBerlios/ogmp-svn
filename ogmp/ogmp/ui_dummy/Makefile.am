# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = ui_dummy.la
ui_dummy_la_SOURCES = ui_dummy.c
ui_dummy_la_LDFLAGS = -avoid-version -module
ui_dummy_la_LIBADD =  $(top_builddir)/libogmp/libogmp.la
