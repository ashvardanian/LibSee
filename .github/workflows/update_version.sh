#!/bin/sh

echo $1 > VERSION && 
    sed -i "s/^\(#define LIBSEE_VERSION_MAJOR \).*/\1$(echo "$1" | cut -d. -f1)/" libsee.c &&
    sed -i "s/^\(#define LIBSEE_VERSION_MINOR \).*/\1$(echo "$1" | cut -d. -f2)/" libsee.c &&
    sed -i "s/^\(#define LIBSEE_VERSION_PATCH \).*/\1$(echo "$1" | cut -d. -f3)/" libsee.c &&
    sed -i "s/VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/VERSION $1/" CMakeLists.txt
