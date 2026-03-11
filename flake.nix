{
  inputs = {
    nixpkgs = {
      url = "github:nixos/nixpkgs/nixos-unstable";
    };
    flake-utils = {
      url = "github:numtide/flake-utils";
    };
  };
  outputs = { nixpkgs, flake-utils, ...  }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs { inherit system; };
      riscv32-pkgs = pkgs.pkgsCross.riscv32-embedded.buildPackages;
      riscv64-pkgs = pkgs.pkgsCross.riscv64-embedded.buildPackages;

    in rec {
      devShell = pkgs.mkShell {
        buildInputs = with pkgs; [
          riscv32-pkgs.gcc
          riscv64-pkgs.gcc
          autoconf
          gnumake
          gcc
        ];
        shellHook = ''
          source set_env.sh
          # alias harness-dev="cd $PROJ_DIR/harness && cargo build && cd .. && ./harness/target/debug/harness"
          # alias harness="cd $PROJ_DIR && ./harness/target/debug/harness"
        '';
      };
    }
  );
}
