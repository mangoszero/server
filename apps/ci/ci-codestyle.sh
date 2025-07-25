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
    --exclude-dir="tools"
)

# Accept multiple input paths
input_paths=("$@")
if [[ ${#input_paths[@]} -eq 0 ]]; then
    input_paths=("src")  # fallback
fi

# Track errors and triggered rules
hadError=0
declare -A results
declare -a triggeredDescriptions

# Perform checks
for check in "${!singleLineRegexChecks[@]}"; do
    matches=$(grep -P -r -I -n "${grep_exclude_args[@]}" "${input_paths[@]}" -e "$check" 2>/dev/null)
    if [[ -n "$matches" ]]; then
        results["$check"]="$matches"
        triggeredDescriptions+=("${singleLineRegexChecks[$check]}")
        hadError=1
    fi
done

# Display results
if [[ $hadError -eq 1 ]]; then
    echo
    for check in "${!results[@]}"; do
        echo "== Rule: ${singleLineRegexChecks[$check]} =="
        echo "${results[$check]}"
        echo
    done

    echo "------------------------------------------"
    echo "Summary of Triggered Rules:"
    for rule in "${triggeredDescriptions[@]}"; do
        echo "- $rule"
    done
    echo "------------------------------------------"

    exit 1
else
    echo "All checks passed. No issues found."
    exit 0
fi
