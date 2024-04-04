# 
# This is a nix flake I use to create a dev environment for lake.
# This is not meant to be the standard way to build lake, its just a way 
# for me to quickly get a consistent env on whatever system im working on,
# when that system is x86_64 linux.
#

{
	description = "Flake providing a development environment for lake.";
	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
	inputs.flake-utils.url = "github:numtide/flake-utils";

	outputs = { nixpkgs, flake-utils, ... }:
		flake-utils.lib.eachDefaultSystem (system:
			let
				pkgs = nixpkgs.legacyPackages.${system};
			in
			{
				devShells.default = pkgs.mkShell {
				packages = with pkgs;
					[ 
						luajit
						(hiPrio clang-tools.override 
						{
							llvmPackages = llvmPackages_17;
							enableLibcxx = false;
						})
						llvmPackages_17.libcxxClang
						gnumake
						gdb
						bear
						linuxKernel.packages.linux_zen.perf
					];
			};
		});
}
