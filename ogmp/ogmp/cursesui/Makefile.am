# Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

CPPFLAGS = -I$(top_srcdir)

libdir = $(OGMP_MODULEDIR)
 
lib_LTLIBRARIES = cursesui.la
cursesui_la_SOURCES = editor.c gui_address_book_browse.c gui_address_book_newentry.c gui_audio_test.c \
			gui.c gui_icon.c gui_insubscriptions_list.c gui_login.c gui_loglines.c gui_menu.c \
			gui_message.c gui_new_call.c gui_new_identity.c gui_new_user.c gui_online.c gui_profiles.c \
			gui_sessions_list.c gui_setup.c gui_subscriptions_list.c \
			gui_topline.c gui_preference.c
cursesui_la_LDFLAGS = -avoid-version -module
cursesui_la_LIBADD = -lncurses -lXaw -lXmu -lXt -lX11 -lSM -lICE -lXext $(top_builddir)/libogmp/libogmp.la
