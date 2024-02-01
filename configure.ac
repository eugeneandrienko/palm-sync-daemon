AC_PREREQ([2.69])
AC_INIT([palm-sync-daemon],[0.1],[evg.andrienko@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror -Wportability])
AC_CONFIG_SRCDIR([src/palm-sync-daemon.c])
AC_CONFIG_HEADERS([config.h])

# Checks for external programs
AC_CHECK_PROG([DOXYGEN], [doxygen], [yes], [no])
AM_CONDITIONAL([HAVE_DOXYGEN], [test "$DOXYGEN" == "yes"])
AM_COND_IF([HAVE_DOXYGEN], [AC_SUBST([MAYBE_DOXYGEN], [docs/doxygen])],
                           [AC_MSG_WARN([Continue without source code documentation])])

AC_CHECK_PROG([PDFLATEX], [pdflatex], [yes], [no])
AC_CHECK_PROG([BIBER], [biber], [yes], [no])
AC_CHECK_PROG([LATEXMK], [latexmk], [yes], [no])
AM_CONDITIONAL([HAVE_PDFLATEX], [test "$PDFLATEX" == "yes" && test "$BIBER" == "yes" && test "$LATEXMK" == "yes"])
AM_COND_IF([HAVE_PDFLATEX], [AC_SUBST([MAYBE_TEX], [docs/tex])],
                            [AC_MSG_WARN([Continue without PDF documentation])])

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_CONFIG_FILES([docs/doxygen/Makefile docs/doxygen/Doxyfile docs/tex/Makefile docs/man/Makefile])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_EGREP
AC_PROG_YACC
AC_PROG_LEX(noyywrap)
AM_PROG_LEX

# Checks for libraries.
AC_CHECK_LIB([popt], [poptGetContext], [], AC_MSG_ERROR([cannot find libpopt]))
AC_CHECK_LIB([pisock], [pi_connect], [], AC_MSG_ERROR([cannot find libpisock]))

# Checks for header files.
AC_CHECK_INCLUDES_DEFAULT
AC_CHECK_HEADERS([errno.h syslog.h stdarg.h stddef.h time.h fcntl.h signal.h endian.h], [], AC_MSG_ERROR([cannot find std headers]))
AC_CHECK_HEADER([sys/queue.h], [], AC_MSG_ERROR([cannot find glibc data structures]))
AC_CHECK_HEADER([popt.h], [], AC_MSG_ERROR([cannot find popt.h]))
AC_CHECK_HEADER([libpisock/pi-header.h], [], AC_MSG_ERROR([cannot find libpisock/pi-header.h]))
AC_CHECK_HEADER([libpisock/pi-socket.h], [], AC_MSG_ERROR([cannot find libpisock/pi-socket.h]))
AC_CHECK_HEADER([libpisock/pi-dlp.h], [], AC_MSG_ERROR([cannot find libpisock/pi-dlp.h]))
AC_CHECK_HEADER([libpisock/pi-file.h], [], AC_MSG_ERROR([cannot find libpisock/pi-file.h]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

AC_OUTPUT
