# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = fmt_ogm.la
fmt_ogm_la_SOURCES = ogm_common.c ogm_format.c
fmt_ogm_la_LDFLAGS = -avoid-version -module
fmt_ogm_la_LIBADD =  -logg $(top_builddir)/libogmp/libogmp.la
