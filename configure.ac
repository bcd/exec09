AC_PREREQ(2.61)
AC_INIT(m6809-run, 0.92, brian@oddchange.com)
AC_CONFIG_SRCDIR([6809.c])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netinet/in.h stdint.h stdlib.h string.h sys/socket.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gettimeofday memset select strchr strstr strtoul socket])

AC_ARG_ENABLE([wpc],
   AC_HELP_STRING([--enable-wpc], [Enable WPC address map]),
   [wpc=$enableval], [wpc=no])
if test $wpc = yes; then
   AC_DEFINE(CONFIG_WPC, 1, [Use WPC address map])
fi

AC_ARG_ENABLE([6309],
   AC_HELP_STRING([--enable-6309], [Enable 6309 extensions]),
   [h6309=$enableval], [h6309=no])
if test $h6309 = yes; then
   AC_DEFINE(H6309, 1, [Use 6309 extensions])
fi

AC_ARG_ENABLE([readline],
   AC_HELP_STRING([--enable-readline], [Enable readline library]),
   [rl_lib=$enableval], [rl_lib=no])
if test $rl_lib = yes; then
   AC_DEFINE(HAVE_READLINE, 1, [Use readline library])
	READLINE_LIBS="-L/lib -lreadline"
	AC_SUBST(READLINE_LIBS)
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
