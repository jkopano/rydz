{
  pkgs,
  lib,
  config,
  inputs,
  ...
}:

let
  raylibDeps = with pkgs; [
    mesa
    mesa.drivers
    libGL
    libglvnd
    xorg.libX11
    xorg.libXcursor
    xorg.libXi
    xorg.libXinerama
    xorg.libXrandr
    xorg.libXext
    xorg.libXxf86vm
    alsa-lib
    wayland
    wayland-scanner
    wayland-protocols
    libxkbcommon
  ];

  compDeps = with pkgs; [
    xmake
    vcpkg
    luajit
    sol2
    glm
    # lld_22
    mold
  ];

  typstDeps = with pkgs; [
    typst
    plantuml
  ];
in
{
  packages = raylibDeps ++ compDeps ++ typstDeps;

  languages.cplusplus.enable = true;

  env.PKG_CONFIG_PATH = lib.makeSearchPath "lib/pkgconfig" compDeps;
  env.LD_LIBRARY_PATH = lib.makeLibraryPath raylibDeps;
}
