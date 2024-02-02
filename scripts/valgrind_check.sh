#!/usr/bin/env bash

if [ "$(pwd | xargs basename)" == "scripts" ]; then
    cd ..
fi

valgrind -s --tool=memcheck ./palm-sync-daemon --foreground
