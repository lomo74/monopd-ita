# Process this file with autoconf to produce a configure script.
AC_INIT(src/main.cpp)
AM_INIT_AUTOMAKE(monopd, 0.9.3)
AM_CONFIG_HEADER(config.h)
AC_LANG(C++)

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADER([libcapsinetwork/listener.h], [], [AC_MSG_ERROR([Please install libCapsiNetwork >= 0.3.0 from http://unixcode.org/libcapsinetwork/])])
AC_CHECK_HEADER([libcapsinetwork/socket.h], [], [AC_MSG_ERROR([Please install libCapsiNetwork >= 0.3.0 from http://unixcode.org/libcapsinetwork/])])
AC_CHECK_HEADER([cmath], [], [AC_MSG_ERROR([Your STL doesn't come with cmath, upgrade your libstdc++.])])
AC_CHECK_HEADER([math++/nodes.h], [], [AC_MSG_ERROR([Please install libmath++ >= 0.0.3 from http://www.surakware.net/download/])])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_OUTPUT([
  Makefile
  conf/Makefile
  doc/Makefile
  doc/api/Makefile
  games/Makefile
  src/Makefile
])
