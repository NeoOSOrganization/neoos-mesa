#ifndef PLATFORM_NEOOS_H
#define PLATFORM_NEOOS_H

#include <EGL/egl.h>
#include <stdint.h>

/* Not a Khronos-registered value -- 0x3400 is verified free of any
 * collision in this Mesa tree's include/EGL/eglext.h. NeoOS-only. */
#define EGL_PLATFORM_NEOOS_MESA 0x3400

/* eglCreateWindowSurface's native-window argument, cast through
 * EGLNativeWindowType. wmclient.h has no "query an existing surface's
 * size" accessor (and adding one would touch neoos-wm, out of scope
 * for this sub-project) -- the caller already knows the size from its
 * own wm_create_window() call, so it bundles it here. Matches how
 * Wayland's EGLNativeWindowType (struct wl_egl_window*) also carries
 * cached size alongside the real surface, rather than X11's bare-XID
 * pattern which relies on being able to query the window manager.
 */
struct NeoosEGLWindow {
   int32_t surface_id;
   uint32_t width;
   uint32_t height;
};

EGLBoolean
dri2_initialize_neoos(_EGLDisplay *disp);

#endif
