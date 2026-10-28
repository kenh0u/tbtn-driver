{
  description = "Panasonic TOUGHPAD/TOUGHBOOK ACPI TBTN Tablet Button Driver";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: {
        default = self.packages.${pkgs.system}.tbtn-driver;
        tbtn-driver = pkgs.linuxPackages.callPackage ./package.nix { };
      });

      nixosModules = {
        default = { config, pkgs, ... }: {
          boot.extraModulePackages = [
            (config.boot.kernelPackages.callPackage ./package.nix { })
          ];
          boot.kernelModules = [ "tbtn_driver" ];
        };
      };
    };
}
