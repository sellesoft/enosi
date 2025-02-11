#!/bin/bash
# Must be run from enosi's root.

mkdir -p llvm/llvm_build/Release

cd llvm/llvm_build/Release

cmake ../../src/llvm "Unix Makefiles" \
  -DLLVM_CCACHE_BUILD=ON \
  -DLLVM_OPTIMIZED_TABLEGEN=ON \
  -DLLVM_ENABLE_PROJECTS="clang;lld;compiler-rt" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ZSTD=OFF

make -j6
