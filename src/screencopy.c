#define _POSIX_C_SOURCE 200112L

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#include <wayland-client-protocol.h>
#include "wlr-screencopy-unstable-v1-client-protocol.h"

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#define SCALE_FLAGS SWS_BICUBIC
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P

static bool opt_with_cursor = false;
static int opt_output_num = 0;

static struct SwsContext *sws_ctx = NULL;

static struct wl_shm *shm = NULL;
static struct zwlr_screencopy_manager_v1 *screencopy_manager = NULL;
const char *output_name = NULL;
static struct wl_output *output = NULL;
static struct zwlr_screencopy_frame_v1 *wl_frame = NULL;
static struct wl_display * wldisplay = NULL;

static struct {
    struct wl_buffer *wl_buffer;
    void *data;
    enum wl_shm_format format;
    int width, height, stride;
    bool y_invert;
    uint64_t start_pts;
    uint64_t pts;
} buffer;
bool buffer_copy_done = false;

static struct wl_buffer *create_shm_buffer(int32_t fmt,
        int width, int height, int stride, void **data_out) {
    int size = stride * height;

    const char shm_name[] = "/scrcpy-capture-wlroots-screencopy";
    int fd = shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if( fd < 0 ) {
        fprintf(stderr, "shm_open failed %d\n", fd);
        return NULL;
    }
    shm_unlink(shm_name);

    int ret;
    while( (ret = ftruncate(fd, size)) == EINTR ) {
        // No-op
    }
    if( ret < 0 ) {
        close(fd);
        fprintf(stderr, "ftruncate failed\n");
        return NULL;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if( data == MAP_FAILED ) {
        fprintf(stderr, "mmap failed: %m\n");
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    close(fd);
    struct wl_buffer *buf = wl_shm_pool_create_buffer(pool, 0, width, height, stride, fmt);
    wl_shm_pool_destroy(pool);

    *data_out = data;
    return buf;
}

static void frame_handle_buffer(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format,
        uint32_t width, uint32_t height, uint32_t stride) {
    buffer.format = format;
    buffer.width = width;
    buffer.height = height;
    buffer.stride = stride;

    if( !buffer.wl_buffer ) {
        buffer.wl_buffer =
            create_shm_buffer(format, width, height, stride, &buffer.data);
    }

    if( buffer.wl_buffer == NULL ) {
        fprintf(stderr, "failed to create buffer\n");
        exit(EXIT_FAILURE);
    }

    zwlr_screencopy_frame_v1_copy(frame, buffer.wl_buffer);
}

static void frame_handle_flags(void *data,
        struct zwlr_screencopy_frame_v1 *frame, uint32_t flags) {
    buffer.y_invert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
}

static void frame_handle_ready(void *data,
        struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi,
        uint32_t tv_sec_lo, uint32_t tv_nsec) {
    buffer.pts = ((((uint64_t)tv_sec_hi) << 32) | tv_sec_lo) * 1000000000 + tv_nsec;
    buffer_copy_done = true;
}

static void frame_handle_failed(void *data,
        struct zwlr_screencopy_frame_v1 *frame) {
    fprintf(stderr, "failed to copy frame\n");
    exit(EXIT_FAILURE);
}

static const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .buffer = frame_handle_buffer,
    .flags = frame_handle_flags,
    .ready = frame_handle_ready,
    .failed = frame_handle_failed,
};

static void handle_global(void *data, struct wl_registry *registry,
        uint32_t name, const char *interface, uint32_t version) {
    if( strcmp(interface, wl_output_interface.name) == 0 && opt_output_num > 0 ) {
        fprintf(stderr, "Using output: %s\n", interface);
        output = wl_registry_bind(registry, name, &wl_output_interface, 1);
        opt_output_num--;
    } else if( strcmp(interface, wl_shm_interface.name) == 0 ) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if( strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0 ) {
        screencopy_manager = wl_registry_bind(registry, name,
            &zwlr_screencopy_manager_v1_interface, 1);
    }
}

static void handle_global_remove(void *data, struct wl_registry *registry,
        uint32_t name) {
    wl_buffer_destroy(buffer.wl_buffer);
}

static const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static enum AVPixelFormat scrcpy_fmt_to_pixfmt(uint32_t fmt) {
    switch (fmt) {
    case WL_SHM_FORMAT_NV12: return AV_PIX_FMT_NV12;
    case WL_SHM_FORMAT_ARGB8888: return AV_PIX_FMT_BGRA;
    case WL_SHM_FORMAT_XRGB8888: return AV_PIX_FMT_BGR0;
    case WL_SHM_FORMAT_ABGR8888: return AV_PIX_FMT_RGBA;
    case WL_SHM_FORMAT_XBGR8888: return AV_PIX_FMT_RGB0;
    case WL_SHM_FORMAT_RGBA8888: return AV_PIX_FMT_ABGR;
    case WL_SHM_FORMAT_RGBX8888: return AV_PIX_FMT_0BGR;
    case WL_SHM_FORMAT_BGRA8888: return AV_PIX_FMT_ARGB;
    case WL_SHM_FORMAT_BGRX8888: return AV_PIX_FMT_0RGB;
    default: return AV_PIX_FMT_NONE;
    };
}

static void update_buffer() {
    buffer_copy_done = false;

    if( wl_frame )
        zwlr_screencopy_frame_v1_destroy(wl_frame);
    wl_frame = zwlr_screencopy_manager_v1_capture_output(screencopy_manager, opt_with_cursor, output);
    zwlr_screencopy_frame_v1_add_listener(wl_frame, &frame_listener, NULL);

    while( !buffer_copy_done && wl_display_dispatch(wldisplay) != -1 ) {
        // This space is intentionally left blank
    }
}

void screencopy_init(int *width, int *height, bool with_cursor, int output_num) {
    opt_with_cursor = with_cursor;
    opt_output_num = output_num;
    wldisplay = wl_display_connect(NULL);
    if( wldisplay == NULL ) {
        fprintf(stderr, "failed to create display: %m\n");
        exit(EXIT_FAILURE);
    }

    struct wl_registry *registry = wl_display_get_registry(wldisplay);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_dispatch(wldisplay);
    wl_display_roundtrip(wldisplay);

    if( shm == NULL ) {
        fprintf(stderr, "compositor is missing wl_shm\n");
        exit(EXIT_FAILURE);
    }
    if( screencopy_manager == NULL ) {
        fprintf(stderr, "compositor doesn't support wlr-screencopy-unstable-v1\n");
        exit(EXIT_FAILURE);
    }
    if( output == NULL ) {
        fprintf(stderr, "no output available\n");
        exit(EXIT_FAILURE);
    }

    update_buffer();
    
    *width = buffer.width;
    *height = buffer.height;
}

void screencopy_frame(AVFrame *frame, int frame_index, int width, int height) {
    update_buffer();

    // Convert from existing format to the target one
    sws_ctx = sws_getCachedContext(sws_ctx,
        frame->width, frame->height, scrcpy_fmt_to_pixfmt(buffer.format),
        frame->width, frame->height, STREAM_PIX_FMT, 0, NULL, NULL, NULL);
    //int *inv_table, srcrange, *table, dstrange, brightness, contrast, saturation;
    //sws_getColorspaceDetails(sws_ctx, &inv_table, &srcrange, &table, &dstrange, &brightness, &contrast, &saturation);
    //sws_setColorspaceDetails(sws_ctx, inv_table, srcrange, table, 1, brightness, contrast, saturation);
    uint8_t * inData[1] = { buffer.data };
    int inLinesize[1] = { buffer.stride };

    // Invert Y axis if source buffer is inverted
    if( buffer.y_invert ) {
        inData[0] += inLinesize[0] * (frame->height-1);
        inLinesize[0] = -inLinesize[0];
    }

    sws_scale(sws_ctx, (const uint8_t * const *)&inData, inLinesize, 0, frame->height, frame->data, frame->linesize);
}
