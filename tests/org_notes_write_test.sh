#!/usr/bin/env bash

TEST_ORG=$(mktemp /tmp/test.XXXXXX)
function cleanup()
{
    rm -f "$TEST_ORG"
}
trap cleanup EXIT
tail -n 11 "$0" > "$TEST_ORG"

EXPECTED_RESULT="#+COMMENT
* Just a header
* TODO Header with category                                         :testtag:
* [#A] Header with text below
First text line
Second text line

Last text line
* VERIFIED [#B] Header with category and with text below           :testtag2:
First text line
Last text line
* Just a header TEST
* Just a header TEST2
* Header with tag TEST		:tag:
* Header with text TEST
Some test text
Second line
* Header with text and tag TEST		:tag2:
Some test text 2
Last line"

./org_notes_write_test "$TEST_ORG"
ACTUAL_RESULT=$(cat "$TEST_ORG")

if [ "$ACTUAL_RESULT" != "$EXPECTED_RESULT" ]; then
    echo "Actual result:"
    echo "$ACTUAL_RESULT"
    echo "---"
    echo "But expected result:"
    echo "$EXPECTED_RESULT"
    echo "---"
    exit 1
fi

exit 0
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
