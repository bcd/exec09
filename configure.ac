AC_PREREQ(2.57)
AC_INIT(m6809-run, 0.90, brian@oddchange.com)
AC_CONFIG_SRCDIR([6809.c])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([memset socket strtoul])

# Template:
# AC_DEFINE will place in config.h; AC_SUBST updates the makefiles
#AC_DEFINE(VAR, value, [Doc string])
#AC_SUBST([VAR])

# Template for --enable options
#AC_ARG_ENABLE([name],
#   AC_HELP_STRING([--enable-name], [doc string for option]),
#   [enable_name=$enableval], [enable_name=default value])
#if test $enable_name = yes; then
#   AC_DEFINE(CONFIG_NAME, 1, [doc string when enabled])
#fi

# Template for --with options
#AC_ARG_WITH([name],
#   AC_HELP_STRING([--with-name=<parameter-name>], [doc string]),
#   [with_name=$withval], [with_name=default value])
#CONFIG_NAME="$with_name" etc.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
