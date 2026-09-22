/* neoos-mesa sub-project 2 proof of life: a standard EGL application
 * (eglGetPlatformDisplay/eglCreateWindowSurface/eglSwapBuffers -- not
 * OSMesa's private API) rendering a real GL triangle into an actual
 * neoos-wm window, composited on screen like any other client.
 */
#include <EGL/egl.h>
#include <GL/gl.h>
#include <stdio.h>
#include <unistd.h>

#include "wmclient.h"
#include "platform_neoos.h"

#define W 200
#define H 200

int main(void) {
    struct wm_conn *wm = wm_connect();
    if (!wm) {
        printf("FAILED: wm_connect returned NULL\n");
        return 1;
    }

    EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_NEOOS_MESA, wm, NULL);
    if (dpy == EGL_NO_DISPLAY) {
        printf("FAILED: eglGetPlatformDisplay returned EGL_NO_DISPLAY\n");
        return 1;
    }

    EGLint major, minor;
    if (!eglInitialize(dpy, &major, &minor)) {
        printf("FAILED: eglInitialize failed\n");
        return 1;
    }
    printf("EGL %d.%d initialized\n", major, minor);

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(dpy, config_attribs, &config, 1, &num_configs)
        || num_configs < 1) {
        printf("FAILED: eglChooseConfig found no matching config\n");
        return 1;
    }

    int32_t sid = wm_create_window(wm, W, H, "egl-triangle");
    if (sid < 0) {
        printf("FAILED: wm_create_window failed (%d)\n", sid);
        return 1;
    }
    struct NeoosEGLWindow win = { sid, W, H };

    EGLSurface surf = eglCreateWindowSurface(dpy, config,
                                             (EGLNativeWindowType) &win, NULL);
    if (surf == EGL_NO_SURFACE) {
        printf("FAILED: eglCreateWindowSurface failed\n");
        return 1;
    }

    eglBindAPI(EGL_OPENGL_API);
    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, NULL);
    if (ctx == EGL_NO_CONTEXT) {
        printf("FAILED: eglCreateContext failed\n");
        return 1;
    }

    if (!eglMakeCurrent(dpy, surf, surf, ctx)) {
        printf("FAILED: eglMakeCurrent failed\n");
        return 1;
    }

    glViewport(0, 0, W, H);
    glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(0.0f, 0.8f);
    glVertex2f(-0.8f, -0.8f);
    glVertex2f(0.8f, -0.8f);
    glEnd();

    eglSwapBuffers(dpy, surf);
    printf("PASS neoos-egl-triangle: swap complete, window committed\n");

    /* Hold the window open long enough for tools/screenshot.sh's
     * capture window (default 24s wait). */
    for (int i = 0; i < 40 && wm_alive(wm); i++)
        sleep(1);

    wm_disconnect(wm);
    return 0;
}
