{
  lib,
  stdenv,
  pkg-config,
  patchelf,
  wayland,
  wayland-scanner,
  libGL,
  mesa,
  assets,
}:

stdenv.mkDerivation {
  pname = "ojuny-login-splash";
  version = "0.1.0";

  src = lib.cleanSource ../.;

  nativeBuildInputs = [
    pkg-config
    patchelf
    wayland
    wayland-scanner
    libGL
  ];
  buildInputs = [
    wayland
  ];

  makeFlags = [
    "ASSET_DIR=${assets}"
    "VERSION=0.1.0"
    "EGL_VENDOR_DIR=${mesa}/share/glvnd/egl_vendor.d"
    "GL_DRIVER_DIR=${mesa}/lib/dri"
    "RPATH=${lib.makeLibraryPath [ libGL wayland mesa ]}"
  ];

  installPhase = ''
    runHook preInstall
    install -Dm755 build/ojuny-login-splash "$out/bin/ojuny-login-splash"
    runHook postInstall
  '';

  postFixup = ''
    patchelf --set-rpath "${lib.makeLibraryPath [ libGL wayland mesa ]}" \
      "$out/bin/ojuny-login-splash"
  '';

  meta = {
    description = "Minimal Ojuny login splash for mango";
    license = lib.licenses.gpl3Only;
    mainProgram = "ojuny-login-splash";
    platforms = lib.platforms.linux;
  };
}
