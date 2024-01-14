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
AC_CHECK_LIB(popt, poptGetContext)

# Checks for header files.
AC_CHECK_INCLUDES_DEFAULT
AC_CHECK_HEADERS([popt.h errno.h syslog.h stdarg.h time.h fcntl.h signal.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

# Checks for library functions.

AC_OUTPUT
