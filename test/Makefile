# Dynamic, not static: Mesa's OSMesa target (upstream meson.build)
# hardcodes shared_library() for this target regardless of Meson's
# --default-library=static, so libOSMesa is a real .so. NeoOS's proven
# dynamic-linking support (PT_INTERP/ET_DYN/dlopen) handles this the
# normal way real systems consume libOSMesa/libGL.
CC := x86_64-neoos-linux-musl-gcc
OSMESA_DIR := ../build-output
CFLAGS := -O2 -Wall -I$(OSMESA_DIR)/include
LDFLAGS := -L$(OSMESA_DIR)/lib -Wl,-rpath-link,$(OSMESA_DIR)/lib -lOSMesa -lm -lpthread

.PHONY: all clean
all: build/triangle_test.nex

build/triangle_test.nex: triangle_test.c
	mkdir -p build
	$(CC) $(CFLAGS) triangle_test.c -o $@ $(LDFLAGS)

clean:
	rm -rf build

WM_DIR := ../../neoos-wm
EGL_CFLAGS := -O2 -Wall -I$(OSMESA_DIR)/include -I../src-new -I$(WM_DIR)
EGL_LDFLAGS := -L$(OSMESA_DIR)/lib -Wl,-rpath-link,$(OSMESA_DIR)/lib -lEGL -lGL -lm -lpthread

build/egl_triangle_client.nex: egl_triangle_client.c $(WM_DIR)/wmclient.c
	mkdir -p build
	$(CC) $(EGL_CFLAGS) egl_triangle_client.c $(WM_DIR)/wmclient.c -o $@ $(EGL_LDFLAGS)

all: build/egl_triangle_client.nex
