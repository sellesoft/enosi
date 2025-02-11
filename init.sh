#!/bin/bash

root=$(pwd)

echo making sure luajit submodule is initialized...

git submodule update --init luajit ||
  { echo initializing luajit failed!; exit 1; }

mkdir bin

if ! test -f bin/luajit;
then
  echo building luajit...

  cd luajit/src

  make clean
  make -j

  cd ../../

  mkdir -p luajit/build/{lib,include,obj}
  mkdir luajit/build/include/luajit

  cp -v luajit/src/src/libluajit.a luajit/build/lib
  cp -v \
     luajit/src/src/lua.h     \
     luajit/src/src/lualib.h  \
     luajit/src/src/lauxlib.h \
     luajit/src/src/luajit.h  \
     luajit/src/src/luaconf.h \
     luajit/build/include/luajit

  cp luajit/src/src/luajit bin/luajit
fi

if ! test -f bin/client;
then
  echo retrieving asset server client executable...
  curl 15.204.247.107:3000/get_client?platform=linux -o bin/client ||
    { echo getting asset server client failed!; exit 1; }
fi

if ! test -d llvm/llvm_build;
then
  echo "downloading llvm from asset server..."
  echo "(this might take a minute...)"
  ./web/download_file.sh llvm.zip llvm.zip ||
    { echo "downloading llvm failed!"; exit 1; }
  echo "unzipping llvm..."
  unzip llvm.zip ||
    { echo "failed to unzip llvm!"; exit 1; }
  rm llvm.zip
fi

# maybe just use actual tmp on linux
mkdir tmp
echo -e "\n\n"

# For requiring iro lua modules
export LUA_PATH="$LUA_PATH;$(pwd)/iro/src/lua/?.lua"
# For requiring modules relative to enosi's root (eg. the build stuff)
export LUA_PATH="$LUA_PATH;$(pwd)/?.lua"
# So luajit can require its lua modules
export LUA_PATH="$LUA_PATH;$(pwd)/luajit/src/src/?.lua"

luajit/src/src/luajit init.lua linux

rm -rfd tmp


