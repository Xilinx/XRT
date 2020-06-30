@ECHO OFF

set SRCDIR=%~dp0
set BUILDDIR=%SRCDIR%\build
set CMAKEDIR=%BUILDDIR%\cmake\windows

if "%XILINX_XRT%" == "" (
  echo Please set XILINX_XRT [source xrt setup.bat]
  GOTO:EOF
)

if "%1" == "clean" (
  goto Clean
)

if "%1" == "" (
  call:Build
  GOTO:EOF
)

ECHO Unknown option: %1
GOTO Help

:Clean
if exist %BUILDDIR% (
  echo Removing '%BUILDDIR%' directory...
  rmdir /S /Q %BUILDDIR%
)
GOTO:EOF


REM --------------------------------------------------------------------------
:Build
set HERE=%cd%
mkdir %CMAKEDIR%
cd %CMAKEDIR%

echo ====================== Windows Debug Build ============================
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Debug -DXILINX_XRT=%XILINX_XRT% %SRCDIR%
cmake --build . --verbose --config Debug --target install

cd %HERE%

GOTO:EOF
