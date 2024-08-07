#!/usr/bin/env bash

TEST_PDB=$(mktemp /tmp/test.XXXXXX.pdb)
function cleanup()
{
    rm -f "$TEST_PDB"
}
trap cleanup EXIT

tail -n 3 "$0" > "$TEST_PDB"

EXPECTED_RESULT+=("[INFO]: Application info offset: 0x68")
EXPECTED_RESULT+=("[INFO]: Qty of records: 3")
EXPECTED_RESULT+=("[INFO]: Offset: 0x0000017a")
EXPECTED_RESULT+=("[INFO]: Attribute: 0x40")
EXPECTED_RESULT+=("[INFO]: Unique ID: 0x00 0x00 0x02")
EXPECTED_RESULT+=("[INFO]: Offset: 0x000001f0")
EXPECTED_RESULT+=("[INFO]: Attribute: 0x42")
EXPECTED_RESULT+=("[INFO]: Unique ID: 0x56 0xa0 0x54")
EXPECTED_RESULT+=("[INFO]: Offset: 0x00000002")
EXPECTED_RESULT+=("[INFO]: Attribute: 0x83");

mapfile -t ACTUAL_RESULT < <(./pdb_record_test "$TEST_PDB" 2>&1)

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
MemoDB                              �ο\���  p�   l   `    DATAmemo           z@    �BV�T    Unfiled         Business        Personal                                                                                                                                                                                                                                             Power Tips 
� In Calendar Day View, press Left/Right on the navigator to move backward and forward one day at a time. Test
Some text... 
