#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([blockchain], [0.0.1], [tim@metaverse.systems])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AM_INIT_AUTOMAKE() 

# Checks for libraries.
PKG_CHECK_MODULES([OPENSSL], [openssl])

# Check for Boost and the serialization library
AX_BOOST_BASE([1.50],, [AC_MSG_ERROR([Could not find Boost])])
AX_BOOST_SERIALIZATION(,[AC_MSG_ERROR([Could not find Boost.Serialization library])])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
