# Builds the QEMU images a test declares in its <test>.nix: an attrset of
# NixOS modules, one image each. Called by pynet with the site config and
# package list the runner probed from the gluon image under test, so the
# gateway matches the mesh it joins.
#
#   nix-build tests/nix -I gluon=tests/nix --arg test <test>.nix \
#     --argstr site <image>.site.json --argstr packages <image>.packages -A gateway
{
  test,
  site,
  packages,
  nixpkgs ? <nixpkgs>,
}:
let
  lib = import (nixpkgs + "/lib");
  specialArgs = {
    site = builtins.fromJSON (builtins.readFile site);
    packages = lib.filter (s: s != "") (lib.splitString "\n" (builtins.readFile packages));
  };
  build =
    name: module:
    (import (nixpkgs + "/nixos/lib/eval-config.nix") {
      inherit specialArgs;
      modules = [
        ./gateway/base.nix
        module
        { networking.hostName = name; }
      ];
    }).config.system.build.images.qemu;
in
lib.mapAttrs build (import test)
