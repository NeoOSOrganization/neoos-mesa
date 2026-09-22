/* neoos-mesa proof of life: draw one triangle through the real OpenGL
 * API (OSMesa's off-screen context, softpipe underneath), and verify
 * specific pixels in the read-back buffer -- not just "didn't crash".
 *
 * Fixed-function glBegin/glVertex2f is used deliberately for this
 * first proof: even the compatibility profile is rasterized through
 * Mesa's real internal pipeline (translated to shaders internally by
 * st/mesa), so it already exercises softpipe/TGSI genuinely, while
 * avoiding GLSL toolchain issues as a variable in this first test.
 */
#include <GL/osmesa.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 64
#define H 64

int main(void) {
    OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
    if (!ctx) {
        printf("FAILED: OSMesaCreateContext returned NULL\n");
        return 1;
    }

    unsigned char *buffer = malloc((size_t)W * H * 4);
    if (!buffer) {
        printf("FAILED: malloc failed\n");
        return 1;
    }
    memset(buffer, 0, (size_t)W * H * 4);

    if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, W, H)) {
        printf("FAILED: OSMesaMakeCurrent failed\n");
        return 1;
    }

    glViewport(0, 0, W, H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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

    glFinish();

    /* Center of the buffer sits inside the triangle -- must be red. */
    int cx = W / 2, cy = H / 2;
    unsigned char *px = &buffer[(size_t)(cy * W + cx) * 4];
    printf("center pixel: r=%d g=%d b=%d a=%d\n", px[0], px[1], px[2], px[3]);
    if (!(px[0] > 200 && px[1] < 50 && px[2] < 50)) {
        printf("FAILED: center pixel is not red (got r=%d g=%d b=%d)\n",
               px[0], px[1], px[2]);
        return 1;
    }

    /* Top-left corner sits outside the triangle -- must stay the clear
     * color (black). */
    unsigned char *corner = &buffer[(size_t)(2 * W + 2) * 4];
    printf("corner pixel: r=%d g=%d b=%d a=%d\n",
           corner[0], corner[1], corner[2], corner[3]);
    if (corner[0] != 0 || corner[1] != 0 || corner[2] != 0) {
        printf("FAILED: corner pixel is not clear color (got r=%d g=%d b=%d)\n",
               corner[0], corner[1], corner[2]);
        return 1;
    }

    printf("PASS neoos-mesa-triangle: real OpenGL triangle rendered correctly\n");
    OSMesaDestroyContext(ctx);
    free(buffer);
    return 0;
}
