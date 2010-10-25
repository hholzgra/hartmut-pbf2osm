# SYNOPSIS
#
#   AX_LIB_PROTOBUF_C()
#
# DESCRIPTION
#
#   This macro provides tests of availability of the Google
#   Protocol Buffers C library. This macro checks for protobufr-c
#   headers and libraries and defines compilation flags
#
#   Macro supports following options and their values:
#
#   1) Single-option usage:
#
#     --with-protobuf_c      -- yes, no, or path to protobuf_c library 
#                          installation prefix
#
#   2) Three-options usage (all options are required):
#
#     --with-protobuf_c=yes
#     --with-protobuf_c-inc  -- path to base directory with protobuf_c headers
#     --with-protobuf_c-lib  -- linker flags for protobuf_c
#
#   This macro calls:
#
#     AC_SUBST(PROTOBUF_C_CFLAGS)
#     AC_SUBST(PROTOBUF_C_LDFLAGS)
#
#   And sets:
#
#     HAVE_PROTOBUF_C
#
# LICENSE
#
#   Copyright (c) 2009 Hartmut Holzgraefe <hartmut@php.net>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.

AC_DEFUN([AX_LIB_PROTOBUF_C],
[
    AC_ARG_WITH([protobuf-c],
        AC_HELP_STRING([--with-protobuf-c=@<:@ARG@:>@],
            [use protobuf-c library from given prefix (ARG=path); check standard prefixes (ARG=yes); disable (ARG=no)]
        ),
        [
        if test "$withval" = "yes"; then
            if test -f /usr/local/include/google/protobuf-c/protobuf-c.h ; then
                protobuf_c_prefix=/usr/local
            elif test -f /usr/include/google/protobuf-c/protobuf-c.h ; then
                protobuf_c_prefix=/usr
            else
                protobuf_c_prefix=""
            fi
            protobuf_c_requested="yes"
        elif test -d "$withval"; then
            protobuf_c_prefix="$withval"
            protobuf_c_requested="yes"
        else
            protobuf_c_prefix=""
            protobuf_c_requested="no"
        fi
        ],
        [
        dnl Default behavior is implicit yes
        if test -f /usr/local/include/google/protobuf-c/protobuf-c.h ; then
            protobuf_c_prefix=/usr/local
        elif test -f /usr/include/google/protobuf-c/protobuf-c.h ; then
            protobuf_c_prefix=/usr
        else
            protobuf_c_prefix=""
        fi
        ]
    )

    AC_ARG_WITH([protobuf-c-inc],
        AC_HELP_STRING([--with-protobuf-c-inc=@<:@DIR@:>@],
            [path to protobuf-c library headers]
        ),
        [protobuf_c_include_dir="$withval"],
        [protobuf_c_include_dir=""]
    )
    AC_ARG_WITH([protobuf-c-lib],
        AC_HELP_STRING([--with-protobuf-c-lib=@<:@ARG@:>@],
            [link options for protobuf-c library]
        ),
        [protobuf_c_lib_flags="$withval"],
        [protobuf_c_lib_flags=""]
    )

    PROTOBUF_C_CFLAGS=""
    PROTOBUF_C_LDFLAGS=""

    dnl
    dnl Collect include/lib paths and flags
    dnl
    run_protobuf_c_test="no"

    if test -n "$protobuf_c_prefix"; then
        protobuf_c_include_dir="$protobuf_c_prefix/include/google/protobuf-c/"
        protobuf_c_lib_flags="-L$protobuf_c_prefix/lib -lprotobuf-c"
        run_protobuf_c_test="yes"
    elif test "$protobuf_c_requested" = "yes"; then
        if test -n "$protobuf_c_include_dir" -a -n "$protobuf_c_lib_flags"; then
            run_protobuf_c_test="yes"
        fi
    else
        run_protobuf_c_test="no"
    fi

    dnl
    dnl Check protobuf_c files
    dnl
    if test "$run_protobuf_c_test" = "yes"; then

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS -I$protobuf_c_include_dir"

        saved_LDFLAGS="$LDFLAGS"
        LDFLAGS="$LDFLAGS $protobuf_c_lib_flags"

        dnl
        dnl Check protobuf_c headers
        dnl
        AC_MSG_CHECKING([for protobuf_c headers in $protobuf_c_include_dir])

        AC_LANG_PUSH([C++])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM(
                [[
@%:@include <google/protobuf-c/protobuf-c.h>
                ]],
                [[]]
            )],
            [
            PROTOBUF_C_CFLAGS="-I$protobuf_c_include_dir"
            protobuf_c_header_found="yes"
            AC_MSG_RESULT([found])
            ],
            [
            protobuf_c_header_found="no"
            AC_MSG_RESULT([not found])
            ]
        )
        AC_LANG_POP([C++])

        dnl
        dnl Check protobuf_c libraries
        dnl
        if test "$protobuf_c_header_found" = "yes"; then

            AC_MSG_CHECKING([for protobuf_c librariy])

            AC_LANG_PUSH([C++])
            AC_LINK_IFELSE([
                AC_LANG_PROGRAM(
                    [[
@%:@include <google/protobuf-c/protobuf-c.h>
                    ]],
                    [[
    protobuf_c_service_destroy((ProtobufCService *)NULL);
                    ]]
                )],
                [
                PROTOBUF_C_LDFLAGS="$protobuf_c_lib_flags"
                protobuf_c_lib_found="yes"
                AC_MSG_RESULT([found])
                ],
                [
                protobuf_c_lib_found="no"
                AC_MSG_RESULT([not found])
                ]
            )
            AC_LANG_POP([C++])
        fi

        CPPFLAGS="$saved_CPPFLAGS"
        LDFLAGS="$saved_LDFLAGS"
    fi

    AC_MSG_CHECKING([for protobuf-c library])

    if test "$run_protobuf_c_test" = "yes"; then
        if test "$protobuf_c_header_found" = "yes" -a "$protobuf_c_lib_found" = "yes"; then
            AC_SUBST([PROTOBUF_C_CFLAGS])
            AC_SUBST([PROTOBUF_C_LDFLAGS])
            AC_SUBST([HAVE_PROTOBUF_C])

            AC_DEFINE([HAVE_PROTOBUF_C], [1],
                [Define to 1 if protobuf_c library is available])

            HAVE_PROTOBUF_C="yes"
        else
            HAVE_PROTOBUF_C="no"
        fi

        AC_MSG_RESULT([$HAVE_PROTOBUF_C])


    else
        HAVE_PROTOBUF_C="no"
        AC_MSG_RESULT([$HAVE_PROTOBUF_C])

        if test "$protobuf_c_requested" = "yes"; then
            AC_MSG_WARN([protobuf-c support requested but headers or library not found. Specify valid prefix of protobuf-c using --with-protobuf-c=@<:@DIR@:>@ or provide include directory and linker flags using --with-protobuf-c-inc and --with-protobuf-c-lib])
        fi
    fi
])

