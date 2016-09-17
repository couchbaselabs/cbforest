@echo off
setlocal ENABLEEXTENSIONS

IF NOT DEFINED VS140COMNTOOLS (
    echo You must have Visual Studio 2015 installed
    exit /b 1
)

pushd "%~dp0"

mkdir ..\prebuilt\x86
mkdir ..\prebuilt\x64

"%VS140COMNTOOLS%..\Ide\devenv.com" ..\..\CBForest.VS2015\CBForest.VS2015.sln /Clean "Release|x86" /Project ..\..\CBForest.VS2015\CBForest.VS2015.vcxproj
"%VS140COMNTOOLS%..\Ide\devenv.com" ..\..\CBForest.VS2015\CBForest.VS2015.sln /Clean "Release|x64" /Project ..\..\CBForest.VS2015\CBForest.VS2015.vcxproj
"%VS140COMNTOOLS%..\Ide\devenv.com" ..\..\CBForest.VS2015\CBForest.VS2015.sln /Build "Release|x86" /Project ..\..\CBForest.VS2015\CBForest.VS2015.vcxproj
"%VS140COMNTOOLS%..\Ide\devenv.com" ..\..\CBForest.VS2015\CBForest.VS2015.sln /Build "Release|x64" /Project ..\..\CBForest.VS2015\CBForest.VS2015.vcxproj

copy ..\..\CBForest.VS2015\Win32\Release\CBForest-Interop.dll ..\prebuilt\x86
copy ..\..\CBForest.VS2015\x64\Release\CBForest-Interop.dll ..\prebuilt\x64
popd