@ECHO OFF

IF "%1" == "clean" (
  goto Clean
)   

IF "%1" == "-clean" (
  goto Clean
) 

if "%1" == "-help" (
  goto Help
)

if "%1" == "-debug" (
  goto DebugBuild
)

if "%1" == "-release" (
  goto ReleaseBuild
)

if "%1" == "-all" (
  call:DebugBuild
  call:ReleaseBuild
  goto:EOF
)


if "%1" == "" (
  call:DebugBuild
  call:ReleaseBuild
  GOTO:EOF
)

ECHO Unknown option: %1
GOTO Help


REM --------------------------------------------------------------------------
:Help
echo.
echo Usage: build.bat [options]
echo.
echo [-help]                    List this help
echo [clean^|-clean]             Remove build directories
GOTO:EOF

REM --------------------------------------------------------------------------
:Clean
if exist Debug (
  echo Removing 'Debug' directory...
  rmdir /S /Q Debug
)
if exist Release (
  echo Removing 'Release' directory...
  rmdir /S /Q Release
)
GOTO:EOF


REM --------------------------------------------------------------------------
:DebugBuild
echo ====================== Windows Debug Build ============================
mkdir Debug
cd Debug
cmake -G "Visual Studio 15 2017 Win64" -DBOOST_ROOT=C:\XRT\libs\boost -DBOOST_LIBRARYDIR=C:\XRT\libs\boost  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..\..\src
cmake --build . --verbose --config Debug
cd ..
GOTO:EOF

REM --------------------------------------------------------------------------
:ReleaseBuild
echo ====================== Windows Release Build ============================
mkdir Release
cd Release
cmake -G "Visual Studio 15 2017 Win64" -DBOOST_ROOT=C:\XRT\libs\boost -DBOOST_LIBRARYDIR=C:\XRT\libs\boost  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..\..\src
cmake --build . --verbose --config Release
cd ..
GOTO:EOF
