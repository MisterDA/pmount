AC_PREREQ(2.52)

AC_INIT(src/pmount.c)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE(pmount, 0.9.11)

AC_PROG_INTLTOOL([0.21])

AM_MAINTAINER_MODE
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL
AC_PROG_INSTALL

AC_CHECK_HEADER(sysfs/libsysfs.h,
	[AC_CHECK_LIB(sysfs, sysfs_open_attribute,
		[LIBS="$LIBS -lsysfs"],
		[AC_MSG_ERROR([Missing sysfs library (install libsysfs-dev or similar?)])])],
	[AC_MSG_ERROR([Missing /usr/include/sysfs/libsysfs.h (install libsysfs-dev or similar?)])])

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
													   
ALL_LINGUAS="ca cs de el en es et fr he hr hu id it ml nb nl no pa pl pt_BR ro ru sv tl yi zh_CN"
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
])

