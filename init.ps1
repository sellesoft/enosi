# This bit of magic uses the vswhere.exe distributed with the repo (in misc/)
# to automate finding MSVC stuff and exporting the environment to this script
# because Windows is really cool!
# This was retrieved from:
# https://github.com/microsoft/vswhere/wiki/Start-Developer-Command-Prompt
$installationPath = misc/vswhere.exe -prerelease -latest -property installationPath
if ($installationPath -and (test-path "$installationPath\Common7\Tools\vsdevcmd.bat")) {
  & "${env:COMSPEC}" /s /c "`"$installationPath\Common7\Tools\vsdevcmd.bat`" -arch=x64 -no_logo && set" | foreach-object {
    $name, $value = $_ -split '=', 2
    set-content env:\"$name" $value
  }
}

echo "making sure luajit submodule is initialized..."

git submodule update --init luajit
if ($LASTEXITCODE -ne 0) 
{
	echo "initializing luajit submodule failed!"
	Exit 1
}

echo "building luajit..."

cd luajit/src/src

cmd.exe /c "msvcbuild.bat"

cd ../../../

md bin -ea 0 | Out-Null

md luajit/build/lib -ea 0 | Out-Null
md luajit/build/include/luajit -ea 0 | Out-Null
md luajit/build/obj -ea 0 | Out-Null

# Put libs, headers, and binaries in standard locations.
cp luajit/src/src/luajit.lib -Destination luajit/build/lib
cp luajit/src/src/lua51.lib -Destination luajit/build/lib

cp luajit/src/src/lua.h     -Destination luajit/build/include/luajit/lua.h
cp luajit/src/src/lualib.h  -Destination luajit/build/include/luajit/lualib.h
cp luajit/src/src/lauxlib.h -Destination luajit/build/include/luajit/lauxlib.h
cp luajit/src/src/luajit.h  -Destination luajit/build/include/luajit/luajit.h
cp luajit/src/src/luaconf.h -Destination luajit/build/include/luajit/luaconf.h

# 'Install' luajit to our standard bin folder.
cp luajit/src/src/luajit.exe -Destination bin
cp luajit/src/src/lua51.dll -Destination bin

if (!(Test-Path bin/client.exe))
{
  echo "retrieving asset server client executable..."
  curl 15.204.247.107:3000/get_client?platform=windows -o bin/client.exe
}

if (!(Test-Path llvm/llvm_build))
{
  echo "downloading llvm from asset server..."
  echo "(this might take a minute...)"
  & "$PSScriptRoot\web\download_file.ps1" llvm.zip llvm.zip
  echo "unzipping llvm..."
  tar -xf llvm.zip
  rm llvm.zip
}

md tmp
echo "\n\n"

# For requiring iro lua modules.
$env:LUA_PATH += ";$PWD/iro/src/lua/?.lua"
# For requiring modules relative to enosi's root (eg. the build stuff).
$env:LUA_PATH += ";$PWD/?.lua"
# So luajit can require its lua modules.
$env:LUA_PATH += ";$PWD/luajit/src/src/?.lua"

# For using whatever is in bin.
$env:PATH += ";$PWD/bin"
# For using our locally built clang.
$env:PATH += ";$PWD/llvm/llvm_build/Release/Release/bin"

bin/luajit init.lua windows




