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
        # nix develop
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = [
            # GCC cross-compiler (includes the linker)
            crossPkgs.buildPackages.gcc

            # Generating compile commands for LSPs
            pkgs.compiledb
            
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

        # nix build
        packages.default = pkgs.stdenv.mkDerivation {
            pname = "ironOS";
            version = "0.0.0";
            src = ./.;

            nativeBuildInputs = [
              crossPkgs.buildPackages.gcc
              limine-cli
              pkgs.xorriso
            ];

            # Replicate the shell variables
            TOOLCHAIN_PREFIX = "x86_64-elf-";
            LIMINE = "limine";
            LIMINE_DATADIR = "${limine-src}";

            # Environment vars needed by Makefile
            BUILD_DIR = "build";
            OUT = "ironOS";

            # Default buildPhase is make
            
            # We need to tell Nix where to put the output ISO
            installPhase = ''
              mkdir -p $out
              cp build/ironOS.iso $out/
            '';
        };

        # nix run
        apps.default = {
          type = "app";
          program = "${pkgs.writeShellScript "run-ironOS" ''
            ${pkgs.qemu}/bin/qemu-system-x86_64 -cdrom ${self.packages.${system}.default}/ironOS.iso -serial file:kernel.log
          ''}";
        };
      }
    );
}
