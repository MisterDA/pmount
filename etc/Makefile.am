EXTRA_DIST = pmount.allow pmount.conf

install-data-hook:
	test -z $(DESTDIR)/etc || mkdir -p -- $(DESTDIR)/etc
	$(INSTALL_DATA) $(srcdir)/pmount.allow $(DESTDIR)/etc/pmount.allow
	$(INSTALL_DATA) $(srcdir)/pmount.conf $(DESTDIR)/etc/pmount.conf

uninstall-hook:
	rm -f $(DESTDIR)/etc/pmount.allow
	rm -f $(DESTDIR)/etc/pmount.conf
