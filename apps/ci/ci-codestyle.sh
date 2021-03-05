#!/bin/bash

set -e

echo "Starting Codestyling Script:"
echo "Checking for whitespaces:"
echo

declare -A singleLineRegexChecks=(
    ["[[:blank:]]$"]="Remove whitespace at the end of the lines above"
    ["\t"]="Replace tabs with 4 spaces in the lines above"
)

for check in ${!singleLineRegexChecks[@]}; do
    echo "  Checking RegEx: '${check}'"
    
    if grep -P -r -I -n ${check} src; then
        echo
        echo "${singleLineRegexChecks[$check]}"
        exit 1
    fi
done

echo
echo "Awesome! No issues..."
