#!/bin/bash

echo
echo "Checking For MaNGOS Coding Standards:"
echo "Starting Codestyling Script:"
echo

declare -A singleLineRegexChecks=(
    # Exception patterns (ignore initializer-style brace lines)
    ["^[[:blank:]]*\\{\\s*\".*?\"\\s*,\\s*\\w+\\s*,\\s*(true|false)\\s*,\\s*&[a-zA-Z_][\\w:]*::[a-zA-Z_]\\w*\\s*,\\s*\".*?\"\\s*,\\s*(NULL|nullptr)\\s*\\},?[[:blank:]]*$"]="Well-formed initializer list line (valid) #ignore"

    # General whitespace/style rules
    ["[[:blank:]]$"]="Remove whitespace at the end of the lines"
    ["\t"]="Replace tabs with 4 spaces in the lines"

    # Control statements and braces
    ["^[[:blank:]]*(if|else if|else|for|while|switch)[[:blank:]]*\(.*\)[[:blank:]]*\{[[:blank:]]*\S"]="Opening brace must be on its own line after control statement"
    ["^[[:blank:]]*[\w:<>\*&~]+[[:blank:]]+\w+::?\w*\(.*\)[[:blank:]]*(const)?[[:blank:]]*\{[[:blank:]]*\S"]="Function opening brace must be on its own line (no code after '{')"
    ["^\s*\S.*\}[[:blank:]]*\S.*$"]="Closing brace must be on its own line (no code before or after '}' on the line)"

    # Brace usage enforcement
    ["^[[:blank:]]*if[[:blank:]]*\(.*\)[[:blank:]]*$"]="if statement must use braces"
    ["^[[:blank:]]*else[[:blank:]]*$"]="else statement must use braces"

    # Spacing rules
    ["\\b(if|for|while|switch|else\\s+if)\\("]="Missing space between keyword and '('"
    ["\\)(\\{)"]="Missing space before '{'"
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

    # If it's a fully ignorable rule, skip early
    if [[ "$ruleDesc" == *"#ignore"* ]]; then
        continue
    fi

    matches=$(grep -P -r -I -n "${grep_exclude_args[@]}" "${input_paths[@]}" -e "$check" 2>/dev/null)

    if [[ -n "$matches" ]]; then
        # Efficiently filter initializer list matches
        filteredMatches=$(printf "%s\n" "$matches" | grep -Pv '^[[:blank:]]*\{\s*".*?"\s*,\s*\w+\s*,\s*(true|false)\s*,\s*&[a-zA-Z_][\w:]*::[a-zA-Z_]\w*\s*,\s*".*?"\s*,\s*(NULL|nullptr)\s*\},?[[:blank:]]*$')

        if [[ -n "$filteredMatches" ]]; then
            echo
            echo "== Rule triggered: ${ruleDesc%%#*} =="
            echo "$filteredMatches"
            triggeredDescriptions+=("${ruleDesc%%#*}")
            hadError=1
        fi
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
