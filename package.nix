{ stdenv, meson, ninja }:
stdenv.mkDerivation {
  name = "ld-audit-nix-cache-lookup";

  src = ./src;
  nativeBuildInputs = [ meson ninja ];
}
