#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "assets.h"

typedef struct Gl {
    void *egl_lib;
    void *gles_lib;
    void *wl_egl_lib;

    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    bool ready;

    EGLint (*p_eglGetError)(void);
    EGLDisplay (*p_eglGetPlatformDisplay)(EGLenum, void *, const EGLAttrib *);
    EGLDisplay (*p_eglGetPlatformDisplayEXT)(EGLenum, void *, const EGLint *);
    EGLBoolean (*p_eglInitialize)(EGLDisplay, EGLint *, EGLint *);
    EGLBoolean (*p_eglTerminate)(EGLDisplay);
    EGLBoolean (*p_eglChooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint,
                                    EGLint *);
    EGLBoolean (*p_eglBindAPI)(EGLenum);
    EGLContext (*p_eglCreateContext)(EGLDisplay, EGLConfig, EGLContext,
                                     const EGLint *);
    EGLSurface (*p_eglCreateWindowSurface)(EGLDisplay, EGLConfig, void *,
                                           const EGLint *);
    EGLBoolean (*p_eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLBoolean (*p_eglSwapBuffers)(EGLDisplay, EGLSurface);
    EGLBoolean (*p_eglSwapInterval)(EGLDisplay, EGLint);
    EGLBoolean (*p_eglDestroySurface)(EGLDisplay, EGLSurface);
    EGLBoolean (*p_eglDestroyContext)(EGLDisplay, EGLContext);

    struct wl_egl_window *(*p_wl_egl_window_create)(struct wl_surface *, int, int);
    void (*p_wl_egl_window_resize)(struct wl_egl_window *, int, int, int, int);
    void (*p_wl_egl_window_destroy)(struct wl_egl_window *);

    GLuint program;
    GLuint vbo;
    GLuint logo_tex;
    GLuint text_tex;
    GLint a_pos;
    GLint a_uv;
    GLint u_tex;
    GLint u_alpha;

    GLenum (*p_glGetError)(void);
    void (*p_glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*p_glClear)(GLbitfield);
    void (*p_glViewport)(GLint, GLint, GLsizei, GLsizei);
    void (*p_glEnable)(GLenum);
    void (*p_glBlendFunc)(GLenum, GLenum);
    void (*p_glGenTextures)(GLsizei, GLuint *);
    void (*p_glBindTexture)(GLenum, GLuint);
    void (*p_glTexParameteri)(GLenum, GLenum, GLint);
    void (*p_glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
                           GLenum, const void *);
    void (*p_glPixelStorei)(GLenum, GLint);
    GLuint (*p_glCreateShader)(GLenum);
    void (*p_glShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
    void (*p_glCompileShader)(GLuint);
    void (*p_glGetShaderiv)(GLuint, GLenum, GLint *);
    void (*p_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (*p_glDeleteShader)(GLuint);
    GLuint (*p_glCreateProgram)(void);
    void (*p_glAttachShader)(GLuint, GLuint);
    void (*p_glLinkProgram)(GLuint);
    void (*p_glGetProgramiv)(GLuint, GLenum, GLint *);
    void (*p_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (*p_glUseProgram)(GLuint);
    GLint (*p_glGetAttribLocation)(GLuint, const GLchar *);
    GLint (*p_glGetUniformLocation)(GLuint, const GLchar *);
    void (*p_glUniform1i)(GLint, GLint);
    void (*p_glUniform1f)(GLint, GLfloat);
    void (*p_glGenBuffers)(GLsizei, GLuint *);
    void (*p_glBindBuffer)(GLenum, GLuint);
    void (*p_glBufferData)(GLenum, GLsizeiptr, const void *, GLenum);
    void (*p_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                                    const void *);
    void (*p_glEnableVertexAttribArray)(GLuint);
    void (*p_glDrawArrays)(GLenum, GLint, GLsizei);
} Gl;

bool gl_load(Gl *gl);
void gl_unload(Gl *gl);
bool gl_init(Gl *gl, struct wl_display *display, const Assets *assets);
bool gl_surface_create(Gl *gl, struct wl_surface *surface, int width, int height,
                       struct wl_egl_window **out_window,
                       EGLSurface *out_surface);
void gl_surface_destroy(Gl *gl, struct wl_egl_window *window,
                        EGLSurface surface);
