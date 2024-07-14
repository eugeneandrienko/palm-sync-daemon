#!/usr/bin/env bash

TEST_ORG=$(mktemp /tmp/test.XXXXXX)
function cleanup()
{
    rm -f "$TEST_ORG"
}
trap cleanup EXIT
tail -n 11 "$0" > "$TEST_ORG"

EXPECTED_RESULT=("[INFO]: Header: Just a header")
EXPECTED_RESULT+=("[INFO]: Header: Header with category")
EXPECTED_RESULT+=("[INFO]: Category: testtag")
EXPECTED_RESULT+=("[INFO]: Header: Header with text below")
EXPECTED_RESULT+=("[INFO]: Text: First text line")
EXPECTED_RESULT+=("Second text line")
EXPECTED_RESULT+=("")
EXPECTED_RESULT+=("Last text line")
EXPECTED_RESULT+=("[INFO]: Header: Header with category and with text below")
EXPECTED_RESULT+=("[INFO]: Text: First text line")
EXPECTED_RESULT+=("Last text line")
EXPECTED_RESULT+=("[INFO]: Category: testtag2")

mapfile -t ACTUAL_RESULT < <(./org_notes_test "$TEST_ORG" 2>&1)

for index in $(seq 0 11); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq -- "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1;
    fi
done
rm -f "$TEST_ORG"
exit 0;
#+COMMENT
* Just a header
* TODO Header with category                                         :testtag:
* [#A] Header with text below
First text line
Second text line

Last text line
* VERIFIED [#B] Header with category and with text below           :testtag2:
First text line
Last text line
