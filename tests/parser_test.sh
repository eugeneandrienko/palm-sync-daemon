#!/usr/bin/env bash

TEST_ORG=$(mktemp /tmp/test.XXXXXX.org)
function cleanup()
{
    rm -f "$TEST_ORG"
}
trap cleanup EXIT
tail -n 38 "$0" > "$TEST_ORG"

EXPECTED_RESULT=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Just a header")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with tag")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: testtag")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with text below")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: First text line")
EXPECTED_RESULT+=("Second text line")
EXPECTED_RESULT+=("")
EXPECTED_RESULT+=("Last text line")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with tag and with text below")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: testtag2")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: First text line")
EXPECTED_RESULT+=("Last text line")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with priority")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: A")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with piority and text")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: C")
EXPECTED_RESULT+=("[INFO]: Text: Some text for C priority")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with priority, tag and text")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: testtag3")
EXPECTED_RESULT+=("[INFO]: Priority: B")
EXPECTED_RESULT+=("[INFO]: Text: Some text for B priority")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with keyword")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: TODO")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with keyword and priority")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: VERIFIED")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: A")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with keyword, tag and priority")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: CANCELLED")
EXPECTED_RESULT+=("[INFO]: Tag: testtag4")
EXPECTED_RESULT+=("[INFO]: Priority: B")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with keyword, tag, priority and text")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: TODO")
EXPECTED_RESULT+=("[INFO]: Tag: testtag")
EXPECTED_RESULT+=("[INFO]: Priority: A")
EXPECTED_RESULT+=("[INFO]: Text: Some text for header with:")
EXPECTED_RESULT+=("- keyword")
EXPECTED_RESULT+=("- tag")
EXPECTED_RESULT+=("- priority")
EXPECTED_RESULT+=("- text")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Кириллический заголовок")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: TODO")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with date")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: Time: Tue Jan 30 00:00:00 2024")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with date and time")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: Time: Tue Jan 30 23:59:00 2024")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with repetitive interval")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: Time: Tue Jan 30 23:59:00 2024")
EXPECTED_RESULT+=("[INFO]: Repeater interval: +1d")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with range")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: (null)")
EXPECTED_RESULT+=("[INFO]: Time range: Tue Jan 30 23:00:00 2024-Tue Jan 30 23:59:00 2024")
EXPECTED_RESULT+=("[INFO]: ---")
EXPECTED_RESULT+=("[INFO]: Header: Header with range and repetitive interval")
EXPECTED_RESULT+=("[INFO]: TODO-keyword: no keyword")
EXPECTED_RESULT+=("[INFO]: Tag: (null)")
EXPECTED_RESULT+=("[INFO]: Priority: no priority")
EXPECTED_RESULT+=("[INFO]: Text: And some text at the end")
EXPECTED_RESULT+=("And some text at the end 2.")
EXPECTED_RESULT+=("[INFO]: Time range: Tue Jan 30 23:00:00 2024-Tue Jan 30 23:59:00 2024")
EXPECTED_RESULT+=("[INFO]: Repeater interval: +1w")

mapfile -t ACTUAL_RESULT < <(./parser_test "$TEST_ORG" 2>&1)

for index in $(seq 0 117); do
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
* Header with tag                                                   :testtag:
* Header with text below
First text line
Second text line

Last text line
* Header with tag and with text below                              :testtag2:
First text line
Last text line
* [#A] Header with priority
* [#C] Header with piority and text
Some text for C priority
* [#B] Header with priority, tag and text                          :testtag3:
Some text for B priority
* TODO Header with keyword
* VERIFIED [#A] Header with keyword and priority
* CANCELLED [#B] Header with keyword, tag and priority             :testtag4:
* TODO [#A] Header with keyword, tag, priority and text             :testtag:
Some text for header with:
- keyword
- tag
- priority
- text
* TODO Кириллический заголовок
* Header with date
SCHEDULED: <2024-01-30 Tue>
* Header with date and time
DEADLINE: <2024-01-30 Tue 23:59>
* Header with repetitive interval
SCHEDULED: <2024-01-30 Tue 23:59 +1d>
* Header with range
SCHEDULED: <2024-01-30 Tue 23:00-23:59>
* Header with range and repetitive interval
SCHEDULED: <2024-01-30 Tue 23:00-23:59 +1w>
And some text at the end
And some text at the end 2.
