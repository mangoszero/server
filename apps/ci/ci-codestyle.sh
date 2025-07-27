#!/bin/bash

#
#   Advanced Code Styling Check for getMaNGOS
#   Script written by Meltie2013 (https://github.com/Meltie2013)
#
#   Copyright: 2025
#   Website: https://getmangos.eu/
#
#   Script Type: Bash
#
#   Co-authors Area:
#
#   Note from the author: Anyone is free to use this script but is asked to keep the original author name and GitHub url in the header.
#   You can add co-authors to the header when the script is updated or changed for your project needs. Removing the original author
#   from the header, will result in a DMCA (take-down).
#
#   This script is viewed under the GPL-2.0 license and shall not be changed unless authorized by the original author.
#

LOG_IGNORED_RULES=false  # Set to true to print exception rule matches

echo
echo "Checking For MaNGOS Coding Standards:"
echo "Starting Codestyling Script:"

declare -A singleLineRegexChecks=(
    # General whitespace/style rules
    ["[[:blank:]]$"]="Remove whitespace at the end of the lines"
    ["\t"]="Replace tabs with 4 spaces in the lines"

    # Exception patterns (ignored, but still shown in summary)
    ["^[[:blank:]]*\\{\\s*\".*?\"\\s*,\\s*\\w+\\s*,\\s*(true|false)\\s*,\\s*&[a-zA-Z_][\\w:]*::[a-zA-Z_]\\w*\\s*,\\s*\".*?\"\\s*,\\s*(NULL|nullptr)\\s*\\},?[[:blank:]]*$"]="Well-formed initializer list line (styling is valid) #ignore"
    ["^[[:blank:]]*\\{[[:blank:]]*\"(SOAP-ENV|SOAP-ENC|xsi|xsd|ns1)\"[[:blank:]]*,[[:blank:]]*\"(http://[^\"]+|urn:[^\"]+)\"(?:[[:blank:]]*,[[:blank:]]*\"(http://[^\"]+|urn:[^\"]+)\")?[[:blank:]]*\\}[[:blank:]]*,?[[:blank:]]*(//.*)?$"]="Namespace initializer list line (styling is valid) #ignore"
    ["^[[:blank:]]*[^[:blank:]]+[[:blank:]]*<<(.*\"[^\"]*[{}][^\"]*\".*)+[[:blank:]]*;?[[:blank:]]*$"]="Streamed brace output (styling is valid) #ignore"

    # Control TODO/FIXME rules
    ["//[[:blank:]]*(TODO|FIXME)[[:blank:]]*(?!:)"]="TODO/FIXME must include description"

    # Virtual destructor enforcement
    ["\\b(?:virtual[[:space:]]+)?~[[:alnum:]_]+[[:space:]]*\\([[:space:]]*\\)[[:space:]]*\\{[[:space:]]*\\}[[:space:]]*;?"]="Prefer '= default' for destructors instead of empty braces"

    # Control statements and braces
    ["^[[:blank:]]*(if|else if|else|do|for|while|switch|try|catch|class|struct|namespace|case)[[:blank:]]*(\(.*\))?[[:blank:]]*\{[[:blank:]]*\S"]="Opening brace must be on its own line after statement"
    ["^[[:blank:]]*[\w:<>\*&~]+[[:blank:]]+\w+::?\w*\(.*\)[[:blank:]]*(const)?[[:blank:]]*\{[[:blank:]]*\S"]="Function opening brace must be on its own line (no code after '{')"
    ["^\s*\S.*\}[[:blank:]]*\S.*$"]="Closing brace must be on its own line (no code before or after '}' on the line)"

    # Brace usage enforcement
    ["^[[:blank:]]*enum[[:blank:]]+(\\w+[[:blank:]]*)?\\{[[:blank:]]*[^}]*\\}[[:blank:]]*;"]="Enum opening brace must be on its own line and enum body properly formatted"

    # Spacing rules
    ["\\b(if|for|while|switch|else\\s+if)\\("]="Missing space between keyword and '('"

    # Bad coding practice enforcement
    ["^[[:blank:]]*using[[:blank:]]+namespace[[:blank:]]+std[[:blank:]]*;"]="Avoid using namespace std (prefer explicit qualifiers)"
    ["^[[:blank:]]*namespace[[:blank:]]*\\{"]="Anonymous namespace brace must be on its own line"
)

# Directories and files to exclude
grep_exclude_args=(
    # Exclude Folders
    --exclude-dir="Eluna"
    --exclude-dir="Extractor_Binaries"
    --exclude-dir="MangosStrings_LanguageHGenerator"
    --exclude-dir="restart-scripts"

    # Exclude Files
    --exclude="CMakeLists.txt"
    --exclude="realmd.conf.dist.in"
    --exclude="mangosd.conf.dist.in"
    --exclude=".dockerignore"
    --exclude=".gitattributes"
    --exclude=".gitignore"
    --exclude=".gitmodules"
)

input_paths=("$@")
if [[ ${#input_paths[@]} -eq 0 ]]; then
    input_paths=("src")
fi

hadError=0
declare -a triggeredDescriptions
declare -a ignoredLines=()   # To store all lines matched by #ignore rules

# First pass: collect ignored lines (to exclude later from other rules)
for check in "${!singleLineRegexChecks[@]}"; do
    ruleDesc="${singleLineRegexChecks[$check]}"
    if [[ "$ruleDesc" == *"#ignore"* ]]; then
        # Keep original description with #ignore intact
        origRuleDesc="$ruleDesc"
        # Create print-friendly description without #ignore
        printRuleDesc="${ruleDesc%%#*}"

        matches=$(grep -P -r -I -n "${grep_exclude_args[@]}" "${input_paths[@]}" -e "$check" 2>/dev/null)
        if [[ -n "$matches" ]]; then
            ignoredLines+=("$matches")
            triggeredDescriptions+=("$origRuleDesc")  # Store original with #ignore for summary categorization

            if $LOG_IGNORED_RULES; then
                echo
                echo "== Exception Rule matched: $printRuleDesc =="
                echo "$matches"
            fi
        fi
    fi
done

# Flatten ignoredLines to a single pattern (line numbers + paths) for exclusion
ignoredPattern=$(printf "%s\n" "${ignoredLines[@]}" | cut -d: -f1,2 | sort -u | tr '\n' '|' | sed 's/|$//')

# Second pass: check non-ignored rules, excluding ignored lines from output
for check in "${!singleLineRegexChecks[@]}"; do
    ruleDesc="${singleLineRegexChecks[$check]}"
    if [[ "$ruleDesc" == *"#ignore"* ]]; then
        # Already handled in first pass, skip here
        continue
    fi

    matches=$(grep -P -r -I -n "${grep_exclude_args[@]}" "${input_paths[@]}" -e "$check" 2>/dev/null)

    # Filter out any ignored lines from matches
    if [[ -n "$matches" ]]; then
        # Remove lines already caught by ignored rules
        filteredMatches=$(printf "%s\n" "$matches" | grep -v -E "^($ignoredPattern):")

        if [[ -n "$filteredMatches" ]]; then
            # Skip lines with streamed or quoted braces
            filteredMatches=$(printf "%s\n" "$filteredMatches" | while IFS= read -r line; do
                echo "$line"
            done)

            if [[ -n "$filteredMatches" ]]; then
                echo
                echo "== Rule triggered: $ruleDesc =="
                echo "$filteredMatches"
                triggeredDescriptions+=("$ruleDesc")
                hadError=1
            fi
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
    declare -A seen=()
    exceptions=()
    violations=()

    for desc in "${triggeredDescriptions[@]}"; do
        # Remove leading/trailing whitespace and outer quotes if present
        clean_desc="$(echo "$desc" | sed -E 's/^[[:space:]]+|[[:space:]]+$//g' | sed -E 's/^"(.*)"$/\1/')"

        # Deduplicate
        [[ -n "${seen[$clean_desc]}" ]] && continue
        seen["$clean_desc"]=1

        # Check if the line ends with #ignore (with or without trailing space)
        if [[ "$desc" =~ [[:space:]]*#ignore$ ]]; then
            # Remove #ignore for printing
            exceptions+=("$(echo "$clean_desc" | sed -E 's/[[:space:]]*#ignore$//')")
        else
            violations+=("$clean_desc")
        fi
    done

    echo "Exception Rules (Informational Only):"
    echo "----"
    if [[ ${#exceptions[@]} -eq 0 ]]; then
        echo "(none)"
    else
        for e in "${exceptions[@]}"; do
            echo "$e"
        done
    fi

    echo "------------------------------------------"
    echo "Violations:"
    echo "----"
    if [[ ${#violations[@]} -eq 0 ]]; then
        echo "(none)"
    else
        for v in "${violations[@]}"; do
            echo "$v"
        done
    fi
fi

exit $hadError
