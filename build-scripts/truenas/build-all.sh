set -uo pipefail

if ! truenas_versions=$(curl -fsSL https://download.truenas.com/ | grep -Po "(?<=href=\")TrueNAS-SCALE-\w*?/\d+(\.\d+)*" | uniq); then
    echo "Failed to fetch TrueNAS version list"
    exit 1
fi

failed_versions=()

for version in ${truenas_versions}; do
    major_version="${version#*/}"
    major_version="${major_version%%.*}"

    # only build for TrueNAS-SCALE-Dragonfish/24.04.0 and later versions
    if [ "$major_version" -ge 24 ] && [ ! -d "build/$version" ]; then
        echo Building "$version"
        if ! bash build.sh "$version"; then
            failed_versions+=("$version")
        fi
    fi
done

find -name '*.ko'

if ((${#failed_versions[@]})); then
    printf 'Failed to build TrueNAS versions:\n'
    printf '  %s\n' "${failed_versions[@]}"
    exit 1
fi
