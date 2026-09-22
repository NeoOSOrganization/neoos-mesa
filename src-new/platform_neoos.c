/*
 * NeoOS EGL platform: bridges Mesa's generic swrast loader path
 * (putImage/getImage callbacks -- the same mechanism old
 * X11-without-DRI and software Wayland use) into neoos-wm's existing
 * memfd-backed client protocol (wmclient.h). Modeled directly on
 * platform_x11.c's swrast code path (swrastCreateDrawable/
 * swrastPutImage/swrastGetImage/dri2_x11_create_window_surface/
 * dri2_x11_swap_buffers/dri2_initialize_x11_swrast), with every
 * XCB/X11-protocol call replaced by a wmclient.h call.
 */

#include <stdlib.h>
#include <string.h>

#include "egl_dri2.h"
#include "loader.h"
#include "platform_neoos.h"
#include "wmclient.h"

/* ARGB8888 shifts/sizes (red@16, green@8, blue@0, alpha@24, 8 bits
 * each) -- the same values egl_dri2.c's own dri2_pbuffer_visuals[]
 * table uses for its "ARGB8888" entry. wm's WM_FORMAT_XRGB8888 has
 * the identical byte layout; alpha is simply unused for our opaque
 * window surfaces. */
static const int neoos_rgba_shifts[4] = { 16, 8, 0, 24 };
static const unsigned int neoos_rgba_sizes[4] = { 8, 8, 8, 8 };

static void
neoosCreateDrawable(struct dri2_egl_surface *dri2_surf)
{
   dri2_surf->neoos_bytes_per_pixel = 4;
}

static void
neoosGetDrawableInfo(__DRIdrawable *draw,
                     int *x, int *y, int *w, int *h,
                     void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   /* Fixed-size windows only (Non-goals) -- the size recorded at
    * eglCreateWindowSurface time is authoritative, no live query. */
   *x = 0;
   *y = 0;
   *w = dri2_surf->base.Width;
   *h = dri2_surf->base.Height;
}

static void
neoosPutImage(__DRIdrawable *draw, int op,
             int x, int y, int w, int h,
             char *data, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   struct wm_conn *wm = (struct wm_conn *) dri2_dpy->base.PlatformDisplay;
   uint32_t *dst = wm_pixels(wm);
   uint32_t stride = wm_stride_px(wm);
   int bpp = dri2_surf->neoos_bytes_per_pixel;
   int row;

   if (!dst || bpp <= 0)
      return;

   for (row = 0; row < h; row++) {
      memcpy(&dst[(y + row) * (int) stride + x],
             data + (size_t) row * w * bpp,
             (size_t) w * bpp);
   }

   wm_damage(wm, x, y, w, h);
   wm_commit(wm);
}

static void
neoosGetImage(__DRIdrawable *read,
             int x, int y, int w, int h,
             char *data, void *loaderPrivate)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);
   struct wm_conn *wm = (struct wm_conn *) dri2_dpy->base.PlatformDisplay;
   uint32_t *src = wm_pixels(wm);
   uint32_t stride = wm_stride_px(wm);
   int bpp = dri2_surf->neoos_bytes_per_pixel;
   int row;

   if (!src || bpp <= 0)
      return;

   for (row = 0; row < h; row++) {
      memcpy(data + (size_t) row * w * bpp,
             &src[(y + row) * (int) stride + x],
             (size_t) w * bpp);
   }
}

static const __DRIswrastLoaderExtension neoos_swrast_loader_extension = {
   .base            = { __DRI_SWRAST_LOADER, 3 },
   .getDrawableInfo = neoosGetDrawableInfo,
   .putImage        = neoosPutImage,
   .getImage        = neoosGetImage,
};

static const __DRIextension *neoos_swrast_loader_extensions[] = {
   &neoos_swrast_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   NULL,
};

static EGLBoolean
dri2_neoos_add_configs_for_visuals(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   unsigned int config_count = 0;
   unsigned i;

   for (i = 0; dri2_dpy->driver_configs[i] != NULL; i++) {
      struct dri2_egl_config *dri2_conf =
         dri2_add_config(disp, dri2_dpy->driver_configs[i],
                         config_count + 1, EGL_WINDOW_BIT, NULL,
                         neoos_rgba_shifts, neoos_rgba_sizes);
      if (dri2_conf && dri2_conf->base.ConfigID == config_count + 1)
         config_count++;
   }

   return config_count != 0;
}

static _EGLSurface *
dri2_neoos_create_window_surface(_EGLDisplay *disp, _EGLConfig *conf,
                                 void *native_window,
                                 const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;
   struct NeoosEGLWindow *win = native_window;

   if (!win) {
      _eglError(EGL_BAD_NATIVE_WINDOW, "eglCreateWindowSurface");
      return NULL;
   }

   dri2_surf = calloc(1, sizeof *dri2_surf);
   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "eglCreateWindowSurface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, EGL_WINDOW_BIT, conf,
                          attrib_list, false, native_window))
      goto cleanup_surface;

   config = dri2_get_dri_config(dri2_conf, EGL_WINDOW_BIT,
                                dri2_surf->base.GLColorspace);
   if (!config) {
      _eglError(EGL_BAD_MATCH,
               "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surface;
   }

   dri2_surf->neoos_surface_id = win->surface_id;
   dri2_surf->base.Width = win->width;
   dri2_surf->base.Height = win->height;
   neoosCreateDrawable(dri2_surf);

   if (!dri2_create_drawable(dri2_dpy, config, dri2_surf, dri2_surf))
      goto cleanup_surface;

   return &dri2_surf->base;

cleanup_surface:
   free(dri2_surf);
   return NULL;
}

static EGLBoolean
dri2_neoos_destroy_surface(_EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);
   dri2_fini_surface(surf);
   free(dri2_surf);
   return EGL_TRUE;
}

static EGLBoolean
dri2_neoos_swap_buffers(_EGLDisplay *disp, _EGLSurface *draw)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(draw);

   /* swrast has no flush extension -- this directly invokes
    * neoosPutImage(op=__DRI_SWRAST_IMAGE_OP_SWAP) under the hood,
    * exactly like platform_x11.c's swrast fallback path. */
   dri2_dpy->core->swapBuffers(dri2_surf->dri_drawable);
   return EGL_TRUE;
}

static const struct dri2_egl_display_vtbl dri2_neoos_display_vtbl = {
   .create_window_surface = dri2_neoos_create_window_surface,
   .destroy_surface       = dri2_neoos_destroy_surface,
   .create_image          = dri2_create_image_khr,
   .swap_buffers          = dri2_neoos_swap_buffers,
   .get_dri_drawable      = dri2_surface_get_dri_drawable,
};

EGLBoolean
dri2_initialize_neoos(_EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy;
   const char *err;

   dri2_dpy = calloc(1, sizeof *dri2_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
   disp->DriverData = (void *) dri2_dpy;

   disp->Device = _eglAddDevice(dri2_dpy->fd, true);
   if (!disp->Device) {
      err = "DRI2: failed to find EGLDevice";
      goto cleanup;
   }

   dri2_dpy->driver_name = strdup("swrast");
   if (!dri2_dpy->driver_name || !dri2_load_driver_swrast(disp)) {
      err = "DRI2: failed to load swrast driver";
      goto cleanup;
   }

   dri2_dpy->loader_extensions = neoos_swrast_loader_extensions;

   if (!dri2_create_screen(disp)) {
      err = "DRI2: failed to create screen";
      goto cleanup;
   }

   if (!dri2_setup_extensions(disp)) {
      err = "DRI2: failed to find required DRI extensions";
      goto cleanup;
   }

   dri2_setup_screen(disp);

   if (!dri2_neoos_add_configs_for_visuals(disp)) {
      err = "DRI2: failed to add configs";
      goto cleanup;
   }

   dri2_dpy->vtbl = &dri2_neoos_display_vtbl;

   return EGL_TRUE;

cleanup:
   dri2_display_destroy(disp);
   return _eglError(EGL_NOT_INITIALIZED, err);
}
