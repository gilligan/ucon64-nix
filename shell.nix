{ system ? builtins.currentSystem
, pins ? import ./npins
, pkgs ? import pins.nixpkgs { inherit system; }
, npins ? import pins.npins { inherit pkgs; }
}:

pkgs.mkShell {
  buildInputs = with pkgs; [ gnumake automake autoconf libusb npins ];
}
