{
  description = "List symbolic links encountered in path traversal.";

  inputs.nixpkgs.url = "github:flox/nixpkgs";

  outputs = inputs: {
    recipes.tracelinks = import ./pkgs/tracelinks;
    packages = builtins.mapAttrs (system: pkgs: {
      default = pkgs.callPackage inputs.self.recipes.tracelinks { };
    }) inputs.nixpkgs.legacyPackages;
  };
}
