#!/usr/bin/env bash

if [ "$1" == "no" ]; then
    echo "No static analysis"
else
    if [ "$(pwd | xargs basename)" == "scripts" ]; then
        cd ..
    fi
    "$1" --error-exitcode=2 --enable=all -isrc/orgmode/parser -I include/ src/
fi
