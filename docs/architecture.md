# Architecture

## Process

`ojuny-login-splash` is a single-threaded Wayland client. It owns one process per session and one layer surface per output.

Startup:

1. Parse arguments.
2. Load `meta`, `logo.rgba`, and `text.rgba` from the build-time asset directory.
3. Open the runtime socket at `$XDG_RUNTIME_DIR/ojuny-login-splash/<pid>.sock`.
4. Connect to Wayland.
5. Bind `wl_compositor`, `wl_shm`, `wl_seat`, `wl_output`, and `zwlr_layer_shell_v1`.
6. Create one `zwlr_layer_surface_v1` per output.
7. Wait for configure.
8. Commit the shm cover frame.
9. Initialize EGL and GLES2.
10. Draw the first EGL frame and enter the event loop.

## Layer surface

- Layer: overlay.
- Namespace: `ojuny-login-splash`.
- Anchor: top, bottom, left, right.
- Size: `0, 0`, which lets the compositor assign the output size.
- Exclusive zone: `-1`, so the surface extends over panels and layer surfaces.
- Keyboard interactivity: exclusive.
- Opaque region: the full logical surface.
- Input region: default, full surface, so pointer input is consumed.

The overlay layer is above dms bars and notifications. A normal xdg-toplevel would sit below them.

## First frame

The first frame uses `wl_shm`:

- `WL_SHM_FORMAT_XRGB8888`.
- Physical buffer size `logical width * output scale`.
- `wl_surface.set_buffer_scale(output scale)`.
- Background fill from the asset metadata.
- In rotation mode, alpha-blended logo and text at angle 0.

The frame is committed as soon as the layer surface is configured. It does not wait for EGL. On slow software rendering or cold shader compilation, the screen is already covered.

## EGL

- Loaded with `dlopen`: `libEGL.so.1`, `libGLESv2.so.2`, `libwayland-egl.so.1`.
- `eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, wl_display, NULL)`.
- One EGL display, one config, one context.
- One `wl_egl_window` and EGL surface per output.
- `eglSwapInterval(display, 0)`.
- Two textures: logo and text.
- One GLES2 program.
- Per frame: make current, viewport, clear, draw logo quad, draw text quad, damage, swap.

The logo quad is rotated on the CPU into four NDC vertices. The text quad is scaled around its center. The transparent logo border keeps the rotated content from clipping.

## Animation

`CLOCK_MONOTONIC` is the only clock. The frame count is never used.

Rotation:

- `phase = elapsed - floor(elapsed)`.
- `base = fmod(floor(elapsed) * pi/3, 2*pi)`.
- `phase < 0.7`: `angle = base + (pi/3) * smoothstep(phase/0.7)`.
- Otherwise: `angle = base + pi/3`.

Fade:

- `p = clamp(elapsed / fade, 0, 1)`.
- `e = smoothstep(p)`.
- `alpha = e`.
- `scale = 0.85 + 0.15 * e`.
- `done` when `p == 1`.

Frame callbacks:

- Requested before the swap that produces the frame.
- In rotation mode, requested forever.
- In fade mode, not requested after the final frame.

A missed frame drops. The next callback samples the current time, so the animation stays aligned with wall clock.

## IPC

Each process binds `$XDG_RUNTIME_DIR/ojuny-login-splash/<pid>.sock`.

`--quit`:

1. Open the runtime directory.
2. Find every `*.sock`.
3. Connect.
4. Write one byte, `q`.
5. Unlink sockets that refuse the connection.
6. Exit 0.

The running process polls the socket alongside the Wayland display. On `q` it stops the loop and exits.

## Service

The Home Manager module creates:

```ini
[Unit]
Description=Ojuny login splash
PartOf=mango-session.target
After=graphical-session-pre.target
Before=xdg-desktop-autostart.target

[Service]
Type=exec
ExecStart=... --seconds 10
ExecStop=... --quit
Restart=no
TimeoutStopSec=1

[Install]
WantedBy=mango-session.target
```

`Type=exec` does not delay the session target. The splash wins the race against dms because it is much smaller, and the layer surface is on the overlay layer.

## Software rendering

Mesa tries the hardware driver, then falls back to llvmpipe. The app does not branch on the renderer.

If EGL initialization fails completely, the shm frame remains. ESC, `--seconds`, and `--quit` still work. In fade mode the full static frame is composited before the fallback is entered.
