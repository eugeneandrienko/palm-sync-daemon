#!/usr/bin/env bash

TEST_PDB=$(mktemp /tmp/test.XXXXXX.pdb)
function cleanup()
{
    rm -f "$TEST_PDB"
}
trap cleanup EXIT

tail -n 3 "$0" > "$TEST_PDB"

EXPECTED_RESULT=("[INFO]: Database name: MemoDB")
EXPECTED_RESULT+=("[INFO]: Attributes: 0")
EXPECTED_RESULT+=("[INFO]: Version: 0")
EXPECTED_RESULT+=("[INFO]: Creation datetime: 1705578204")
EXPECTED_RESULT+=("[INFO]: Modification datetime: 1705597699")
EXPECTED_RESULT+=("[INFO]: Last backup datetime: 0")
EXPECTED_RESULT+=("[INFO]: Modification number: 108")
EXPECTED_RESULT+=("[INFO]: Application info offset: 0x60")
EXPECTED_RESULT+=("[INFO]: Sort info offset: 0x00")
EXPECTED_RESULT+=("[INFO]: Database type ID: 0x44415441")
EXPECTED_RESULT+=("[INFO]: Creator ID: 0x6d656d6f")
EXPECTED_RESULT+=("[INFO]: Unique ID seed: 0")
EXPECTED_RESULT+=("[INFO]: Qty of records: 2")
EXPECTED_RESULT+=("[INFO]: Offset: 0x0000017a")
EXPECTED_RESULT+=("[INFO]: Attribute: 0x00")
EXPECTED_RESULT+=("[INFO]: Unique ID: 0x00 0x00 0x02")
EXPECTED_RESULT+=("[INFO]: Offset: 0x000001f0")
EXPECTED_RESULT+=("[INFO]: Attribute: 0x02")
EXPECTED_RESULT+=("[INFO]: Unique ID: 0x56 0xa0 0x54")
EXPECTED_RESULT+=("[INFO]: Renamed categories: 0")
EXPECTED_RESULT+=("[INFO]: Last unique ID: 0x0f")
EXPECTED_RESULT+=("[INFO]: Padding: 0")
EXPECTED_RESULT+=("[INFO]: Name: Unfiled")
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: Business")
EXPECTED_RESULT+=("[INFO]: ID: 1")
EXPECTED_RESULT+=("[INFO]: Name: Personal")
EXPECTED_RESULT+=("[INFO]: ID: 2")
EXPECTED_RESULT+=("[INFO]: Name: ") # 3
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 4
EXPECTED_RESULT+=("[INFO]: ID: 0")
EXPECTED_RESULT+=("[INFO]: Name: ") # 5
EXPECTED_RESULT+=("[INFO]: ID: 0")
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

mapfile -t ACTUAL_RESULT < <(./pdb_test "$TEST_PDB" 2>&1)

for index in $(seq 0 54); do
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
MemoDB                              �ο\���  p�   l   `    DATAmemo           z@    �BV�T    Unfiled         Business        Personal                                                                                                                                                                                                                                             Power Tips 
� In Calendar Day View, press Left/Right on the navigator to move backward and forward one day at a time. Test
Some text... 
