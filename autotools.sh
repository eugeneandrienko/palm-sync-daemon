#!/usr/bin/env bash

rm -vf *\~
aclocal
autoconf
autoheader
automake
