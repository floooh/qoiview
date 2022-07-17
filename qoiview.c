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

#define MAX_FILE_SIZE (300 * 1024 * 1024)
#define MAX_SCALE (8.0f)
#define MIN_SCALE (0.25f)

static struct {
    struct {
        sg_image img;
        sgl_pipeline pip;
        float width;
        float height;
        float scale;
        struct { float x, y; } offset;
        struct { float r, g, b; } color;
    } image;
    sg_pass_action pass_action;
    sg_image checkerboard_img;
    struct {
        sfetch_error_t error;
        bool qoi_decode_failed;
        uint8_t buf[MAX_FILE_SIZE];
    } file;
} state;

static void reset_image_params(void) {
    state.image.scale = 1.0f;
    state.image.offset.x = 0.0f;
    state.image.offset.y = 0.0f;
    state.image.color.r = 1.0f;
    state.image.color.g = 1.0f;
    state.image.color.b = 1.0f;
}

static void scale(float d) {
    state.image.scale += d;
    if (state.image.scale > MAX_SCALE) {
        state.image.scale = MAX_SCALE;
    }
    else if (state.image.scale < MIN_SCALE) {
        state.image.scale = MIN_SCALE;
    }
}

static void move(float dx, float dy) {
    state.image.offset.x += (dx / state.image.scale);
    state.image.offset.y += (dy / state.image.scale);
}

static void create_image(void* ptr, size_t size) {
    reset_image_params();
    state.file.qoi_decode_failed = false;
    if (state.image.img.id != SG_INVALID_ID) {
        sg_destroy_image(state.image.img);
        state.image.img.id = SG_INVALID_ID;
    }
    qoi_desc qoi;
    void* pixels = qoi_decode(ptr, (int)size, &qoi, 4);
    if (!pixels) {
        state.file.qoi_decode_failed = true;
        return;
    }
    state.image.width = (float) qoi.width;
    state.image.height = (float) qoi.height;        
    state.image.img = sg_make_image(&(sg_image_desc){
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .width = qoi.width,
        .height = qoi.height,
        .mag_filter = SG_FILTER_NEAREST,
        .min_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = {
            .ptr = pixels,
            .size = qoi.width * qoi.height * 4
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
    sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_cpc() });
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

    // create a pipeline object with alpha blending for rendering the loaded image
    state.image.pip = sgl_make_pipeline(&(sg_pipeline_desc){
        .colors[0] = {
            .write_mask = SG_COLORMASK_RGB,
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            }
        }
    });

    // texture for rendering checkboard background
    uint32_t pixels[4][4];
    for (uint32_t y = 0; y < 4; y++) {
        for (uint32_t x = 0; x < 4; x++) {
            pixels[y][x] = ((x ^ y) & 1) ? 0xFF666666 : 0xFF333333;
        }
    }
    state.checkerboard_img = sg_make_image(&(sg_image_desc){
        .width = 4,
        .height = 4,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
        .data.subimage[0][0] = SG_RANGE(pixels)
    });
}

static void frame(void) {
    sfetch_dowork();

    const float disp_w = sapp_widthf();
    const float disp_h = sapp_heightf();

    sgl_defaults();
    sgl_enable_texture();
    sgl_matrix_mode_projection();
    sgl_ortho(-disp_w*0.5f, disp_w*0.5f, disp_h*0.5f, -disp_h*0.5f, -1.0f, +1.0f);

    // draw checkerboard background
    {
        const float x0 = -disp_w * 0.5f;
        const float x1 = x0 + disp_w;
        const float y0 = -disp_h * 0.5f;
        const float y1 = y0 + disp_h;

        const float u0 = (x0 / 32.0f);
        const float u1 = (x1 / 32.0f);
        const float v0 = (y0 / 32.0f);
        const float v1 = (y1 / 32.0f);

        sgl_texture(state.checkerboard_img);
        sgl_begin_quads();
        sgl_v2f_t2f(x0, y0, u0, v0);
        sgl_v2f_t2f(x1, y0, u1, v0);
        sgl_v2f_t2f(x1, y1, u1, v1);
        sgl_v2f_t2f(x0, y1, u0, v1);
        sgl_end();
    }

    if (state.image.img.id == SG_INVALID_ID) {
        // draw instructions
        sdtx_canvas(disp_w * 0.5f, disp_h * 0.5f);
        sdtx_origin(2.0f, 2.0f);
        if ((state.file.error != SFETCH_ERROR_NO_ERROR) || (state.file.qoi_decode_failed)) {
            sdtx_printf("ERROR: %s\n\n\n", error_as_string());
        }
        sdtx_printf("Drag'n'drop .qoi image into window\n\n\n"
                    "LMB: drag image\n\n"
                    "Wheel: zoom image\n\n"
                    "1,2,3: RGB channels on/off\n\n"
                    "Spacebar: reset\n\n");
    }
    else {
        // draw actual image
        const float x0 = ((-state.image.width * 0.5f) * state.image.scale) + (state.image.offset.x * state.image.scale);
        const float x1 = x0 + (state.image.width * state.image.scale);
        const float y0 = ((-state.image.height * 0.5f) * state.image.scale) + (state.image.offset.y * state.image.scale);
        const float y1 = y0 + (state.image.height * state.image.scale);

        sgl_texture(state.image.img);
        sgl_load_pipeline(state.image.pip);
        sgl_c3f(state.image.color.r, state.image.color.g, state.image.color.b);
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
        case SAPP_EVENTTYPE_KEY_UP:
            switch (ev->key_code) {
                case SAPP_KEYCODE_SPACE: reset_image_params(); break;
                case SAPP_KEYCODE_1: state.image.color.r = (state.image.color.r == 0.0f) ? 1.0f : 0.0f; break;
                case SAPP_KEYCODE_2: state.image.color.g = (state.image.color.g == 0.0f) ? 1.0f : 0.0f; break;
                case SAPP_KEYCODE_3: state.image.color.b = (state.image.color.b == 0.0f) ? 1.0f : 0.0f; break;
                default: break;
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
