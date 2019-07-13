EXTRA_DIST = pmount.allow

install-data-hook:
	test -z $(DESTDIR)/etc || mkdir -p -- $(DESTDIR)/etc
	$(INSTALL_DATA) $(srcdir)/pmount.allow $(DESTDIR)/etc/pmount.allow

uninstall-hook:
	rm -f $(DESTDIR)/etc/pmount.allow
