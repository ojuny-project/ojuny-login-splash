{
  pkgs,
  ...
}:

pkgs.mkShell {
  packages = with pkgs; [
    gcc
    gnumake
    pkg-config
    wayland
    wayland-scanner
    libGL
    mesa
    (python3.withPackages (packages: [
      packages.pillow
      packages.numpy
    ]))
    dejavu_fonts
    gdb
    strace
    sway
    wtype
    grim
    jq
  ];

  DEJAVU_FONT = "${pkgs.dejavu_fonts}/share/fonts/truetype/DejaVuSans.ttf";
  ASSET_DIR = "build/assets";

  shellHook = ''
    export DEJAVU_FONT
    export ASSET_DIR
    export EGL_VENDOR_DIR=${pkgs.mesa}/share/glvnd/egl_vendor.d
    export GL_DRIVER_DIR=${pkgs.mesa}/lib/dri
    export RPATH=${pkgs.lib.makeLibraryPath [ pkgs.libGL pkgs.wayland pkgs.mesa ]}
    echo "ojuny-login-splash: make assets && make test"
  '';
}
