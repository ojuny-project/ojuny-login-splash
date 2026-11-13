# ojuny-login-splash

A minimal, fast login splash for mango. It starts with the compositor session, covers mango and dms while they load, shows the Ojuny logo and "Welcome to Ojuny", and exits on a timer, on ESC, or on `--quit`.

## Requirements

- A Wayland compositor with `zwlr_layer_shell_v1`. Mango qualifies.
- `wl_shm` for the first frame and the no-EGL fallback.
- Mesa EGL and GLES2 for the animation. Hardware drivers are used when available; llvmpipe is the software path.
- A systemd user session for the service.

## Build

```sh
nix build
```

Run directly:

```sh
./result/bin/ojuny-login-splash --seconds 5
```

Development:

```sh
nix develop
make assets
make
make test
```

`make assets` needs `DEJAVU_FONT`, which the devshell sets. The default asset directory is `build/assets`. Pass `TIPS_FILE=/path/to/tips.txt` to render tips locally.

Integration tests need a headless compositor and the tools from the devshell:

```sh
make integration
```

## Usage

```
ojuny-login-splash [options]

  --seconds N   exit after N seconds
  --fade N      fade and grow over N seconds instead of rotating
  --fade-in N   asset fade-in over N seconds (default 0.3)
  --fade-out N  fade out over N seconds before the timer exits (default 0.3)
  --quit        stop all running instances
  --help        show help
  --version     show the version
```

Default animation:

- 60 degree rotation over 0.7 seconds with smoothstep easing.
- 0.3 second hold.
- Repeats from the next 60 degree step.
- Logo and text fade in over 0.3 seconds.
- A timed exit fades the assets and the background out over 0.3 seconds.
- Renders on `wl_surface.frame` callbacks at the output refresh rate. There is no fixed frame rate.

`--fade N` disables rotation. Logo and text fade from alpha 0 to 1 and grow from 0.85 to 1.0 over N seconds. The final frame is kept. `--fade-in` and `--fade-out` work with both the rotation and the `--fade` mode. Set either to 0 to disable it. ESC and `--quit` always exit immediately; the timed fade-out does not delay them.

The background fade uses the EGL buffer alpha. When the driver does not provide an alpha config, the background stays opaque while the assets still fade.

`--quit` scans `$XDG_RUNTIME_DIR/ojuny-login-splash/*.sock`, sends a quit byte to every instance, removes stale sockets, and exits.

ESC exits immediately. The layer surface requests exclusive keyboard focus.

## Home Manager service

```nix
{
  imports = [ inputs.ojuny-login-splash.homeManagerModules.default ];

  services.ojuny-login-splash.enable = true;
  services.ojuny-login-splash.seconds = 10;
  services.ojuny-login-splash.showOnRebuild = false;
  services.ojuny-login-splash.fadeIn = 0.3;
  services.ojuny-login-splash.fadeOut = 0.3;
  # services.ojuny-login-splash.fade = 2.5;
}
```

The service is:

- `WantedBy = [ "mango-session.target" ]`
- `PartOf = [ "mango-session.target" ]`
- `After = [ "graphical-session-pre.target" ]`
- `Before = [ "xdg-desktop-autostart.target" ]`
- `ExecStop = ... --quit`
- `Restart = no`
- `TimeoutStopSec = 1`

No mango `exec_once` entry is needed.

If mango has `layer_animations=1`, disable the layer animation for the splash namespace:

```
layerrule=noanim:1,noblur:1,noshadow:1,layer_name:ojuny-login-splash
```

## Releases

A release publishes:

- `ojuny-login-splash.bin`: the arx bundle used by the Nix binary package.
- `ojuny-login-splash-flake.tar.gz`: a flake pinned to the tagged source.
- `SHA256SUMS`.

Install the prebuilt package through Nix. Do not run the `.bin` directly:

```sh
nix build github:ojuny-project/ojuny-login-splash#packages.x86_64-linux.ojuny-login-splash-binary
```

`nix/binary.nix` extracts the bundle during the Nix build and installs the executable. The installed wrapper does not extract at run time.

The release flake installs the tagged source through Nix:

```sh
nix build 'github:ojuny-project/ojuny-login-splash/<tag>'
```

## Assets

Assets are generated at build time as raw RGBA files in a dedicated asset directory referenced by the binary. There is no runtime decoding:

- Logo: source image, optional recolor, 22 percent transparent border.
- Text: "Welcome to Ojuny", rendered with the chosen font.
- Tips: zero or more subtext lines, pre-rendered with a smaller font.
- Metadata: background color, logo size, text size, text gap.

Defaults:

- Background `#242424`
- Foreground `#d5c4a1`
- Logo source colors unchanged
- Font DejaVu Sans 32px
- Logo content 512px
- Text gap 32px
- Tip font 18px, tip color `#a89984`, tip gap 10px
- No tips by default

Add tips through the asset derivation:

```nix
tips = [
  "Press Super+Return to open a terminal."
  "Super+R reloads the mango configuration."
];
tipForeground = config.lib.stylix.colors.base04;
```

One tip is chosen at random per splash instance. The tips are rendered at build time, so the runtime still has no font stack. In `ojuny-desktop`, set `desktop.ojuny.splash.tips`; the template settings page exposes it as an editable list.

Override the asset derivation to match Stylix:

```nix
services.ojuny-login-splash.package =
  pkgs.callPackage "${inputs.ojuny-login-splash}/nix/package.nix" {
    assets = pkgs.callPackage "${inputs.ojuny-login-splash}/nix/assets.nix" {
      background = config.lib.stylix.colors.base00;
      foreground = config.lib.stylix.colors.base05;
      targetFills = [
        config.lib.stylix.colors.base03
        config.lib.stylix.colors.base08
        config.lib.stylix.colors.base0D
        config.lib.stylix.colors.base0A
      ];
    };
  };
```

## Startup and rendering

1. Parse arguments, load assets, create the runtime socket.
2. Connect to Wayland and bind the required globals.
3. Create one overlay layer surface per output.
4. On the first configure, commit an opaque `wl_shm` frame. In rotation mode this frame already contains the logo at angle 0 and the text.
5. Initialize EGL and GLES2, start the animation clock, and draw the first EGL frame.
6. Keep the shm frame until compositor release. If EGL cannot initialize, keep it and continue handling ESC, `--seconds`, and `--quit`.

The shm frame is the visibility guarantee. EGL startup delays the animation, never the cover.

## Layout

The plymouth theme uses a 512px logo with a 22 percent border and places text at `H/2 + 512/2 + 32`. The splash uses the same values:

- Logo center at `(W/2, H/2)`.
- Text top at `H/2 + 256 + 32`.
- Opaque background across the full output.
- Buffer scale follows the output scale, so the logo keeps the plymouth physical size on HiDPI.

## Performance

- Runtime dependencies: libc and libwayland-client. EGL, GLES2, and wayland-egl are loaded with `dlopen` and are optional.
- First shm frame: tens of milliseconds after the compositor sends configure.
- EGL startup: under 100ms typical, under 300ms on old software rendering.
- Steady state: one clear and two textured quads per frame.
- Native refresh pacing with `wl_surface.frame`.
- `eglSwapInterval(display, 0)` avoids a second vsync.
- Time-based animation drops frames instead of slowing down.

## Limitations

- Wayland only.
- Requires `zwlr_layer_shell_v1`.
- Without Mesa EGL, the animation is unavailable and the static frame remains.
- ESC requires the compositor to grant exclusive keyboard focus.
- `--quit` must run with the same `XDG_RUNTIME_DIR` as the service.
- No cursor hiding.

## License

See `LICENSE`.
