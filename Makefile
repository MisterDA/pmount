LIBS=-lsysfs
CFLAGS?=-Wall -W -O2
HAL_CFLAGS=$(shell pkg-config hal-storage --cflags)
HAL_LDFLAGS=$(shell pkg-config hal-storage --libs)
VERSION=$(shell head -n 1 CHANGES)
PREFIX?=/usr/local

pmount_OBJ = pmount.o policy.o utils.o fs.o luks.o
pumount_OBJ = pumount.o policy.o utils.o luks.o
pmount_hal_OBJ = pmount-hal.o policy.o utils.o fs.o

all: pmount pumount pmount-hal po/pmount.pot

pmount: $(pmount_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

pumount: $(pumount_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

pmount-hal.o: pmount-hal.c
	$(CC) -c $(CFLAGS) $(HAL_CFLAGS) $<

pmount-hal: $(pmount_hal_OBJ)
	$(CC) $(LDFLAGS) $(HAL_LDFLAGS) $^ $(LIBS) -o $@

install: all install-mo
	install -o root -g root -m 4755 -D ./pmount $(DESTDIR)/$(PREFIX)/bin/pmount
	install -o root -g root -m 4755 -D ./pumount $(DESTDIR)/$(PREFIX)/bin/pumount
	install -o root -g root -m 755 -D ./pmount-hal $(DESTDIR)/$(PREFIX)/bin/pmount-hal
	install -o root -g root -m 644 -D ./pmount.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pmount.1
	install -o root -g root -m 644 -D ./pumount.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pumount.1
	install -o root -g root -m 644 -D ./pmount-hal.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pmount-hal.1
	install -o root -g root -m 644 -D ./pmount.allow $(DESTDIR)/etc/pmount.allow

uninstall: uninstall-mo
	rm -f $(DESTDIR)/$(PREFIX)/bin/pmount $(DESTDIR)/$(PREFIX)/bin/pumount $(DESTDIR)/$(PREFIX)/bin/pmount-hal
	rm -f $(DESTDIR)/$(PREFIX)/share/man/man1/pmount.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pumount.1 
	rm -f $(DESTDIR)/$(PREFIX)/share/man/man1/pmount-hal.1

install-mo:
	for f in po/*.po; do P="$(DESTDIR)/$(PREFIX)/share/locale/$$(basename $${f%.po})/LC_MESSAGES/"; mkdir -p "$$P"; msgfmt -o "$$P/pmount.mo" $$f; done

uninstall-mo:
	for f in po/*.po; do P="$(DESTDIR)/$(PREFIX)/share/locale/$$(basename $${f%.po})/LC_MESSAGES/"; rm -f "$$P/pmount.mo"; rmdir -p --ignore-fail-on-non-empty "$$P"; done

clean:
	rm -f pmount pumount pmount-hal $(pmount_OBJ) $(pumount_OBJ) $(pmount_hal_OBJ) po/pmount.pot

po/pmount.pot:
	xgettext -k_ -o po/pmount.pot --copyright-holder "Martin Pitt" --msgid-bugs-address="martin.pitt@canonical.com" *.c

updatepo: po/pmount.pot
	for f in po/*.po; do echo -n "updating $$f "; msgmerge -U $$f po/pmount.pot; done

dist: clean
	mkdir ../pmount-$(VERSION)
	find -type f ! -path '*.bzr*' ! -name . | cpio -pd ../pmount-$(VERSION)
	cd ..; tar cv pmount-$(VERSION) | gzip -9 > "pmount-$(VERSION).tar.gz"; rm -r pmount-$(VERSION)

# dependencies
policy.o: policy.h utils.h
pmount.o: policy.h utils.h fs.h luks.h
pumount.o: policy.h utils.h luks.h
utils.o: utils.h
fs.o: fs.h
pmount-hal.o: policy.h

