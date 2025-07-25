#!/bin/bash

echo
echo "Checking For MaNGOS Coding Standards:"
echo "Starting Codestyling Script:"
echo

declare -A singleLineRegexChecks=(
    ["[[:blank:]]$"]="Remove whitespace at the end of the lines above"
    ["\t"]="Replace tabs with 4 spaces in the lines above"
    ["^[[:blank:]]*(?:[a-zA-Z_][a-zA-Z_0-9]*[[:blank:]]+)*[a-zA-Z_][a-zA-Z_0-9]*[[:blank:]]*\([^)]*\)[[:blank:]]*\{[[:blank:]]*$"]="Move opening brace to a new line after function definition"
    ["\{[[:blank:]]*\S"]="Opening brace must be on its own line (no code after '{')"
    ["\S[[:blank:]]*\}"]="Closing brace must be on its own line (no code before '}')"
    ["^[[:blank:]]*if\s*\(.*\)[[:blank:]]*(?!\{)"]="if statement must use braces"
    ["^[[:blank:]]*else[[:blank:]]*(?!if|\{)"]="else statement must use braces"
    ["\bif\("]="Missing space between 'if' and '('"
)

# Ignore directories
grep_exclude_args=(
    --exclude-dir="Eluna"
    --exclude-dir="Extractor_Binaries"
    --exclude-dir="MangosStrings_LanguageHGenerator"
    --exclude-dir="restart-scripts"
    --exclude="CMakeLists.txt"
)

# Accept multiple input paths
input_paths=("$@")
if [[ ${#input_paths[@]} -eq 0 ]]; then
    input_paths=("src")  # fallback
fi

hadError=0
declare -a triggeredDescriptions

for check in "${!singleLineRegexChecks[@]}"; do
    ruleDesc="${singleLineRegexChecks[$check]}"
    matches=$(grep -P -r -I -n "${grep_exclude_args[@]}" "${input_paths[@]}" -e "$check" 2>/dev/null)

    if [[ -n "$matches" ]]; then
        echo
        echo "== Rule triggered: $ruleDesc =="
        echo "$matches"
        triggeredDescriptions+=("$ruleDesc")
        hadError=1
    fi
done

echo
echo "------------------------------------------"
echo "Summary of Triggered Rules:"
echo "------------------------------------------"

if [[ ${#triggeredDescriptions[@]} -eq 0 ]]; then
    echo "No style violations found."
else
    for rule in "${triggeredDescriptions[@]}"; do
        echo "$rule"
    done
fi

exit $hadError
