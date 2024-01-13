#!/usr/bin/env bash

if [ "$(pwd | xargs basename)" == "scripts" ]; then
    cd ..
fi

rm -vf *\~
aclocal
autoconf
autoheader
automake
