SUBDIRS = etc man po src tests

EXTRA_DIST = \
       intltool-extract.in \
       intltool-update.in  \
       intltool-merge.in \
       README.devel

CLEANFILES = \
       intltool-extract    \
       intltool-update     \
       intltool-merge


# We make sure the lock directory exists, is a directory and is owned
# by root:root
install-exec-hook:
	rm -rf $(DESTDIR)$(LOCKDIR)
	mkdir -p $(DESTDIR)$(LOCKDIR)
	chown root:root $(DESTDIR)$(LOCKDIR)
	chmod 755 $(DESTDIR)$(LOCKDIR)

