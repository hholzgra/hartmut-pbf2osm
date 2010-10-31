dnl Process this file with autoconf to produce a configure script.
AC_INIT(pbf2osm, 0.11)

dnl Required autoconf version
AC_PREREQ(2.65)

dnl use automake to generate standard Makefiles
dnl use 'silent' make rules by default
AM_INIT_AUTOMAKE([dist-bzip2 silent-rules])
AM_SILENT_RULES([yes])

dnl Additional macro definitions are in here
AC_CONFIG_MACRO_DIR([m4])

dnl Generate configuration header file
AC_CONFIG_HEADER(config.h)

dnl Find C compiler
AC_PROG_CC_C99

dnl Find C++ compiler
AC_PROG_CXX

dnl Make sure we have libtool installed
AM_PROG_LIBTOOL

dnl check for zlib library
AX_LIB_ZLIB
if test "$HAVE_ZLIB" = "no" 
then
  AC_MSG_ERROR([required library not found]);
fi

dnl Check for bzip2 library
AX_LIB_BZIP2
if test "$HAVE_BZIP2" = "no" 
then
  AC_MSG_ERROR([required library not found]);
fi

dnl Check for protobuf-c library
AX_LIB_PROTOBUF_C
if test "$HAVE_PROTOBUF_C" = "no" 
then
  AC_MSG_ERROR([required library not found]);
fi
AC_CHECK_MEMBER([ProtobufCMessageDescriptor.packed],AC_MSG_RESULT([OK]),AC_MSG_ERROR([need at least protobuf-c 0.14]))

dnl check for right version of protoc-c
PROTOC_C_MIN_VERSION="2.3.0"
AC_PATH_PROG(PROTOC_C, protoc-c, false)
if test "$PROTOC_C" != "false"
then
  AC_MSG_CHECKING([protoc-c version >= $PROTOC_C_MIN_VERSION])
  version=`$PROTOC_C --version | sed -e's/^.* //'`
  AX_COMPARE_VERSION($version, [ge], $PROTOC_C_MIN_VERSION, 
  			       [AC_MSG_RESULT(OK)], 
			       [PROTOC_C=false; AC_MSG_WARN($version too old)] )
fi
if test -d .git && test "$PROTOC_C" = "false"
then
  AC_MSG_ERROR([building from git repository requires protoc_c verison $PROTOC_C_MIN_VERSION or newer])
fi


dnl Generate Makefile
AC_OUTPUT(Makefile src/Makefile)

