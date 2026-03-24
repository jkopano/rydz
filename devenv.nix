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

  compDeps = with pkgs; [
    xmake
    vcpkg
  ];

  typstDeps = with pkgs; [
    typst
    plantuml
  ];
in
{
  packages = raylibDeps ++ compDeps ++ typstDeps ++ [ pkgs.gpu-viewer ];

  languages.cplusplus.enable = true;

  env.LD_LIBRARY_PATH = lib.makeLibraryPath raylibDeps;
}
