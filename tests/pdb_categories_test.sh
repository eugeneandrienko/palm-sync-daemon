#!/usr/bin/env bash

TEST_PDB=$(mktemp /tmp/test.XXXXXX.pdb)
function cleanup()
{
    rm -f "$TEST_PDB"
}
trap cleanup EXIT

tail -n 3 "$0" > "$TEST_PDB"

EXPECTED_RESULT+=("[INFO]: Renamed categories: 0")
EXPECTED_RESULT+=("[INFO]: Last unique ID: 0x0f")
EXPECTED_RESULT+=("[INFO]: Padding: 0")
EXPECTED_RESULT+=("[INFO]: Name: EDITED")
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: Business")
EXPECTED_RESULT+=("[INFO]: ID: 1")
EXPECTED_RESULT+=("[INFO]: Name: Personal")
EXPECTED_RESULT+=("[INFO]: ID: 2")
EXPECTED_RESULT+=("[INFO]: Name: ") # 3
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 4
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: NEW2") # 5
EXPECTED_RESULT+=("[INFO]: ID: 5")
EXPECTED_RESULT+=("[INFO]: Name: ") # 6
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 7
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 8
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 9
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 10
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 11
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 12
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 13
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 14
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 15
EXPECTED_RESULT+=("[INFO]: ID: 0")

mapfile -t ACTUAL_RESULT < <(./pdb_categories_test "$TEST_PDB" 2>&1)

for index in $(seq 0 9); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1;
    fi
done
rm -f "$TEST_PDB"
exit 0
MemoDB                              �ο\���  p�   l   `    DATAmemo           z@    �BV�T    Unfiled         Business        Personal        ABC             DEFEF           GHI             KLM                                                                                                                                                              0132456             Power Tips 
� In Calendar Day View, press Left/Right on the navigator to move backward and forward one day at a time. Test
Some text... 
