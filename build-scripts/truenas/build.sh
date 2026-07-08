#!/usr/bin/env bash

set -euo pipefail
set -x

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_ROOT="${TRUENAS_BUILD_ROOT:-$SCRIPT_DIR/build}"
version_path="${1:?missing TrueNAS version path}"
source_ref="${2:-master}"

mkdir -p "$BUILD_ROOT"

docker build --tag bookworm-build "$SCRIPT_DIR"
docker run \
    --rm \
    --mount type=bind,source="$BUILD_ROOT",target=/build \
    --mount type=bind,source="$SCRIPT_DIR/build-truenas-kmod.sh",target=/build.sh,readonly \
    --mount type=bind,source="$SCRIPT_DIR/extract-truenas-headers.sh",target=/extract-truenas-headers.sh,readonly \
    bookworm-build \
    bash -c 'cd /build && bash /build.sh "$@"' _ "$version_path" "$source_ref"
