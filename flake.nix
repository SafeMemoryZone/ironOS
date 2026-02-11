{
  description = "Flake for developing ironOS";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    # TODO: support more platforms

    let
      system = "aarch64-darwin";

      pkgs = import nixpkgs { inherit system; };
      crossPkgs = pkgs.pkgsCross.x86_64-embedded;

      # NOTE: The nixpkgs limine package excludes x86 binaries when installed on aarch64-darwin
      # We fetch the full binary release from GitHub to get the BIOS files needed for the ISO

      # Build limine

      limineVersion = "10.6.6";

      limine-src = pkgs.fetchFromGitHub {
        owner = "limine-bootloader";
        repo = "limine";
        rev = "v${limineVersion}-binary";
        sha256 = "sha256-ulDELUBmJ2qgjsPE1KUfucmf99DxMLT0ceAEAvdKzFY=";
      };

      # Build the CLI tool from the same source as the bootloader binaries
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
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = [
            # C cross-compiler
            crossPkgs.buildPackages.gcc

            # Bootloader CLI tool
            limine-cli

            # Used to package the OS into an ISO file
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
    };
}
