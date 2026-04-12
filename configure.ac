#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([blockchain], [0.5.0], [tim@metaverse.systems])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_RANLIB
AM_INIT_AUTOMAKE([subdir-objects]) 

# Checks for libraries.
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([CATCH2], catch2-with-main)

# Check for Boost and the serialization library
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)

AX_BOOST_BASE([1.50],, [AC_MSG_ERROR([Could not find Boost])])
AX_BOOST_SERIALIZATION(,[AC_MSG_ERROR([Could not find Boost.Serialization library])])
AX_BOOST_PROGRAM_OPTIONS(,[AC_MSG_ERROR([Could not find Boost.ProgramOptions library])])

# Platform-specific libraries
PLATFORM_LIBS=""
case "$host_os" in
  mingw*|msys*|cygwin*)
    PLATFORM_LIBS="-lws2_32 -lmswsock"
    ;;
esac
AC_SUBST([PLATFORM_LIBS])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 tests/Makefile])
AC_OUTPUT
