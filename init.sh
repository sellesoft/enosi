#/bin/bash

root=$(pwd)

echo making sure submodules are initialized...

git submodule update --init --recursive ||
  { echo initializing submodules failed!; exit 1; }

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


