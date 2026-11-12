{
  lib,
  stdenv,
  python3,
  dejavu_fonts,
  logoSrc ? ../assets/ojuny-logo.png,
  background ? "#242424",
  foreground ? "#d5c4a1",
  sourceFills ? [ "#4F5B58" "#E67E80" "#7FBBB3" "#DBBC7F" ],
  targetFills ? sourceFills,
  font ? "${dejavu_fonts}/share/fonts/truetype/DejaVuSans.ttf",
  fontSize ? 32,
  text ? "Welcome to Ojuny",
  borderPercent ? 22,
  textGap ? 32,
}:

stdenv.mkDerivation {
  pname = "ojuny-login-splash-assets";
  version = "0.1.0";

  dontUnpack = true;
  dontConfigure = true;
  dontBuild = true;

  nativeBuildInputs = [
    (python3.withPackages (packages: [
      packages.pillow
      packages.numpy
    ]))
  ];

  installPhase = ''
    runHook preInstall
    mkdir -p "$out"
    python3 ${../scripts/gen-assets.py} \
      --logo ${logoSrc} \
      --out-dir "$out" \
      --background "${background}" \
      --foreground "${foreground}" \
      --source-fills "${lib.concatStringsSep "," sourceFills}" \
      --target-fills "${lib.concatStringsSep "," targetFills}" \
      --font "${font}" \
      --font-size ${toString fontSize} \
      --text "${text}" \
      --border-percent ${toString borderPercent} \
      --text-gap ${toString textGap}
    runHook postInstall
  '';
}
