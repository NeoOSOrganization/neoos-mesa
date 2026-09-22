#!/bin/bash
set -e

PREFIX="${PREFIX:-$(pwd)/build-output}"
BUILD_TMP="${BUILD_TMP:-$(pwd)/build-tmp}"

if [ ! -f upstream/meson.build ]; then
    echo "Error: upstream Mesa checkout not found (git submodule update --init)" >&2
    exit 1
fi

mkdir -p "$PREFIX"
rm -rf "$BUILD_TMP"

meson setup "$BUILD_TMP" upstream \
    --cross-file="$(pwd)/cross-file.txt" \
    --prefix="$PREFIX" \
    --default-library=static \
    -Dosmesa=true \
    -Dgallium-drivers=softpipe \
    -Dplatforms= \
    -Dglx=disabled \
    -Degl=disabled \
    -Dgbm=disabled \
    -Dvulkan-drivers= \
    -Dllvm=disabled \
    -Dshader-cache=disabled

echo "OK: Meson configure succeeded -- see build-tmp/meson-logs/meson-log.txt"
