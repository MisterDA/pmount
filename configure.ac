AC_PREREQ(2.52)

AC_INIT(src/pmount.c)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE(pmount, 0.9.23)

AC_PROG_INTLTOOL([0.21])

AM_MAINTAINER_MODE
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL
AC_PROG_INSTALL


AC_DEFINE([HAVE_BLKID], [], [Description])


AC_CHECK_HEADER(blkid/blkid.h,
	[AC_CHECK_LIB(blkid, blkid_get_cache,
		[AC_DEFINE(HAVE_BLKID) LIBS="$LIBS -lblkid"],
		[AC_MSG_WARN([Missing blkid library: you will not have fs autodetection (install libblkid-dev or similar?)])])],
	[AC_MSG_ERROR([Missing /usr/include/blkid/blkid.h: you will not have fs autodetection (install libblkid-dev or similar?)])])

# directory configuration
AC_ARG_WITH(media-dir, 
	AC_HELP_STRING([--with-media-dir=DIR], [Parent directory for mounts; MUST have trailing slash (default: /media/)]),
	AC_DEFINE_UNQUOTED(MEDIADIR, "$withval", [directory for mounts]),
	AC_DEFINE_UNQUOTED(MEDIADIR, "/media/", [directory for mounts]))
AC_ARG_WITH(lock-dir, 
	AC_HELP_STRING([--with-lock-dir=DIR], [Directory for mount locks (default: /var/lib/pmount-locks)]),
	AC_DEFINE_UNQUOTED(LOCKDIR, "$withval", [directory for locks]),
	AC_DEFINE_UNQUOTED(LOCKDIR, "/var/lib/pmount-locks", [directory for locks]))
AC_ARG_WITH(whitelist, 
	AC_HELP_STRING([--with-whitelist=PATH], [Path of whitelist configuration file (default: /etc/pmount.allow)]),
	AC_DEFINE_UNQUOTED(WHITELIST, "$withval", [whitelist configuration file]),
	AC_DEFINE_UNQUOTED(WHITELIST, "/etc/pmount.allow", [whitelist configuration file]))
AC_ARG_WITH(conffile, 
	AC_HELP_STRING([--with-conffile=PATH], [Path of system configuration file (default: /etc/pmount.conf)]),
	AC_DEFINE_UNQUOTED(SYSTEM_CONFFILE, "$withval", [system configuration file]),
	AC_DEFINE_UNQUOTED(SYSTEM_CONFFILE, "/etc/pmount.conf", [system configuration file]))


# Path to programs
AC_ARG_WITH(mount-prog, 
	AC_HELP_STRING([--with-mount-prog=PATH], [Path to mount program (default: /bin/mount)]),
	AC_DEFINE_UNQUOTED(MOUNTPROG, "$withval", [path to mount]),
	AC_DEFINE_UNQUOTED(MOUNTPROG, "/bin/mount", [path to mount]))
AC_ARG_WITH(umount-prog, 
	AC_HELP_STRING([--with-umount-prog=PATH], [Path to umount program (default: /bin/umount)]),
	AC_DEFINE_UNQUOTED(UMOUNTPROG, "$withval", [path to umount]),
	AC_DEFINE_UNQUOTED(UMOUNTPROG, "/bin/umount", [path to umount]))
AC_ARG_WITH(cryptsetup-prog, 
	AC_HELP_STRING([--with-cryptsetup-prog=PATH], [Path to cryptsetup program (default: /sbin/cryptsetup)]),
	AC_DEFINE_UNQUOTED(CRYPTSETUP, "$withval", [path to cryptsetup]),
	AC_DEFINE_UNQUOTED(CRYPTSETUP, "/sbin/cryptsetup", [path to cryptsetup]))
AC_ARG_WITH(mount-ntfs3g, 
	AC_HELP_STRING([--with-mount-ntfs3g=PATH], [Where to look for mount.ntfs-3g (default: /sbin/mount.ntfs-3g)]),
	AC_DEFINE_UNQUOTED(MOUNT_NTFS_3G, "$withval", [location of mount.ntfs-3g]),
	AC_DEFINE_UNQUOTED(MOUNT_NTFS_3G, "/sbin/mount.ntfs-3g", [location of mount.ntfs-3g]))
AC_ARG_WITH(fsck-prog, 
	AC_HELP_STRING([--with-fsck-prog=PATH], [Path to fsck program (default: /sbin/fsck)]),
	AC_DEFINE_UNQUOTED(FSCKPROG, "$withval", [path to fsck]),
	AC_DEFINE_UNQUOTED(FSCKPROG, "/sbin/fsck", [path to fsck]))

AC_ARG_WITH(losetup-prog, 
	AC_HELP_STRING([--with-losetup-prog=PATH], [Path to losetup program (default: /sbin/losetup)]),
	AC_DEFINE_UNQUOTED(LOSETUPPROG, "$withval", [path to losetup]),
	AC_DEFINE_UNQUOTED(LOSETUPPROG, "/sbin/losetup", [path to losetup]))


AC_ARG_ENABLE(hal,
  AC_HELP_STRING([--enable-hal],[build pmount-hal frontend, if hal is available]),
  [case $enableval in
     yes) ENABLE_HAL=yes ;;
     no)  ENABLE_HAL=no ;;
     *) AC_MSG_ERROR(bad value $enableval for --enable-hal) ;;
   esac],
   [ENABLE_HAL=yes]) dnl Default value

if test "$ENABLE_HAL" = "yes"; then
        PKG_CHECK_MODULES(HAL, hal >= 0.5.2 hal-storage >= 0.5.2,
                [ BUILD_HAL="hal >= 0.5.2, hal-storage >= 0.5.2"
                  AC_DEFINE(BUILD_HAL, 1, [defined if pmount-hal frontend should be built])],
                [ BUILD_HAL=""])
else
        BUILD_HAL=""
fi
AM_CONDITIONAL(PMOUNT_HAL, test -n "$BUILD_HAL")

dnl Checking for regular expressions.
dnl TODO: allow disabling the reading of the configuration file at compile
dnl time ? (and if regular expressions are not found)
AC_CHECK_HEADER(regex.h,
	[],
	[AC_MSG_ERROR([Cannot use POSIX regular expressions !])])	   

AC_ARG_ENABLE(ruid-root-cryptsetup,
  AC_HELP_STRING([--enable-ruid-root-cryptsetup],[Run cryptsetup with RUID = root]),
  [case $enableval in
     yes) CS_RUID=yes ;;
     no)  CS_RUID=no ;;
     *) AC_MSG_ERROR(bad value $enableval for --enable-ruid-root-cryptsetup) ;;
   esac],
   [CS_RUID=yes])
if test x"$CS_RUID" = xyes; then
   AC_MSG_NOTICE([Will be running cryptsetup with EUID = RUID = 0. Use --disable-ruid-root-cryptsetup ])
   AC_DEFINE(CRYPTSETUP_RUID, 1, [Define this parameter to run cryptsetup with RUID = EUID = root])
fi

GETTEXT_PACKAGE="pmount"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package])
AM_GLIB_GNU_GETTEXT

AC_OUTPUT([
Makefile
po/Makefile.in
man/Makefile
etc/Makefile
src/Makefile
tests/Makefile
])

