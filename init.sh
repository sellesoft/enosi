#/bin/bash

echo making sure submodules are initialized...

git submodule update --init --recursive ||
  { echo initializing submodules failed!; exit 1; }

if ! stat --terse bin/luajit; then
  echo building luajit...

  mkdir luajit/{lib,include,obj}

  cd luajit/src

  make clean
  make -j

  cd ../../

	cp luajit/src/src/libluajit.a luajit/lib
	cp luajit/src/src/lua.h     \
		 luajit/src/src/lualib.h  \
		 luajit/src/src/lauxlib.h \
		 luajit/src/src/luajit.h  \
		 luajit/src/src/luaconf.h \
		 luajit/include

  cp luajit/src/src/luajit bin
else
  echo luajit already exists!
fi

# maybe just use actual tmp on linux
mkdir tmp
echo -e "\n\n"

bin/luajit init.lua linux

rm -rfd tmp

