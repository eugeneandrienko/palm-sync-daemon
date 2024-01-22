#!/usr/bin/env bash

if [ "$(pwd | xargs basename)" == "scripts" ]; then
    cd ..
fi

xdg-open docs/doxygen/html/index.html

