# Nix flake that should setup a complete environment for building all of these projects.
# This is not required, and can just serve as a guide for what env is needed to build
# everything.

{	
	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

	outputs = { nixpkgs, ... }:
		let
			pkgs = nixpkgs.legacyPackages.x86_64-linux;
      impure-clang = (pkgs.callPackage ./impure-clang.nix {});
      llvmlib = pkgs.llvmPackages_17.libcxxClang;

      shell = pkgs.mkShell
      {
				name = "enosienv";
				buildInputs = with pkgs;
				[
          tree-sitter
          nodejs
					impure-clang
					clang-tools
          llvmlib
          llvmlib.libc.libgcc
					llvmPackages_17.libllvm
					gnumake
					gdb
					bear
					linuxKernel.packages.linux_zen.perf
					explain
					acl
					valgrind
					rr
					mold
					lua-language-server
					luarocks
					nelua

          # notcurses
          # libunistring
          # libdeflate
          # doctest
          # ncurses

          pkg-config
          clangbuildanalyzer

					ccache

					# stuff needed to build llvm
					cmake
					ninja
					gcc
					lld
					stdenv.cc.cc.lib

          xorg.libX11
          xorg.libXrandr
          xorg.libXcursor

          pkg-config

          python312Packages.glad2
				];

				shellHook = 
        let
          # tool that sets up dynamic library paths
          libpath = with pkgs; lib.makeLibraryPath 
          [
            explain
            acl
            libunistring
            libdeflate
            ncurses
            stdenv.cc.cc
            xorg.libX11
            xorg.libXrandr
            xorg.libXcursor
          ];
          target = llvmlib.libc.libgcc.libgcc;
          attrs = pkgs.lib.concatMapStrings 
            (x: "echo "+x.fst+": \""+ (
              if builtins.typeOf(x.snd) == "set" then
                "<set>"
              else if builtins.typeOf(x.snd) == "lambda" then
               "<lambda>"
              else
                toString(x.snd)) + "\"\n")
            (pkgs.lib.zipLists 
              (pkgs.lib.attrNames(target)) 
              (pkgs.lib.attrValues(target)));
        in 
        '' 
          export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${libpath} 
          unset LUA_PATH
          unset LUA_CPATH
          export NIX_LDFLAGS="$NIX_LDFLAGS -L${llvmlib.libc}/lib"
          ${attrs}
        '';
			};
		in
		{
			devShells.x86_64-linux.default = shell;
		};
}
