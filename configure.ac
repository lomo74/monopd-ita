# Process this file with autoconf to produce a configure script.
AC_INIT(src/main.cpp)
AM_INIT_AUTOMAKE(monopd, 0.10.0)
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
AC_CHECK_HEADER([math++/nodes.h], [], [AC_MSG_ERROR([Please install libmath++ >= 0.0.3 from http://www.surakware.net/download/])])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

if test "x$GCC" = "xyes"; then  \
     CFLAGS="-Wall -ggdb";  \ 
     CXXFLAGS="-Wall -ggdb";  \ 
fi

AC_OUTPUT([
  Makefile
  conf/Makefile
  doc/Makefile
  doc/api/Makefile
  games/Makefile
  src/Makefile
])
