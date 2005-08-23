# not a GNU package. You can remove this line, if
# have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.4

SUBDIRS = misc include libogmp ogmp

noinst_HEADERS = config.h

CONFIG_CLEAN_FILES = libtool-nofpic

debug:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit;\
	done;

debug-install: install-debug

install-debug: debug
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit; \
	done;
	$(MAKE) $(AM_MAKEFLAGS) install-data-hook

install-data-hook:
	@rm -f $(DESTDIR)$(OGMP_MODULEDIR)/*.la
	@rm -f $(DESTDIR)$(OGMP_MODULEDIR)/*/*.la
	@if test -x "$(top_srcdir)/post-install.sh" ; then \
	  $(top_srcdir)/post-install.sh ; \
	fi

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in configure acinclude.m4 aclocal.m4
	-rm -f config.h.in stamp-h.in ltconfig ltmain.sh
	-rm -f config.guess config.sub install-sh missing mkinstalldirs
	-rm -f libtool-nofpic depcomp config.log

maintainer-clean-generic-hook: 
	rm -f config.status
