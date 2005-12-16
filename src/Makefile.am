INCLUDES = $(HAL_CFLAGS)

noinst_LIBRARIES = libpmount-util.a
libpmount_util_a_SOURCES = \
	fs.c     \
	luks.c   \
	policy.c \
	utils.c

bin_PROGRAMS = pmount-hal
noinst_PROGRAMS = pmount pumount

pmount_SOURCES = pmount.c
pmount_LDADD = libpmount-util.a

pmount_hal_SOURCES = pmount-hal.c
pmount_hal_LDADD = libpmount-util.a $(HAL_LIBS)

pumount_SOURCES = pumount.c
pumount_LDADD = libpmount-util.a

EXTRA_DIST = \
	fs.h     \
	luks.h   \
	policy.h \
	utils.h

INSTALL_DIR = $(DESTDIR)/$(prefix)/bin
INSTALL_SRC = $(top_builddir)/src

install-data-hook:
	test -z $(INSTALL_DIR) || mkdir -p -- $(INSTALL_DIR)
	$(INSTALL_DATA) -o root -g root -m 4755 -D $(INSTALL_SRC)/pmount $(INSTALL_DIR)/pmount
	$(INSTALL_DATA) -o root -g root -m 4755 -D $(INSTALL_SRC)/pumount $(INSTALL_DIR)/pumount
	
uninstall-hook:
	rm -f $(INSTALL_DIR)/pmount
	rm -f $(INSTALL_DIR)/pumount
	
