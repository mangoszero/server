#!/bin/bash

#
#   Fix Statement Spacing for getMaNGOS
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

# Fixes missing space between control statement keywords and '('

# List of statement keywords to fix
keywords="if|else[[:blank:]]+if|for|while|switch"

for file in $(find . -type f \( -name "*.cpp" -o -name "*.h" \)); do
    temp_file="${file}.tmp"
    cp "$file" "$temp_file"

    changed=false

    IFS='|' read -ra KW_ARR <<< "$keywords"
    for kw in "${KW_ARR[@]}"; do
        # Fix keyword immediately followed by '(' with no space
        sed -E "s/\b($kw)\(/\1 (/g" "$temp_file" > "$temp_file.tmp" && mv "$temp_file.tmp" "$temp_file"
        if ! diff -q "$file" "$temp_file" >/dev/null; then
            changed=true
        fi
    done

    if $changed; then
        mv "$temp_file" "$file"
        echo "Fixed spacing in: $file"
    else
        rm "$temp_file"
    fi
done
