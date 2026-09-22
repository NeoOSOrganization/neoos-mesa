#!/bin/bash
set -e

PREFIX="${PREFIX:-$(pwd)/build-output}"
BUILD_TMP="${BUILD_TMP:-$(pwd)/build-tmp}"

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
cp ../neoos-wm/wmclient.c ../neoos-wm/wmclient.h ../neoos-wm/wmproto.h \
   upstream/src/egl/drivers/dri2/

mkdir -p "$PREFIX"
rm -rf "$BUILD_TMP"

meson setup "$BUILD_TMP" upstream \
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
