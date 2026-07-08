#!/usr/bin/env bash

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${TRUENAS_BUILD_ROOT:-$SCRIPT_DIR/build}"
UGREEN_SOURCE_REFS="${UGREEN_SOURCE_REFS:-master}"

if ! truenas_versions=$(curl -fsSL https://download.truenas.com/ | grep -Po "(?<=href=\")TrueNAS-SCALE-\w*?/\d+(\.\d+)*" | uniq); then
    echo "Failed to fetch TrueNAS version list"
    exit 1
fi

failed_versions=()
headers_missing_file="$BUILD_ROOT/headers-missing.txt"

read -r -a source_refs <<< "$UGREEN_SOURCE_REFS"
if [ "${#source_refs[@]}" -eq 0 ]; then
    echo "No UGREEN source refs selected"
    exit 1
fi

mkdir -p "$BUILD_ROOT"

ref_output_root() {
    local source_ref="$1"

    if [ "$source_ref" = "master" ]; then
        printf '%s\n' "$BUILD_ROOT"
    else
        source_ref="${source_ref#refs/tags/}"
        printf '%s/tags/%s\n' "$BUILD_ROOT" "$source_ref"
    fi
}

for source_ref in "${source_refs[@]}"; do
    output_root="$(ref_output_root "$source_ref")"

    for version in ${truenas_versions}; do
        major_version="${version#*/}"
        major_version="${major_version%%.*}"

        if [ -f "$headers_missing_file" ] && grep -Fxq -- "$version" "$headers_missing_file"; then
            echo "Skipping $version because headers are listed in $headers_missing_file"
            continue
        fi

        # only build for TrueNAS-SCALE-Dragonfish/24.04.0 and later versions
        if [ "$major_version" -ge 24 ] && [ ! -f "$output_root/$version/led-ugreen.ko" ]; then
            echo "Building $source_ref for $version"
            if ! TRUENAS_BUILD_ROOT="$BUILD_ROOT" bash "$SCRIPT_DIR/build.sh" "$version" "$source_ref"; then
                failed_versions+=("$source_ref $version")
            fi
        fi
    done
done

find "$BUILD_ROOT" \( -name '*.ko' -o -path "$headers_missing_file" \) -print

if ((${#failed_versions[@]})); then
    printf 'Failed to build TrueNAS versions:\n'
    printf '  %s\n' "${failed_versions[@]}"
    exit 1
fi
