#!/bin/bash
# Re-diffs the patched files in upstream/ against a clean mesa-22.3.5
# checkout, refreshing this directory's *.diff files. Run this after
# editing any patched file directly in upstream/ (during development),
# before committing -- the diffs are the source of truth build.sh
# applies, not upstream/'s own working-tree state.
set -e
cd "$(dirname "$0")/../.."   # neoos-mesa/

CLEAN=$(mktemp -d)
git -C upstream worktree add --detach "$CLEAN/mesa-22.3.5" mesa-22.3.5 >/dev/null

FILES=(
  src/egl/meson.build
  src/egl/drivers/dri2/egl_dri2.h
  src/egl/drivers/dri2/egl_dri2.c
  src/egl/main/egldisplay.h
  src/egl/main/egldisplay.c
  src/egl/main/eglapi.c
)

for f in "${FILES[@]}"; do
  diff -u "$CLEAN/mesa-22.3.5/$f" "upstream/$f" \
    > "patches/mesa-22.3.5/$(echo "$f" | tr / _).diff" || true
  echo "regenerated patches/mesa-22.3.5/$(echo "$f" | tr / _).diff"
done

git -C upstream worktree remove --force "$CLEAN/mesa-22.3.5"
rm -rf "$CLEAN"
