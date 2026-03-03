{
  pkgs,
  lib,
  config,
  inputs,
  ...
}:

let
  raylibDeps = with pkgs; [
    libGL
    xorg.libX11
    xorg.libXcursor
    xorg.libXi
    xorg.libXinerama
    xorg.libXrandr
    alsa-lib
    wayland
    wayland-scanner
    wayland-protocols
    libxkbcommon
  ];
in
{
  packages = raylibDeps ++ [ pkgs.xmake ];

  languages.cplusplus.enable = true;

  env.LD_LIBRARY_PATH = lib.makeLibraryPath raylibDeps;
}
