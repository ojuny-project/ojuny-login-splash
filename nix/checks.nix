{
  lib,
  stdenv,
  pkg-config,
  wayland,
}:

stdenv.mkDerivation {
  pname = "ojuny-login-splash-tests";
  version = "0.1.0";

  src = lib.cleanSource ../.;

  nativeBuildInputs = [
    pkg-config
    wayland
  ];

  buildPhase = ''
    runHook preBuild
    make test
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    touch "$out"
    runHook postInstall
  '';
}
