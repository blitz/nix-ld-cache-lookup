{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";
    patchelf = {
      url = "github:nixos/patchelf";
      inputs.nixpkgs.follows = "nixpkgs";
    };

    nixpkgs-with-cache.url = "github:blitz/nixpkgs/glibc-resolution-cache";
  };

  outputs = inputs: {
    packages = builtins.mapAttrs
      (system: pkgs: {
        default = pkgs.callPackage ./package.nix { };
      })
      inputs.nixpkgs.legacyPackages;

    devShells = builtins.mapAttrs
      (system: pkgs: {
        default = pkgs.mkShell {
          packages = [
            inputs.self.packages.${system}.default
            inputs.patchelf.packages.${system}.default

            # Binaries with the dynamic loader cache.
            inputs.nixpkgs-with-cache.legacyPackages.${system}.binutils
          ];

          env.LD_AUDIT = "${inputs.self.packages.${system}.default}/lib/libnix_cache_lookup.so";
        };
      })
      inputs.nixpkgs.legacyPackages;
  };
}
