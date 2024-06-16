#!/usr/bin/env bash

EXPECTED_RESULT=("[INFO]: Hashes of str1 and str2 are not equal")
EXPECTED_RESULT+=("[INFO]: Hashes of str1 and str3 are equal")

mapfile -t ACTUAL_RESULT < <(./helper_hash_test 2>&1)

for index in $(seq 0 1); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1
    fi
done
