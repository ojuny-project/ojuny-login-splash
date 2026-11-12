# Performance

## Budget

| Milestone | Target |
|---|---|
| Process start to Wayland connect | under 30ms |
| Connect to first shm frame | under 100ms |
| First shm frame to first EGL frame | under 300ms on old software rendering |
| Steady frame | one clear and two textured quads |
| Native refresh | frame-callback paced, no fixed rate |

## How the budget is protected

- The first frame uses `wl_shm` and is committed before EGL initialization.
- The logo and text are pre-rendered at build time. There is no image decoder or font stack at runtime.
- EGL, GLES2, and wayland-egl are loaded with `dlopen`, so a missing EGL does not prevent startup.
- The static frame is kept until the compositor releases it.
- The animation is time-based. A missed frame does not slow it down.
- `eglSwapInterval(display, 0)` prevents a second vsync.
- No toolkit, no event loop library, no allocation in the steady-state draw path.

## Measuring

The splash logs:

```
[ojuny-splash] first shm frame on N output(s) after X ms
[ojuny-splash] first EGL frame after X ms
```

Run with `--seconds 3` and read the journal or stderr.

## Software rendering test

```sh
LIBGL_ALWAYS_SOFTWARE=1 ./result/bin/ojuny-login-splash --seconds 3
```

Mesa should report llvmpipe. The static frame appears immediately. The animation runs at the rate the CPU can sustain.

## Hardware test

Run on the target compositor at its native refresh. The animation samples the current time on every frame callback, so a 60Hz output gets 60 samples per second and a 600Hz output gets 600.

## Known tradeoffs

- The shm cover uses one full-size buffer until the first EGL frame is displayed. At 4K this is about 33MB temporarily.
- The logo texture includes a 22 percent transparent border, which is about 2MB at 512px content size.
- EGL startup on llvmpipe can take a few hundred milliseconds on very old hardware. This delays the animation only.
