dnl# -*- mode:sh; sh-basic-offset:2; indent-tabs-mode:nil -*-
dnl#
dnl# autoconf macros
dnl#
dnl# (C) 2009 Benjamin Gerard <benjihan -4t- users.sourceforge -d0t- net>
dnl#
dnl# Distribued under the term of the GPL3+
dnl#
dnl# $Id: sc68_package.m4 96 2009-02-15 01:07:39Z benjihan $
dnl#

# serial 20090303

# SC68_CHECKS()
# -------------
# Does common checks for all sc68 related packages.
AC_DEFUN_ONCE([SC68_CHECKS],[
    AC_REQUIRE([SC68_TOOLS])
    AC_REQUIRE([SC68_OPTIONS])
   
    # Check for __dllspec
    # -------------------
    AS_IF([test "x$ac_cc_declspec" = "xyes"],
      [AC_DEFINE([HAVE_DECLSPEC],[1],[Support __declspec()])])

    # visibility attribut
    # -------------------
    if test [$]has_visibility = yes; then
      AC_DEFINE([HAVE_VISIBILITY],[1],[Support visibility __attribute__])
      AX_CHECK_COMPILER_FLAGS(
        [-fvisibility=hidden],
        [fvisibility=yes],
        [fvisibility=no])
      if test [$]fvisibility = yes; then
        SC68_ADD_FLAG(PKG_LIB_CFLAGS,[-fvisibility=hidden])
      fi
    fi
    
    # Check some more (mostly GCC) switch
    # -----------------------------------
    AC_FOREACH([FLAG],
      [-std=${CSTD-c99} -pedantic -Wall],
      [AX_CHECK_COMPILER_FLAGS(FLAG,SC68_ADD_FLAG(PKG_ALL_CFLAGS,FLAG))])

    # --enable-debug
    if test X[$]enable_debug = Xyes; then
      AC_FOREACH([FLAG],
        [-g -O0],
        [AX_CHECK_COMPILER_FLAGS(FLAG,[SC68_ADD_FLAG(PKG_ALL_CFLAGS,FLAG)])])
    fi

    # --enable-all-static
    if test X[$]enable_all_static = Xyes; then
      AC_ENABLE_STATIC
      AC_DISABLE_SHARED
      SC68_ADD_FLAG(PKG_ALL_LFLAGS,[-all-static])
      SC68_ADD_FLAG(PKG_ALL_CFLAGS,[-static])
    fi

    # libtool shared library needs -no-undefined
    AC_MSG_CHECKING([whether shared library need -no-undefined])
    if test X[$]ac_sys_platform_win32 = Xyes; then
      AC_MSG_RESULT([yes])
      SC68_ADD_FLAG(PKG_LIB_LFLAGS,[-no-undefined])
    else
      AC_MSG_RESULT([no])
    fi

    # PUP_FLAGS : common for all (both libraries and programms)
    AC_SUBST(PKG_ALL_CFLAGS)
    AC_SUBST(PKG_ALL_EFLAGS)
    AC_SUBST(PKG_ALL_LFLAGS)

    # LIBPUP_FLAGS : for building libraries
    AC_SUBST(PKG_LIB_CFLAGS)
    AC_SUBST(PKG_LIB_EFLAGS)
    AC_SUBST(PKG_LIB_LFLAGS)

    # BINPUP_FLAGS : for building programms
    AC_SUBST(PKG_BIN_CFLAGS)
    AC_SUBST(PKG_BIN_EFLAGS)
    AC_SUBST(PKG_BIN_LFLAGS)

    # PACKAGE_FLAGS : for others to compile/link against us
    AC_SUBST([PACKAGE_CFLAGS])
    AC_SUBST([PACKAGE_EFLAGS])
    AC_SUBST([PACKAGE_LFLAGS])
    
    # help tracking libtool dependencies
    AC_SUBST([LIBTOOL_DEPS])

  ])

dnl# ----------------------------------------------------------------------
dnl#
dnl# End Of $Id: sc68_package.m4 96 2009-02-15 01:07:39Z benjihan $
dnl#
dnl# ----------------------------------------------------------------------