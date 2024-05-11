{
	description = "Flake providing a development environment for lpp.";
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
							llvmPackages = llvmPackages_18;
							enableLibcxx = false;
						})
						(callPackage ./impure-clang.nix {})
						pkgs.stdenv.cc.cc.lib
						gnumake
						gdb
						bear
						linuxKernel.packages.linux_zen.perf
						valgrind
					];

				env =
				{
					LD_LIBRARY_PATH = "${pkgs.stdenv.cc.cc.lib}/lib";
				};
			};
		});
}
