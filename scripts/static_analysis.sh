#!/usr/bin/env bash

if [ "$1" == "no" ]; then
    echo "No static analysis"
else
    if [ "$(pwd | xargs basename)" == "scripts" ]; then
        cd ..
    fi
    "$1" --quiet --enable=all --std=c11 -isrc/orgmode/parser -I include/ src/ > static_analysis_report.txt 2>&1
fi
