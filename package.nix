{ stdenv, lib, kernel, kmod }:

stdenv.mkDerivation {
  pname = "tbtn-driver";
  version = "0.1.0";

  src = ./.;

  hardeningDisable = [ "pic" "format" ];

  nativeBuildInputs = kernel.moduleBuildDependencies;

  makeFlags = [
    "KERNELRELEASE=${kernel.modDirVersion}"
    "KDIR=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
    "INSTALL_MOD_PATH=$(out)"
  ];

  buildFlags = [ "all" ];
  installTargets = [ "install" ];

  meta = with lib; {
    description = "Panasonic TOUGHPAD/TOUGHBOOK ACPI TBTN Tablet Button Driver";
    license = licenses.gpl2Only;
    platforms = platforms.linux;
  };
}
