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

if "%1" == "" (
  goto DebugBuild
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
GOTO:EOF


REM --------------------------------------------------------------------------
:DebugBuild
echo ====================== Windows Debug Build ============================
mkdir Debug
cd Debug
cmake -G "Visual Studio 15 2017" -DBOOST_ROOT=C:\XRT\libs\boost -DBOOST_LIBRARYDIR=C:\XRT\libs\boost\lib -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..\..\src
msbuild ALL_BUILD.vcxproj
cd ..
GOTO:EOF
