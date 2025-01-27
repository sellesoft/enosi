# This bit of magic uses the vswhere.exe distributed with the repo (in misc/)
# to automate finding MSVC stuff and exporting the environment to this script
# because Windows is really cool!
# This was retrieved from:
# https://github.com/microsoft/vswhere/wiki/Start-Developer-Command-Prompt
$installationPath = misc/vswhere.exe -prerelease -latest -property installationPath
if ($installationPath -and (test-path "$installationPath\Common7\Tools\vsdevcmd.bat")) {
  & "${env:COMSPEC}" /s /c "`"$installationPath\Common7\Tools\vsdevcmd.bat`" -no_logo && set" | foreach-object {
    $name, $value = $_ -split '=', 2
    set-content env:\"$name" $value
  }
}

echo "making sure submodules are initialized..."

git submodule update --init --recursive
if ($LASTEXITCODE -ne 0) 
{
	echo "initializing submodules failed!"
	Exit 1
}

echo "building luajit..."

cd luajit/src/src

cmd.exe /c "msvcbuild.bat"

cd ../../../

md luajit/build/lib -ea 0 | Out-Null
md luajit/build/include -ea 0 | Out-Null
md luajit/build/obj -ea 0 | Out-Null

# Put libs, headers, and binaries in standard locations.
cp luajit/src/src/luajit.lib -Destination luajit/build/lib

cp luajit/src/src/lua.h     -Destination luajit/build/include/luajit
cp luajit/src/src/lualib.h  -Destination luajit/build/include/luajit
cp luajit/src/src/lauxlib.h -Destination luajit/build/include/luajit
cp luajit/src/src/luajit.h  -Destination luajit/build/include/luajit
cp luajit/src/src/luaconf.h -Destination luajit/build/include/luajit

# 'Install' luajit to our standard bin folder.
cp luajit/src/src/luajit.exe -Destination bin
cp luajit/src/src/lua51.dll -Destination bin

md tmp
echo "\n\n"

# For requiring iro lua modules.
$env:LUA_PATH += ";$PWD/iro/src/lua/?.lua"
# For requiring modules relative to enosi's root (eg. the build stuff).
$env:LUA_PATH += ";$PWD/?.lua"
# So luajit can require its lua modules.
$env:LUA_PATH += ";$PWD/luajit/src/src/?.lua"

bin/luajit init.lua windows




