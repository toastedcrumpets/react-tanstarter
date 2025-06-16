{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    esp-idf.url = "github:mirrexagon/nixpkgs-esp-dev";
    esp-qemu.url = "github:SFrijters/nix-qemu-espressif";
  };

  outputs = { self, nixpkgs, flake-utils, esp-idf, esp-qemu, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            esp-idf.overlays.default
            esp-qemu.overlays.default
          ];
        };

      in
      with pkgs;
      {
        formatter = nixpkgs.legacyPackages.x86_64-linux.nixpkgs-fmt;
        devShells.default = mkShell {
          buildInputs = [
            bashInteractive
            esp-idf-full
            qemu-esp32
          ];
        };
      }
    );
}