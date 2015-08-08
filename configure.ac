# Process this file with autoconf to produce a configure script.
AC_INIT(configure.ac)
AM_INIT_AUTOMAKE(monopd, 0.10.2)
AM_CONFIG_HEADER(config.h)
AC_LANG(C++)

# Checks for programs.
AC_ISC_POSIX
AC_SUBST(CFLAGS,$CFLAGS)
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_SUBST(CXXFLAGS,$CFLAGS)
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADER([cmath], [], [AC_MSG_ERROR([Your STL doesn't come with cmath, upgrade your libstdc++.])])
AC_CHECK_HEADER([muParser.h], [], [AC_MSG_ERROR([Please install libmuparser >= 2.0.0 from http://muparser.beltoforion.de/])])
AC_CHECK_HEADER([utf8.h], [], [AC_MSG_ERROR([Please install UTF8-CPP library from http://utfcpp.sourceforge.net/])])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# Check whether to enable systemd startup notification.
# This requires libsystemd-daemon.
AC_ARG_WITH([systemd-daemon], AS_HELP_STRING([--with-systemd-daemon],
            [Add support for systemd startup notification (default is autodetected)]),
            [USE_SYSTEMD_DAEMON=$withval], [USE_SYSTEMD_DAEMON=auto])
AS_IF([test "x$USE_SYSTEMD_DAEMON" != "xno"], [
    PKG_CHECK_MODULES([SYSTEMD_DAEMON], [libsystemd],
        [AC_DEFINE(USE_SYSTEMD_DAEMON, 1, [Use systemd startup notification])],
        [PKG_CHECK_MODULES([SYSTEMD_DAEMON], [libsystemd-daemon],
            [AC_DEFINE(USE_SYSTEMD_DAEMON, 1, [Use systemd startup notification])],
            [AS_IF([test "x$USE_SYSTEMD_DAEMON" = "xyes"],
                [AC_MSG_ERROR([systemd startup notification support requested, but libsystemd-daemon not found.])]
            )]
        )]
    )
])

if test "x$GCC" = "xyes"; then  \
     CFLAGS+=" -Wall -Wextra";  \
     CXXFLAGS+=" -Wall -Wextra";  \
fi

AC_OUTPUT([
  Makefile
  conf/Makefile
  doc/Makefile
  doc/api/Makefile
  doc/systemd/Makefile
  games/Makefile
  src/Makefile
])
