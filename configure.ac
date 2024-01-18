AC_PREREQ([2.69])
AC_INIT([palm-sync-daemon],[0.1],[evg.andrienko@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror -Wportability])
AC_CONFIG_SRCDIR([src/palm-sync-daemon.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_EGREP

# Checks for libraries.
AC_CHECK_LIB([popt], [poptGetContext], [], AC_MSG_ERROR([cannot find libpopt]))
AC_CHECK_LIB([pisock], [pi_connect], [], AC_MSG_ERROR([cannot find libpisock]))

# Checks for header files.
AC_CHECK_INCLUDES_DEFAULT
AC_CHECK_HEADERS([errno.h syslog.h stdarg.h time.h fcntl.h signal.h])
AC_CHECK_HEADER([popt.h], [], AC_MSG_ERROR([cannot find popt.h]))
AC_CHECK_HEADER([libpisock/pi-header.h], [], AC_MSG_ERROR([cannot find libpisock/pi-header.h]))
AC_CHECK_HEADER([libpisock/pi-socket.h], [], AC_MSG_ERROR([cannot find libpisock/pi-socket.h]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

AC_OUTPUT
