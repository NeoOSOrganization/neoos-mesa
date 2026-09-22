#!/bin/bash
set -e

PREFIX="${PREFIX:-$(pwd)/build-output}"
BUILD_TMP="${BUILD_TMP:-$(pwd)/build-tmp}"
RUNTIME="${RUNTIME:-$(pwd)/build-output-runtime-libs}"

# Where everything this build consumes lives. The defaults are the
# sibling checkouts of a normal development tree; neoos-os-builder
# passes its own scratch-directory paths.
NEOOS_TOOLCHAIN="${NEOOS_TOOLCHAIN:-$HOME/opt/cross-x86_64-neoos}"
ZLIB_DIR="${ZLIB_DIR:-$(pwd)/../neoos-zlib/build-output}"
EXPAT_DIR="${EXPAT_DIR:-$(pwd)/../neoos-expat/build-output}"
LIBDRM_DIR="${LIBDRM_DIR:-$(pwd)/../neoos-libdrm/build-output}"
WM_DIR="${WM_DIR:-$(pwd)/../neoos-wm}"
export PATH="$NEOOS_TOOLCHAIN/bin:$PATH"
for d in "$ZLIB_DIR" "$EXPAT_DIR" "$LIBDRM_DIR"; do
    [ -d "$d/lib/pkgconfig" ] || { echo "Error: $d/lib/pkgconfig missing -- build that dependency first" >&2; exit 1; }
done

if [ ! -f upstream/meson.build ]; then
    echo "Error: upstream Mesa checkout not found (git submodule update --init)" >&2
    exit 1
fi

echo "Resetting upstream/ to pristine mesa-22.3.5 before patching..."
git -C upstream checkout -- . 2>/dev/null || true
git -C upstream clean -fd src/egl/drivers/dri2/platform_neoos.c \
    src/egl/drivers/dri2/platform_neoos.h \
    src/egl/drivers/dri2/wmclient.c \
    src/egl/drivers/dri2/wmclient.h \
    src/egl/drivers/dri2/wmproto.h 2>/dev/null || true

for diff in patches/mesa-22.3.5/*.diff; do
    [ -s "$diff" ] || continue
    echo "Applying $diff"
    patch -p1 -d upstream < "$diff"
done

echo "Copying new files (platform_neoos, wmclient) into upstream/..."
cp src-new/platform_neoos.c src-new/platform_neoos.h \
   upstream/src/egl/drivers/dri2/
# wmclient.c's own #include "wmproto.h" needs this alongside it -- the
# egl target's include dirs don't reach ../neoos-wm on their own.
cp "$WM_DIR/wmclient.c" "$WM_DIR/wmclient.h" "$WM_DIR/wmproto.h" \
   upstream/src/egl/drivers/dri2/

mkdir -p "$PREFIX"
rm -rf "$BUILD_TMP"

CONSTANTS="$(pwd)/cross-constants.txt"
printf "[constants]\ntoolchain = '%s'\nzlib = '%s'\nexpat = '%s'\nlibdrm = '%s'\n" \
    "$NEOOS_TOOLCHAIN" "$ZLIB_DIR" "$EXPAT_DIR" "$LIBDRM_DIR" > "$CONSTANTS"

meson setup "$BUILD_TMP" upstream \
    --cross-file="$CONSTANTS" \
    --cross-file="$(pwd)/cross-file.txt" \
    --prefix="$PREFIX" \
    --default-library=static \
    -Dosmesa=true \
    -Dgallium-drivers=swrast \
    -Dplatforms= \
    -Dglx=disabled \
    -Degl=enabled \
    -Dgles1=false \
    -Dgles2=false \
    -Dgbm=disabled \
    -Dvulkan-drivers= \
    -Dllvm=disabled \
    -Dshader-cache=disabled

echo "OK: Meson configure succeeded -- see build-tmp/meson-logs/meson-log.txt"

ninja -C "$BUILD_TMP"
ninja -C "$BUILD_TMP" install

# libGL.so: upstream Mesa's meson build only ever produces this from
# src/glx/meson.build, which requires the full GLX/X11-protocol stack
# (glxclient.h, glxcmds.c, indirect_*.c) -- inapplicable with no X11
# and -Dglx=disabled. What upstream DOES build unconditionally is
# src/mapi/glapi's "bridge" dispatch-thunk archive (libglapi_bridge.a:
# real per-symbol glVertex2f/glBegin/etc. entry points forwarding into
# whatever table _glapi_set_dispatch() last installed -- zero GLX/X11
# code, pure arch-specific thunks). libEGL.so already links the same
# shared libglapi.so.0 and calls _glapi_set_dispatch() when
# eglMakeCurrent binds a context, so a libGL.so assembled from the
# bridge archive + a dynamic link against that same libglapi.so.0
# dispatches into the real bound context correctly. Verified: nm -D
# shows glBegin/glEnd/glVertex2f/etc. as defined T symbols, and the
# only NEEDED entries are libglapi.so.0 and libc.so.
GLAPI_BRIDGE="$BUILD_TMP/src/mapi/glapi/libglapi_bridge.a"
if [ ! -f "$GLAPI_BRIDGE" ]; then
    echo "ERROR: $GLAPI_BRIDGE not found -- upstream's mapi/glapi/meson.build layout may have changed" >&2
    exit 1
fi
x86_64-neoos-linux-musl-gcc -shared -fPIC \
    -Wl,-soname,libGL.so.1 \
    -Wl,--whole-archive "$GLAPI_BRIDGE" -Wl,--no-whole-archive \
    -L"$PREFIX/lib" -lglapi -lpthread \
    -o "$PREFIX/lib/libGL.so.1.0.0"
ln -sf libGL.so.1.0.0 "$PREFIX/lib/libGL.so.1"
ln -sf libGL.so.1 "$PREFIX/lib/libGL.so"

if [ -f "$PREFIX/lib/libOSMesa.so.8" ] || ls "$PREFIX"/lib/libOSMesa.so.8* >/dev/null 2>&1; then
    echo "OK: libOSMesa.so built at $PREFIX/lib"
else
    echo "ERROR: build finished but libOSMesa.so not found" >&2
    exit 1
fi
if [ -f "$PREFIX/lib/libEGL.so" ] && [ -f "$PREFIX/lib/libGL.so" ]; then
    echo "OK: libEGL.so and libGL.so built at $PREFIX/lib"
else
    echo "ERROR: build finished but libEGL.so/libGL.so not found" >&2
    exit 1
fi

# The runtime set a NeoOS disk image stages at /lib (NeoOS's
# disk-image rule copies from here): every .so a Mesa-linked program
# loads, stripped. The C/C++ runtime comes from the toolchain's own
# sysroot -- the libc.so the programs were linked against -- and musl's
# libc.so IS its dynamic linker, so ld-musl-x86_64.so.1 is a copy.
SYSROOT_LIB="$NEOOS_TOOLCHAIN/x86_64-neoos-linux-musl/lib"
STRIP=x86_64-neoos-linux-musl-strip
rm -rf "$RUNTIME"
mkdir -p "$RUNTIME/dri"
$STRIP -o "$RUNTIME/libOSMesa.so.8"     "$(readlink -f "$PREFIX/lib/libOSMesa.so.8")"
$STRIP -o "$RUNTIME/libglapi.so.0"      "$(readlink -f "$PREFIX/lib/libglapi.so.0")"
$STRIP -o "$RUNTIME/libEGL.so.1"        "$(readlink -f "$PREFIX/lib/libEGL.so.1")"
$STRIP -o "$RUNTIME/libGL.so.1"         "$(readlink -f "$PREFIX/lib/libGL.so.1")"
$STRIP -o "$RUNTIME/dri/swrast_dri.so"  "$PREFIX/lib/dri/swrast_dri.so"
$STRIP -o "$RUNTIME/libstdc++.so.6"     "$(readlink -f "$SYSROOT_LIB/libstdc++.so.6")"
$STRIP -o "$RUNTIME/libgcc_s.so.1"      "$SYSROOT_LIB/libgcc_s.so.1"
$STRIP -o "$RUNTIME/libc.so"            "$SYSROOT_LIB/libc.so"
cp "$RUNTIME/libc.so" "$RUNTIME/ld-musl-x86_64.so.1"
echo "OK: runtime libraries staged at $RUNTIME"
