#! /bin/sh

# Copyright (c) 2004-2007  Daniel Elstner  <daniel.kitta@gmail.com>
#
# This file is part of Somato.
#
# Somato is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Somato is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Somato; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

if test "$#$NOCONFIGURE" = 0
then
  echo "I am going to run $srcdir/configure with no arguments -- if you"
  echo "wish to pass any to it, please specify them on the $0 command line."
fi

# Let the user override the default choice of tools.
if test -z "$ACLOCAL" || test -z "$AUTOMAKE"
then
  # Prefer explicitely versioned executables.
  for version in 1.10 1.9 1.8
  do
    if { "aclocal-$version" --version && "automake-$version" --version; } </dev/null >/dev/null 2>&1
    then
      ACLOCAL=aclocal-$version
      AUTOMAKE=automake-$version
      break
    fi
  done
fi

( # Enter a subshell to temporarily change the working directory.
  cd "$srcdir" || exit 1

  # Explicitely delete some old cruft, which seems to be
  # more reliable than --force options and the like.
  rm -f	acconfig.h config.cache config.guess config.rpath config.sub \
	depcomp install-sh missing mkinstalldirs
  rm -rf autom4te.cache

  #WARNINGS=all; export WARNINGS
  # Trace commands and exit if a command fails.
  set -ex

  ${ACLOCAL-aclocal} -I m4 $ACLOCAL_FLAGS
  ${AUTOCONF-autoconf}
  ${AUTOHEADER-autoheader}
  ${AUTOMAKE-automake} --add-missing --copy $AUTOMAKE_FLAGS
) || exit 1

if test -z "$NOCONFIGURE"
then
  echo "+ $srcdir/configure $*"
  "$srcdir/configure" "$@" || exit 1
  echo
  echo 'Now type "make" to compile Somato.'
fi
