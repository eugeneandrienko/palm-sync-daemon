#!/usr/bin/env bash

EXPECTED_RESULT=("[EMERGENCY]: Test emerg")
EXPECTED_RESULT+=("[ALERT]: Test alert")
EXPECTED_RESULT+=("[CRITICAL]: Test crit")
EXPECTED_RESULT+=("[ERROR]: Test err")
EXPECTED_RESULT+=("[WARNING]: Test warning")
EXPECTED_RESULT+=("[NOTICE]: Test notice")
EXPECTED_RESULT+=("[INFO]: Test info")
EXPECTED_RESULT+=("[DEBUG]: Test debug")
EXPECTED_RESULT+=("[UNKNOWN PRIORITY]: Test unknown priority")

mapfile -t ACTUAL_RESULT < <(./log_test 2>&1)

for index in $(seq 0 8); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1;
    fi
done
