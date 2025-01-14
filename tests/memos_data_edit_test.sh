#!/usr/bin/env bash

TEST_PDB=$(mktemp /tmp/test.XXXXXX.pdb)
function cleanup()
{
    rm -f "$TEST_PDB"
}
trap cleanup EXIT

tail -n 3 "$0" > "$TEST_PDB"

EXPECTED_RESULT=("[INFO]: Header: Power Tips ")
EXPECTED_RESULT+=("[INFO]: Text: - In Calendar Day View, press Left/Right on the navigator to move backward and forward one day at a time.")
EXPECTED_RESULT+=("[INFO]: Category: Unfiled")
EXPECTED_RESULT+=("[INFO]: Header: Test 3")
EXPECTED_RESULT+=("[INFO]: Text: Sample text 3")
EXPECTED_RESULT+=("[INFO]: Category: Personal")
EXPECTED_RESULT+=("[INFO]: Header: Тест 4")
EXPECTED_RESULT+=("[INFO]: Text: Тестовая заметка")
EXPECTED_RESULT+=("[INFO]: Category: Unfiled")

XPECTED_RESULT+=("[INFO]: Category: Personal")

mapfile -t ACTUAL_RESULT < <(./memos_data_edit_test "$TEST_PDB" 2>&1)

for index in $(seq 0 5); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Faxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}."
        exit 1;
    fi
done
rm -f "$TEST_PDB"
exit 0
MemoDB                              �ο\���  p�   l   `    DATAmemo           z@    �BV�T    Unfiled         Business        Personal                                                                                                                                                                                                                                             Power Tips 
- In Calendar Day View, press Left/Right on the navigator to move backward and forward one day at a time. Test
Some text... 
