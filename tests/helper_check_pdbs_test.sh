#!/usr/bin/env bash

rm -rf /tmp/previousDatebook.pdb \
   /tmp/previousMemos.pdb \
   /tmp/previousTodo.pdb

EXPECTED_RESULT=("[INFO]: NO FILES")

mapfile -t ACTUAL_RESULT < <(./helper_check_pdbs_test 2>&1)

for index in $(seq 0 1); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1;
    fi
done

touch /tmp/previousDatebook.pdb
touch /tmp/previousMemos.pdb
touch /tmp/previousTodo.pdb

EXPECTED_RESULT=("[INFO]: /tmp/previousDatebook.pdb")
EXPECTED_RESULT+=("[INFO]: /tmp/previousMemos.pdb")
EXPECTED_RESULT+=("[INFO]: /tmp/previousTodo.pdb")

mapfile -t ACTUAL_RESULT < <(./helper_check_pdbs_test 2>&1)

for index in $(seq 0 1); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1;
    fi
done

rm -rf /tmp/previousDatebook.pdb \
   /tmp/previousMemos.pdb \
   /tmp/previousTodo.pdb
