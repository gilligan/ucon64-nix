{ system ? builtins.currentSystem
, pins ? import ./npins
, pkgs ? import pins.nixpkgs { inherit system; }
}:
let
  inherit (pkgs) stdenv;
in
  stdenv.mkDerivation {
    version = "2.2.2";
    pname = "ucon64";
    src = ./src;

    buildInputs = with pkgs; [ automake autoconf gnumake libusb ];
    configureFlags = [ "--with-libusb" ];
    installPhase = ''
      mkdir -p $out/bin
      cp ucon64 $out/bin
    '';
  }
