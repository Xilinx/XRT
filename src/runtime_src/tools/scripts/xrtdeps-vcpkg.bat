@ECHO OFF

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

setlocal
set "DO_CLEAN=0"
set "SCRIPTDIR=%~dp0"
set "SCRIPTDIR=%SCRIPTDIR:~0,-1%"
set "SRC_ROOT=%SCRIPTDIR%\..\..\..\.."
set "EXTRA_PORTS="

for %%I in ("%SRC_ROOT%") do set "SRC_ROOT=%%~fI"

if "%XRT_EXT_ROOT%"==""  set "XRT_EXT_ROOT=%SRC_ROOT%\build\ext.vcpkg"
if "%VCPKG_TRIPLET%"=="" set "VCPKG_TRIPLET=x64-windows"

REM --------------------------------------------------------------------------
:parseArgs
if "%~1"=="" goto argsParsed

if /I "%~1"=="-help"    goto help
if /I "%~1"=="-extroot" goto parseExtRoot
if /I "%~1"=="-triplet" goto parseTriplet
if /I "%~1"=="-port"    goto parsePort
if /I "%~1"=="-clean"   ( set "DO_CLEAN=1" & shift & goto parseArgs )

echo Unknown option: %1
goto help

REM --------------------------------------------------------------------------
:parseExtRoot
shift

if "%~1"=="" (
  echo ERROR: -extroot requires a path argument
  exit /B 2
)

set "XRT_EXT_ROOT=%~1"
shift
goto parseArgs

REM --------------------------------------------------------------------------
:parseTriplet
shift

if "%~1"=="" (
  echo ERROR: -triplet requires an argument
  exit /B 2
)

set "VCPKG_TRIPLET=%~1"
shift
goto parseArgs

REM --------------------------------------------------------------------------
:parsePort
shift

if "%~1"=="" (
  echo ERROR: -port requires a port name
  exit /B 2
)

set "EXTRA_PORTS=%EXTRA_PORTS% %~1"
shift
goto parseArgs

REM --------------------------------------------------------------------------
:argsParsed
if "%DO_CLEAN%"=="1" goto clean

set "EXT_DIR=%XRT_EXT_ROOT%\vcpkg_installed\%VCPKG_TRIPLET%"

mkdir "%XRT_EXT_ROOT%" >NUL 2>NUL

pushd "%XRT_EXT_ROOT%" || exit /B
del /F /Q vcpkg.json vcpkg-configuration.json vcpkg-lock.json >NUL 2>NUL
vcpkg new --application || exit /B
vcpkg add port boost-filesystem boost-program-options boost-property-tree boost-format boost-headers boost-interprocess boost-uuid boost-asio boost-process opencl protobuf %EXTRA_PORTS% || exit /B
vcpkg install --triplet %VCPKG_TRIPLET% --clean-after-build || exit /B
popd

REM Needed for pyxrt.
python -m pip install pybind11 || exit /B

echo.
echo Dependency prefix ready:
echo   EXT_DIR=%EXT_DIR%
echo.
echo Build example:
echo   build\build26.bat -ext "%EXT_DIR%" -stage_ext -opt -npu -install

exit /B 0

REM --------------------------------------------------------------------------
:help
echo.
echo Usage: xrtdeps-vcpkg.bat [options]
echo.
echo [-help]              - List this help
echo [-extroot ^<path^>]    - Root directory for vcpkg deps (default: ^<repo^>\build\ext.vcpkg)
echo [-clean]             - Remove the extroot directory (default: ^<repo^>\build\ext.vcpkg)
echo [-triplet ^<triplet^>] - vcpkg triplet (default: x64-windows)
echo [-port ^<port^>]       - Extra vcpkg port to install (may be repeated)
echo.
echo Note: Uses first vcpkg on PATH.

exit /B 2

REM --------------------------------------------------------------------------
:clean
if not exist "%XRT_EXT_ROOT%\vcpkg.json" (
  echo ERROR: Refusing to delete "%XRT_EXT_ROOT%" because vcpkg.json was not found.
  exit /B 2
)
rmdir /S /Q "%XRT_EXT_ROOT%" || exit /B 2

exit /B 0
