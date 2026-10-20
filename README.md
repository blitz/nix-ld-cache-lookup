# Nix Dynamic Loader Cache Lookup via `LD_AUDIT`

This is a proof-of-concept of using an `LD_AUDIT` plugin to parse the
[dynamic linker cache](https://github.com/NixOS/nixpkgs/pull/535735)
that [`patchelf`](https://github.com/nixos/patchelf) can create.

## Unfinished Parts

- Make plugin independent of libc (`-ffreestanding`)
  - This is the reason you see crashes on startup when mixing glibc versions.
- More defensive programming (see TODOs)
- Handle `?<dir>` cache entries

## How to Try

```console
$ nix develop
 ... this will compile a while ...

$ as --version
... you will see debug output from the LD_AUDIT plugin ...
```
