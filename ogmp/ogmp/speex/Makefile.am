# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)
 
libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = speex_rtp.la speex_player.la speex_maker.la \
		speex_sender.la ogm_speex.la

speex_rtp_la_SOURCES = speex_rtp.c speex_sdp.c speex_config.c
speex_rtp_la_LDFLAGS = -avoid-version -module
speex_rtp_la_LIBADD =  -lxrtp -lspeex $(top_builddir)/libogmp/libogmp.la

ogm_speex_la_SOURCES = ogm_speex.c
ogm_speex_la_LDFLAGS = -avoid-version -module
ogm_speex_la_LIBADD =  -lspeex $(top_builddir)/libogmp/libogmp.la

speex_player_la_SOURCES = speex_codec.c speex_player.c
speex_player_la_LDFLAGS = -avoid-version -module
speex_player_la_LIBADD =  -lspeex $(top_builddir)/libogmp/libogmp.la

speex_maker_la_SOURCES = speex_codec.c speex_maker.c
speex_maker_la_LDFLAGS = -avoid-version -module
speex_maker_la_LIBADD =  -lspeex $(top_builddir)/libogmp/libogmp.la

speex_sender_la_SOURCES = speex_config.c speex_sender.c
speex_sender_la_LDFLAGS = -avoid-version -module
speex_sender_la_LIBADD =  -lxrtp -lspeex $(top_builddir)/libogmp/libogmp.la
