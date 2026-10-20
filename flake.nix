{
  description = "Nix Shared Library Cache Lookup PoC";

  inputs = {
    # Chosen to have the same glibc version as the
    # glibc-resolution-cache below. It also has a new
    # patchelf. Unfortunately, this means you will compile a while.
    #
    # We could use the glibc-resolution-cache branch here, but some
    # derivations don't evaluate (e.g. pkgs.hello ...).
    nixpkgs.url = "github:blitz/nixpkgs/29607508b9c3a89a8001dda3c3597136fd31dbeb";

    # This is Domen's PR branch minus the actual glibc patch. So
    # everything from this branch has a shared library cache baked in
    # via patchelf.
    # 
    # Here is Domen's branch for reference:
    # https://github.com/NixOS/nixpkgs/pull/535735
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
            # Some binaries with the dynamic loader cache built in.
            inputs.nixpkgs-with-cache.legacyPackages.${system}.binutils
          ];

          env.LD_AUDIT = "${inputs.self.packages.${system}.default}/lib/libnix_cache_lookup.so";
        };
      })
      inputs.nixpkgs.legacyPackages;
  };
}
