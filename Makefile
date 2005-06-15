LIBS=-lsysfs
CFLAGS?=-Wall -W -O2
PREFIX?=/usr/local

pmount_OBJ = pmount.o policy.o utils.o fs.o
pumount_OBJ = pumount.o policy.o utils.o

all: pmount pumount

pmount: $(pmount_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

pumount: $(pumount_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

install: pmount pumount install-mo
	install -o root -g root -m 4755 -D ./pmount $(DESTDIR)/$(PREFIX)/bin/pmount
	install -o root -g root -m 4755 -D ./pumount $(DESTDIR)/$(PREFIX)/bin/pumount
	install -o root -g root -m 755 -D ./pmount-hal $(DESTDIR)/$(PREFIX)/bin/pmount-hal
	install -o root -g root -m 644 -D ./pmount.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pmount.1
	install -o root -g root -m 644 -D ./pumount.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pumount.1
	install -o root -g root -m 644 -D ./pmount-hal.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pmount-hal.1

uninstall: uninstall-mo
	rm -f $(DESTDIR)/$(PREFIX)/bin/pmount $(DESTDIR)/$(PREFIX)/bin/pumount $(DESTDIR)/$(PREFIX)/bin/pmount-hal
	rm -f $(DESTDIR)/$(PREFIX)/share/man/man1/pmount.1 $(DESTDIR)/$(PREFIX)/share/man/man1/pumount.1 
	rm -f $(DESTDIR)/$(PREFIX)/share/man/man1/pmount-hal.1

install-mo:
	for f in po/*.po; do P="$(DESTDIR)/$(PREFIX)/share/locale/$$(basename $${f%.po})/LC_MESSAGES/"; mkdir -p "$$P"; msgfmt -o "$$P/pmount.mo" $$f; done

uninstall-mo:
	for f in po/*.po; do P="$(DESTDIR)/$(PREFIX)/share/locale/$$(basename $${f%.po})/LC_MESSAGES/"; rm -f "$$P/pmount.mo"; rmdir -p --ignore-fail-on-non-empty "$$P"; done

clean:
	rm -f pmount pumount $(pmount_OBJ) $(pumount_OBJ)

updatepo:
	xgettext -k_ -o po/template.pot --copyright-holder "Martin Pitt" --msgid-bugs-address="martin.pitt@canonical.com" *.c
	for f in po/*.po; do echo -n "updating $$f "; msgmerge -U $$f po/template.pot; done

# dependencies
policy.o: policy.h utils.h
pmount.o: policy.h utils.h fs.h
pumount.o: policy.h utils.h
utils.o: utils.h
fs.o: fs.h

