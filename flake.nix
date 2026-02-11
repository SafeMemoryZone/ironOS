{
  description = "Flake for developing ironOS";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    # Add flake-utils to handle multiple systems
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        crossPkgs = pkgs.pkgsCross.x86_64-embedded;

        # To ensure compatibility on all platforms, we fetch the Limine binaries 
        # that also include the source code and compile them locally.
        limineVersion = "10.6.6";

        limine-src = pkgs.fetchFromGitHub {
          owner = "limine-bootloader";
          repo = "limine";
          rev = "v${limineVersion}-binary";
          sha256 = "sha256-ulDELUBmJ2qgjsPE1KUfucmf99DxMLT0ceAEAvdKzFY=";
        };

        limine-cli = pkgs.stdenv.mkDerivation {
          pname = "limine-cli";
          version = limineVersion;
          src = limine-src;
          installPhase = ''
            mkdir -p $out/bin
            cp limine $out/bin/
          '';
        };
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = [
            # GCC cross-compiler (includes the linker)
            crossPkgs.buildPackages.gcc
            
            # Limine bootloader
            limine-cli

            # Utility used to create ISO files
            pkgs.xorriso
            
            # Emulator used to test ironOS
            pkgs.qemu
          ];

          shellHook = ''
            export TOOLCHAIN_PREFIX="x86_64-elf-"
            export LIMINE="limine"
            export LIMINE_DATADIR="${limine-src}"
          '';
        };
      }
    );
}
