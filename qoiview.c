//------------------------------------------------------------------------------
//  qoiview.c
//
//  qoiview file=[image.qoi]
//
//  Or drag'n'drop a .qoi file into the view window.
//------------------------------------------------------------------------------
#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "qoi.h"
#define SOKOL_IMPL
#if defined(__APPLE__)
#define SOKOL_METAL
#elif defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES2
#else
#define SOKOL_GLCORE33
#endif
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_args.h"
#include "sokol/sokol_gl.h"
#include "sokol/sokol_fetch.h"
#include "sokol/sokol_debugtext.h"
#include "sokol/sokol_glue.h"
#include <stdint.h>
#include <stdlib.h>

#define MAX_FILE_SIZE (10 * 1024 * 1024)
#define MAX_SCALE (8.0f)
#define MIN_SCALE (0.25f)

static struct {
    sg_pass_action pass_action;
    sg_image img;
    float width;
    float height;
    float scale;
    struct {
        float x;
        float y;
    } offset;
    struct {
        sfetch_error_t error;
        bool qoi_decode_failed;
        uint8_t buf[MAX_FILE_SIZE];
    } file;
} state;

static void reset_offset_scale(void) {
    state.scale = 1.0f;
    state.offset.x = 0.0f;
    state.offset.y = 0.0f;
}

static void scale(float d) {
    state.scale += d;
    if (state.scale > MAX_SCALE) {
        state.scale = MAX_SCALE;
    }
    else if (state.scale < MIN_SCALE) {
        state.scale = MIN_SCALE;
    }
}

static void move(float dx, float dy) {
    state.offset.x += dx;
    state.offset.y += dy;
}

static void create_image(void* ptr, size_t size) {
    state.file.qoi_decode_failed = false;
    if (state.img.id != SG_INVALID_ID) {
        sg_destroy_image(state.img);
        state.img.id = SG_INVALID_ID;
    }
    int width, height;
    void* pixels = qoi_decode(ptr, (int)size, &width, &height, 4);
    if (!pixels) {
        state.file.qoi_decode_failed = true;
        return;
    }
    state.width = (float) width;
    state.height = (float) height;        
    reset_offset_scale();
    state.img = sg_make_image(&(sg_image_desc){
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = width,
        .height = height,
        .mag_filter = SG_FILTER_NEAREST,
        .min_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr = pixels,
            .size = width * height * 4
        }
    });
    free(pixels);
}

static void load_callback(const sfetch_response_t* response) {
    if (response->fetched) {
        state.file.error = SFETCH_ERROR_NO_ERROR;
        create_image(response->buffer_ptr, response->fetched_size);
    }
    else if (response->failed) {
        state.file.error = response->error_code;
    }
}

#if defined(__EMSCRIPTEN__)
static void emsc_dropped_file_callback(const sapp_html5_fetch_response* response) {
    if (response->succeeded) {
        state.file.error = SFETCH_ERROR_NO_ERROR;
        create_image(response->buffer_ptr, response->fetched_size);
    }
    else {
        switch (response->error_code) {
            case SAPP_HTML5_FETCH_ERROR_BUFFER_TOO_SMALL:
                state.file.error = SFETCH_ERROR_BUFFER_TOO_SMALL;
                break;
            case SAPP_HTML5_FETCH_ERROR_OTHER:
                state.file.error = SFETCH_ERROR_FILE_NOT_FOUND;
                break;
            default: break;
        }
    }
}
#endif

static void start_load_file(const char* path) {
    sfetch_send(&(sfetch_request_t){
        .path = path,
        .callback = load_callback,
        .buffer_ptr = state.file.buf,
        .buffer_size = sizeof(state.file.buf)
    });
}

static void start_load_dropped_file(void) {
    #if defined(__EMSCRIPTEN__)
        sapp_html5_fetch_dropped_file(&(sapp_html5_fetch_request){
            .dropped_file_index = 0,
            .callback = emsc_dropped_file_callback,
            .buffer_ptr = state.file.buf,
            .buffer_size = sizeof(state.file.buf)
        });
    #else
        const char* path = sapp_get_dropped_file_path(0);
        start_load_file(path);
    #endif
}

static const char* error_as_string(void) {
    if (state.file.qoi_decode_failed) {
        return "Not a valid .qoi file (decoding failed)";
    }
    else switch (state.file.error) {
        case SFETCH_ERROR_FILE_NOT_FOUND: return "File not found";
        case SFETCH_ERROR_BUFFER_TOO_SMALL: return "Image file too big";
        case SFETCH_ERROR_UNEXPECTED_EOF: return "Unexpected EOF";
        case SFETCH_ERROR_INVALID_HTTP_STATUS: return "Invalid HTTP status";
        default: return "Unknown error";
    }
}

static void init(void) {
    sg_setup(&(sg_desc){ .context = sapp_sgcontext() });
    sgl_setup(&(sgl_desc_t){0});
    sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_z1013() });
    sfetch_setup(&(sfetch_desc_t){
        .max_requests = 1,
        .num_channels = 1,
        .num_lanes = 1,
    });
    state.pass_action = (sg_pass_action){
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.0f, 0.0f, 0.0f, 1.0f }}
    };

    if (sargs_exists("file")) {
        start_load_file(sargs_value("file"));
    }
}

static void frame(void) {
    sfetch_dowork();

    const float disp_w = sapp_widthf();
    const float disp_h = sapp_heightf();

    sdtx_canvas(disp_w * 0.5f, disp_h * 0.5f);
    sdtx_origin(2.0f, 2.0f);
    if (state.img.id == SG_INVALID_ID) {
        if ((state.file.error != SFETCH_ERROR_NO_ERROR) || (state.file.qoi_decode_failed)) {
            sdtx_printf("ERROR: %s\n\n\n", error_as_string());
        }
        sdtx_printf("Drag'n'drop .qoi image into window\n\n\n"
                    "Left mouse button to drag.\n\n"
                    "Mousewheel to zoom\n\n"
                    "Spacebar to reset\n\n");
    }
    else {

        const float x0 = ((-state.width * 0.5f) * state.scale) + state.offset.x;
        const float x1 = x0 + (state.width * state.scale);
        const float y0 = ((-state.height * 0.5f) * state.scale) + state.offset.y;
        const float y1 = y0 + (state.height * state.scale);

        sgl_defaults();
        sgl_matrix_mode_projection();
        sgl_ortho(-disp_w*0.5f, disp_w*0.5f, disp_h*0.5f, -disp_h*0.5f, -1.0f, +1.0f);
        sgl_enable_texture();
        sgl_texture(state.img);
        sgl_begin_quads();
        sgl_v2f_t2f(x0, y0, 0.0f, 0.0f);
        sgl_v2f_t2f(x1, y0, 1.0f, 0.0f);
        sgl_v2f_t2f(x1, y1, 1.0f, 1.0f);
        sgl_v2f_t2f(x0, y1, 0.0f, 1.0f);
        sgl_end();
    }

    sg_begin_default_passf(&state.pass_action, disp_w, disp_h);
    sgl_draw();
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event* ev) {
    switch (ev->type) {
        case SAPP_EVENTTYPE_FILES_DROPPED:
            start_load_dropped_file();
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (ev->key_code == SAPP_KEYCODE_SPACE) {
                reset_offset_scale();
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            if (ev->modifiers & SAPP_MODIFIER_LMB) {
                move(ev->mouse_dx, ev->mouse_dy);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            scale(ev->scroll_y * 0.25f);
            break;
        default:
            break;
    }
}

static void cleanup(void) {
    sfetch_shutdown();
    sdtx_shutdown();
    sgl_shutdown();
    sg_shutdown();
    sargs_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    sargs_setup(&(sargs_desc){
        .argc = argc,
        .argv = argv,
    });
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = event,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .window_title = "qoiview",
        .enable_dragndrop = true,
        .icon.sokol_default = true,
        .gl_force_gles2 = true,
    };
}
