#!/usr/bin/env bash

if [ "$(pwd | xargs basename)" == "scripts" ]; then
    cd ..
fi

DOCUMENTATION="docs/doxygen/html/index.html"
librewolf "$DOCUMENTATION"
if [ "$?" -ne "0" ]; then
    xdg-open "$DOCUMENTATION"
fi
