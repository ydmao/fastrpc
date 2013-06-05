dnl Process this file with autoconf to produce a configure script.

AC_INIT([fastrpc], [0.1])
AC_PREREQ(2.60)
AC_CONFIG_HEADERS([config.h])
AC_SUBST(ac_configure_args)
ac_user_cxx=${CXX+y}
ac_user_cxxflags=${CXXFLAGS+y}

AC_PROG_CC
AC_PROG_CXX
AC_LANG_CPLUSPLUS

if test -z "$ac_user_cxx$ac_user_cxxflags"; then
    AC_MSG_CHECKING([whether the C++ compiler requires -std=gnu++0x])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[struct s { int a; }; int f(s x) { auto &y = x; return y.a; }]], [[]])],
	[AC_MSG_RESULT([no])],
	[CXX="$CXX -std=gnu++0x"
	AC_MSG_RESULT([yes])])
fi

AC_C_BIGENDIAN([AC_DEFINE([ENDIAN_BIG], [1], [big endian])],
               [AC_DEFINE([ENDIAN_LITTLE], [1], [little endian])],
               [AC_MSG_ERROR([Unknown endian])],
               [AC_MSG_ERROR([Universal endian])])

AC_CHECK_HEADERS([ev++.h])
if test -z "$ac_cv_header_evpp_h"; then
    AC_MSG_ERROR([Cannot find <ev++.h>])
fi

AC_CACHE_CHECK([whether the C++ compiler understands constexpr], [ac_cv_cxx_constexpr], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[constexpr int f(int x) { return x + 1; }]], [[]])],
	[ac_cv_cxx_constexpr=yes], [ac_cv_cxx_constexpr=no])])
if test "$ac_cv_cxx_constexpr" = yes; then
    AC_DEFINE([HAVE_CXX_CONSTEXPR], [1], [Define if the C++ compiler understands constexpr.])
fi

AC_CACHE_CHECK([whether the C++ compiler understands 'auto'], [ac_cv_cxx_auto], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[struct s { int a; }; int f(s x) { auto &y = x; return y.a; }]], [[]])],
	[ac_cv_cxx_auto=yes], [ac_cv_cxx_auto=no])])
if test "$ac_cv_cxx_auto" = yes; then
    AC_DEFINE([HAVE_CXX_AUTO], [1], [Define if the C++ compiler understands 'auto'.])
else
    AC_MSG_ERROR([

The C++ compiler does not appear to understand C++11.
To fix this problem, try supplying a "CXX" argument to ./configure,
such as "./configure CXX='c++ -std=gnu++0x'"

========================================================])
fi

AC_OUTPUT
