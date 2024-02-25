#!/usr/bin/env bash

EXPECTED_RESULT=("[INFO]: UTF8 string: \"Usual string\", len = 12")
EXPECTED_RESULT+=("[INFO]: CP1251 string: \"Usual string\", len = 12")
EXPECTED_RESULT+=("[INFO]: UTF8 string: \"РљРёСЂРёР»Р»РёС‡РµСЃРєР°СЏ СЃС‚СЂРѕРєР°\", len = 39")
EXPECTED_RESULT+=("[INFO]: CP1251 string: \"Кириллическая строка\", len = 20")
EXPECTED_RESULT+=("[INFO]: CP1251 string: \"Usual string\", len = 12")
EXPECTED_RESULT+=("[INFO]: UTF8 string: \"Usual string\", len = 12")
EXPECTED_RESULT+=("[INFO]: CP1251 string: \"РљРёСЂРёР»Р»РёС‡РµСЃРєР°СЏ СЃС‚СЂРѕРєР°\", len = 20")
EXPECTED_RESULT+=("[INFO]: UTF8 string: \"Кириллическая строка\", len = 39")

mapfile -t ACTUAL_RESULT < <(./helper_iconv_test 2>&1 | iconv -f CP1251 -t UTF8)

for index in $(seq 0 3); do
    echo "${ACTUAL_RESULT[$index]}" | \
        sed -r 's/.+(\[.+)$/\1/g' | \
        grep -Fxq "${EXPECTED_RESULT[$index]}"
    if [ "$?" -ne "0" ]; then
        echo "Failed test! Expected ${EXPECTED_RESULT[$index]}. But actual: ${ACTUAL_RESULT[$index]}"
        exit 1;
    fi
done
