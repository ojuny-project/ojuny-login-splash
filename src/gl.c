#include "gl.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#define LOAD_REQUIRED(library, field, symbol)                                     \
    do {                                                                          \
        *(void **)(&gl->field) = dlsym(library, symbol);                           \
        if (!gl->field) {                                                          \
            log_error("missing %s", symbol);                                       \
            goto fail;                                                             \
        }                                                                          \
    } while (0)

static void *open_library(const char *first, const char *second) {
    void *library = dlopen(first, RTLD_NOW | RTLD_LOCAL);
    if (!library && second) {
        library = dlopen(second, RTLD_NOW | RTLD_LOCAL);
    }
    return library;
}

bool gl_load(Gl *gl) {
    memset(gl, 0, sizeof(*gl));
    gl->display = EGL_NO_DISPLAY;
    gl->context = EGL_NO_CONTEXT;

#ifdef OJUNY_EGL_VENDOR_DIR
    if (!getenv("__EGL_VENDOR_LIBRARY_DIRS")) {
        setenv("__EGL_VENDOR_LIBRARY_DIRS", OJUNY_EGL_VENDOR_DIR, 0);
    }
#endif
#ifdef OJUNY_GL_DRIVER_DIR
    if (!getenv("LIBGL_DRIVERS_PATH")) {
        setenv("LIBGL_DRIVERS_PATH", OJUNY_GL_DRIVER_DIR, 0);
    }
#endif

    gl->egl_lib = open_library("libEGL.so.1", "libEGL.so");
    if (!gl->egl_lib) {
        log_error("libEGL is not available: %s", dlerror());
        return false;
    }
    gl->gles_lib = open_library("libGLESv2.so.2", "libGLESv2.so");
    if (!gl->gles_lib) {
        log_error("libGLESv2 is not available: %s", dlerror());
        goto fail;
    }
    gl->wl_egl_lib = open_library("libwayland-egl.so.1", "libwayland-egl.so");
    if (!gl->wl_egl_lib) {
        log_error("libwayland-egl is not available: %s", dlerror());
        goto fail;
    }

    LOAD_REQUIRED(gl->egl_lib, p_eglGetError, "eglGetError");
    LOAD_REQUIRED(gl->egl_lib, p_eglInitialize, "eglInitialize");
    LOAD_REQUIRED(gl->egl_lib, p_eglTerminate, "eglTerminate");
    LOAD_REQUIRED(gl->egl_lib, p_eglChooseConfig, "eglChooseConfig");
    LOAD_REQUIRED(gl->egl_lib, p_eglBindAPI, "eglBindAPI");
    LOAD_REQUIRED(gl->egl_lib, p_eglCreateContext, "eglCreateContext");
    LOAD_REQUIRED(gl->egl_lib, p_eglCreateWindowSurface, "eglCreateWindowSurface");
    LOAD_REQUIRED(gl->egl_lib, p_eglMakeCurrent, "eglMakeCurrent");
    LOAD_REQUIRED(gl->egl_lib, p_eglSwapBuffers, "eglSwapBuffers");
    LOAD_REQUIRED(gl->egl_lib, p_eglSwapInterval, "eglSwapInterval");
    LOAD_REQUIRED(gl->egl_lib, p_eglDestroySurface, "eglDestroySurface");
    LOAD_REQUIRED(gl->egl_lib, p_eglDestroyContext, "eglDestroyContext");

    *(void **)(&gl->p_eglGetPlatformDisplay) =
        dlsym(gl->egl_lib, "eglGetPlatformDisplay");
    *(void **)(&gl->p_eglGetPlatformDisplayEXT) =
        dlsym(gl->egl_lib, "eglGetPlatformDisplayEXT");
    if (!gl->p_eglGetPlatformDisplay && !gl->p_eglGetPlatformDisplayEXT) {
        log_error("EGL has no Wayland platform entry point");
        goto fail;
    }

    LOAD_REQUIRED(gl->wl_egl_lib, p_wl_egl_window_create, "wl_egl_window_create");
    LOAD_REQUIRED(gl->wl_egl_lib, p_wl_egl_window_resize, "wl_egl_window_resize");
    LOAD_REQUIRED(gl->wl_egl_lib, p_wl_egl_window_destroy, "wl_egl_window_destroy");

    LOAD_REQUIRED(gl->gles_lib, p_glGetError, "glGetError");
    LOAD_REQUIRED(gl->gles_lib, p_glClearColor, "glClearColor");
    LOAD_REQUIRED(gl->gles_lib, p_glClear, "glClear");
    LOAD_REQUIRED(gl->gles_lib, p_glViewport, "glViewport");
    LOAD_REQUIRED(gl->gles_lib, p_glEnable, "glEnable");
    LOAD_REQUIRED(gl->gles_lib, p_glBlendFunc, "glBlendFunc");
    LOAD_REQUIRED(gl->gles_lib, p_glGenTextures, "glGenTextures");
    LOAD_REQUIRED(gl->gles_lib, p_glBindTexture, "glBindTexture");
    LOAD_REQUIRED(gl->gles_lib, p_glTexParameteri, "glTexParameteri");
    LOAD_REQUIRED(gl->gles_lib, p_glTexImage2D, "glTexImage2D");
    LOAD_REQUIRED(gl->gles_lib, p_glPixelStorei, "glPixelStorei");
    LOAD_REQUIRED(gl->gles_lib, p_glCreateShader, "glCreateShader");
    LOAD_REQUIRED(gl->gles_lib, p_glShaderSource, "glShaderSource");
    LOAD_REQUIRED(gl->gles_lib, p_glCompileShader, "glCompileShader");
    LOAD_REQUIRED(gl->gles_lib, p_glGetShaderiv, "glGetShaderiv");
    LOAD_REQUIRED(gl->gles_lib, p_glGetShaderInfoLog, "glGetShaderInfoLog");
    LOAD_REQUIRED(gl->gles_lib, p_glDeleteShader, "glDeleteShader");
    LOAD_REQUIRED(gl->gles_lib, p_glCreateProgram, "glCreateProgram");
    LOAD_REQUIRED(gl->gles_lib, p_glAttachShader, "glAttachShader");
    LOAD_REQUIRED(gl->gles_lib, p_glLinkProgram, "glLinkProgram");
    LOAD_REQUIRED(gl->gles_lib, p_glGetProgramiv, "glGetProgramiv");
    LOAD_REQUIRED(gl->gles_lib, p_glGetProgramInfoLog, "glGetProgramInfoLog");
    LOAD_REQUIRED(gl->gles_lib, p_glUseProgram, "glUseProgram");
    LOAD_REQUIRED(gl->gles_lib, p_glGetAttribLocation, "glGetAttribLocation");
    LOAD_REQUIRED(gl->gles_lib, p_glGetUniformLocation, "glGetUniformLocation");
    LOAD_REQUIRED(gl->gles_lib, p_glUniform1i, "glUniform1i");
    LOAD_REQUIRED(gl->gles_lib, p_glUniform1f, "glUniform1f");
    LOAD_REQUIRED(gl->gles_lib, p_glGenBuffers, "glGenBuffers");
    LOAD_REQUIRED(gl->gles_lib, p_glBindBuffer, "glBindBuffer");
    LOAD_REQUIRED(gl->gles_lib, p_glBufferData, "glBufferData");
    LOAD_REQUIRED(gl->gles_lib, p_glVertexAttribPointer, "glVertexAttribPointer");
    LOAD_REQUIRED(gl->gles_lib, p_glEnableVertexAttribArray,
                  "glEnableVertexAttribArray");
    LOAD_REQUIRED(gl->gles_lib, p_glDrawArrays, "glDrawArrays");
    return true;

fail:
    gl_unload(gl);
    return false;
}

void gl_unload(Gl *gl) {
    if (gl->egl_lib) {
        dlclose(gl->egl_lib);
    }
    if (gl->gles_lib) {
        dlclose(gl->gles_lib);
    }
    if (gl->wl_egl_lib) {
        dlclose(gl->wl_egl_lib);
    }
    memset(gl, 0, sizeof(*gl));
    gl->display = EGL_NO_DISPLAY;
    gl->context = EGL_NO_CONTEXT;
}

static GLuint compile_shader(Gl *gl, GLenum type, const char *source) {
    GLuint shader = gl->p_glCreateShader(type);
    if (!shader) {
        log_error("glCreateShader failed");
        return 0;
    }
    gl->p_glShaderSource(shader, 1, &source, NULL);
    gl->p_glCompileShader(shader);
    GLint status = 0;
    gl->p_glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[1024];
        GLsizei length = 0;
        gl->p_glGetShaderInfoLog(shader, sizeof(log), &length, log);
        log_error("shader compilation failed: %.*s", (int)length, log);
        gl->p_glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool build_program(Gl *gl) {
    static const char *vertex_source =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
        "    v_uv = a_uv;\n"
        "}\n";
    static const char *fragment_source =
        "precision mediump float;\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_tex;\n"
        "uniform float u_alpha;\n"
        "void main() {\n"
        "    vec4 color = texture2D(u_tex, v_uv);\n"
        "    gl_FragColor = vec4(color.rgb, color.a * u_alpha);\n"
        "}\n";

    GLuint vertex = compile_shader(gl, GL_VERTEX_SHADER, vertex_source);
    if (!vertex) {
        return false;
    }
    GLuint fragment = compile_shader(gl, GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment) {
        gl->p_glDeleteShader(vertex);
        return false;
    }

    GLuint program = gl->p_glCreateProgram();
    if (!program) {
        log_error("glCreateProgram failed");
        gl->p_glDeleteShader(vertex);
        gl->p_glDeleteShader(fragment);
        return false;
    }
    gl->p_glAttachShader(program, vertex);
    gl->p_glAttachShader(program, fragment);
    gl->p_glLinkProgram(program);

    GLint status = 0;
    gl->p_glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[1024];
        GLsizei length = 0;
        gl->p_glGetProgramInfoLog(program, sizeof(log), &length, log);
        log_error("program link failed: %.*s", (int)length, log);
        gl->p_glDeleteShader(vertex);
        gl->p_glDeleteShader(fragment);
        return false;
    }
    gl->p_glDeleteShader(vertex);
    gl->p_glDeleteShader(fragment);

    gl->program = program;
    gl->a_pos = gl->p_glGetAttribLocation(program, "a_pos");
    gl->a_uv = gl->p_glGetAttribLocation(program, "a_uv");
    gl->u_tex = gl->p_glGetUniformLocation(program, "u_tex");
    gl->u_alpha = gl->p_glGetUniformLocation(program, "u_alpha");
    if (gl->a_pos < 0 || gl->a_uv < 0 || gl->u_tex < 0 || gl->u_alpha < 0) {
        log_error("shader interface is incomplete");
        return false;
    }
    gl->p_glUseProgram(program);
    gl->p_glUniform1i(gl->u_tex, 0);
    return true;
}

static bool upload_texture(Gl *gl, GLuint *texture, uint32_t width, uint32_t height,
                           const uint8_t *pixels) {
    gl->p_glGenTextures(1, texture);
    if (!*texture) {
        log_error("glGenTextures failed");
        return false;
    }
    gl->p_glBindTexture(GL_TEXTURE_2D, *texture);
    gl->p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLenum error = gl->p_glGetError();
    if (error != GL_NO_ERROR) {
        log_error("texture upload failed: 0x%x", error);
        return false;
    }
    return true;
}

static bool choose_config(Gl *gl) {
    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLint count = 0;
    if (gl->p_eglChooseConfig(gl->display, attributes, &gl->config, 1, &count) &&
        count > 0) {
        return true;
    }

    EGLint fallback[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE,
    };
    count = 0;
    if (gl->p_eglChooseConfig(gl->display, fallback, &gl->config, 1, &count) &&
        count > 0) {
        return true;
    }
    log_error("no suitable EGL config");
    return false;
}

bool gl_init(Gl *gl, struct wl_display *display, const Assets *assets) {
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    if (gl->p_eglGetPlatformDisplay) {
        egl_display = gl->p_eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display,
                                                  NULL);
    } else if (gl->p_eglGetPlatformDisplayEXT) {
        egl_display = gl->p_eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_KHR,
                                                     display, NULL);
    }
    if (egl_display == EGL_NO_DISPLAY) {
        log_error("eglGetPlatformDisplay(Wayland) failed: 0x%x", gl->p_eglGetError());
        return false;
    }
    gl->display = egl_display;

    EGLint major = 0;
    EGLint minor = 0;
    if (!gl->p_eglInitialize(gl->display, &major, &minor)) {
        log_error("eglInitialize failed: 0x%x", gl->p_eglGetError());
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }
    if (!gl->p_eglBindAPI(EGL_OPENGL_ES_API)) {
        log_error("eglBindAPI failed: 0x%x", gl->p_eglGetError());
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }
    if (!choose_config(gl)) {
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }

    EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    gl->context = gl->p_eglCreateContext(gl->display, gl->config, EGL_NO_CONTEXT,
                                         context_attributes);
    if (gl->context == EGL_NO_CONTEXT) {
        log_error("eglCreateContext failed: 0x%x", gl->p_eglGetError());
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }

    if (!gl->p_eglMakeCurrent(gl->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                              gl->context)) {
        log_error("eglMakeCurrent without a surface failed: 0x%x",
                  gl->p_eglGetError());
        gl->p_eglDestroyContext(gl->display, gl->context);
        gl->context = EGL_NO_CONTEXT;
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }

    gl->p_eglSwapInterval(gl->display, 0);
    if (!build_program(gl)) {
        gl->p_eglDestroyContext(gl->display, gl->context);
        gl->context = EGL_NO_CONTEXT;
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }

    gl->p_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (!upload_texture(gl, &gl->logo_tex, assets->logo_w, assets->logo_h,
                        assets->logo) ||
        !upload_texture(gl, &gl->text_tex, assets->text_w, assets->text_h,
                        assets->text) ||
        (assets->tip &&
         !upload_texture(gl, &gl->tip_tex, assets->tip_w, assets->tip_h,
                         assets->tip))) {
        gl->p_eglDestroyContext(gl->display, gl->context);
        gl->context = EGL_NO_CONTEXT;
        gl->p_eglTerminate(gl->display);
        gl->display = EGL_NO_DISPLAY;
        return false;
    }

    gl->p_glGenBuffers(1, &gl->vbo);
    gl->p_glEnable(GL_BLEND);
    gl->p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->ready = true;
    log_info("EGL %d.%d ready", (int)major, (int)minor);
    return true;
}

bool gl_surface_create(Gl *gl, struct wl_surface *surface, int width, int height,
                       struct wl_egl_window **out_window,
                       EGLSurface *out_surface) {
    struct wl_egl_window *window =
        gl->p_wl_egl_window_create(surface, width, height);
    if (!window) {
        log_error("wl_egl_window_create failed");
        return false;
    }
    EGLSurface egl_surface = gl->p_eglCreateWindowSurface(
        gl->display, gl->config, (void *)window, NULL);
    if (egl_surface == EGL_NO_SURFACE) {
        log_error("eglCreateWindowSurface failed: 0x%x", gl->p_eglGetError());
        gl->p_wl_egl_window_destroy(window);
        return false;
    }
    *out_window = window;
    *out_surface = egl_surface;
    return true;
}

void gl_surface_destroy(Gl *gl, struct wl_egl_window *window,
                        EGLSurface surface) {
    if (gl->display != EGL_NO_DISPLAY && gl->context != EGL_NO_CONTEXT) {
        gl->p_eglMakeCurrent(gl->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                             gl->context);
    }
    if (surface != EGL_NO_SURFACE) {
        gl->p_eglDestroySurface(gl->display, surface);
    }
    if (window) {
        gl->p_wl_egl_window_destroy(window);
    }
}
