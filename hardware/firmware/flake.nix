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
          config.allowUnfree = true;
        };

      in
      with pkgs;
      {
        formatter = nixpkgs.legacyPackages.x86_64-linux.nixpkgs-fmt;
        
        devShells.default = mkShell {
          buildInputs = [
            # Shell
            bashInteractive

            # Full ESP-IDF development environment
            esp-idf-full

            (vscode-with-extensions.override {
              vscodeExtensions = with vscode-extensions; [
              tuttieee.emacs-mcx
              bbenoist.nix
              ms-python.python
	            ms-vscode.cpptools
        	    ms-vscode.cpptools-extension-pack
              ms-vscode.cmake-tools
              ] ++ pkgs.vscode-utils.extensionsFromVscodeMarketplace [
                {
                  name = "esp-idf-extension";
                  publisher = "espressif";
                  version = "1.10.0";
                  sha256 = "tTh25AHg8msKtrNXfn39aHT8bqWy84lfd+Aal6ip4Oc=";
                }];
            })
            
            # QEMU for ESP32 
            (qemu-esp32.override {
              sdlSupport = true; # Needed for screen output
              gtkSupport = true; # Alternative to SDL
            })
          ];
        };
      }
    );
}
