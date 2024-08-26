{
  description = "List symbolic links encountered in path traversal.";

  inputs = {
    nixpkgs.url = "github:flox/nixpkgs";
    utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { nixpkgs, ... }@inputs:
    inputs.utils.lib.eachSystem
      [
        "aarch64-darwin"
        "aarch64-linux"
        "x86_64-darwin"
        "x86_64-linux"
      ]
      (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          packages.default = pkgs.callPackage ./pkgs/tracelinks/default.nix { };
        }
      );
}
