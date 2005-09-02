# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir) -DOGMP_MODULEDIR=\"@OGMP_MODULEDIR@\"
 
bin_PROGRAMS = ogmp
ogmp_SOURCES = ogmp_main.c
ogmp_LDADD = $(top_builddir)/libogmp/libogmp.la
