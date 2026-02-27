@ECHO OFF

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

setlocal
set "DO_CLEAN=0"
set "SCRIPTDIR=%~dp0"
set "SCRIPTDIR=%SCRIPTDIR:~0,-1%"
set "SRC_ROOT=%SCRIPTDIR%\..\..\..\.."
set "EXTRA_PORTS="
set "CREATE_PACKAGE=0"
REM Pin vcpkg registry baseline for determinism.
REM Dependency versioning can be done here.
REM Current vcpkg release: 2026.01.16
SET "VCPKG_BASELINE_SHA=66c0373dc7fca549e5803087b9487edfe3aca0a1"

for %%I in ("%SRC_ROOT%") do set "SRC_ROOT=%%~fI"

if "%XRT_EXT_ROOT%"==""  set "XRT_EXT_ROOT=%SRC_ROOT%\build\ext.vcpkg"
if "%VCPKG_TRIPLET%"=="" set "VCPKG_TRIPLET=x64-windows-static"

REM --------------------------------------------------------------------------
:parseArgs
if "%~1"=="" goto argsParsed

if /I "%~1"=="-help"     goto help
if /I "%~1"=="-extroot"  goto parseExtRoot
if /I "%~1"=="-triplet"  goto parseTriplet
if /I "%~1"=="-port"     goto parsePort
if /I "%~1"=="-baseline" goto parseBaseline
if /I "%~1"=="-pkg"      ( set "CREATE_PACKAGE=1" & shift & goto parseArgs )
if /I "%~1"=="-clean"    ( set "DO_CLEAN=1" & shift & goto parseArgs )

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
:parseBaseline
shift

set "VCPKG_BASELINE_SHA="
set "BASELINE_ARG=%~1"
if "%BASELINE_ARG%"=="" goto parseArgs
if "%BASELINE_ARG:~0,1%"=="-" goto parseArgs

set "VCPKG_BASELINE_SHA=%BASELINE_ARG%"
shift
goto parseArgs

REM --------------------------------------------------------------------------
:argsParsed
if "%DO_CLEAN%"=="1" goto clean

set "EXT_DIR=%XRT_EXT_ROOT%\vcpkg_installed\%VCPKG_TRIPLET%"
set "PKG_OUT_DIR=%SRC_ROOT%\build\pkg"
set "PKG_BASELINE_ID=unpinned"
if not "%VCPKG_BASELINE_SHA%"=="" set "PKG_BASELINE_ID=%VCPKG_BASELINE_SHA:~0,7%"

mkdir "%XRT_EXT_ROOT%" >NUL 2>NUL

pushd "%XRT_EXT_ROOT%" || exit /B
del /F /Q vcpkg.json vcpkg-configuration.json vcpkg-lock.json >NUL 2>NUL
vcpkg new --application || exit /B

if "%VCPKG_BASELINE_SHA%"=="" goto baselineDone

python -c "import json, pathlib; p=pathlib.Path('vcpkg-configuration.json'); d=json.loads(p.read_text(encoding='utf-8')); dr=d.get('default-registry') or {}; dr['baseline']=r'%VCPKG_BASELINE_SHA%'; d['default-registry']=dr; p.write_text(json.dumps(d, indent=2) + chr(10), encoding='utf-8')" || exit /B

:baselineDone

copy /Y "%SRC_ROOT%\src\vcpkg.json" vcpkg.json >NUL || exit /B
if not "%EXTRA_PORTS%"=="" ( vcpkg add port %EXTRA_PORTS% || exit /B )
vcpkg install --triplet %VCPKG_TRIPLET% --clean-after-build || exit /B
call :writeBaselineMarker || exit /B
popd

REM Needed for pyxrt.
python -m pip install pybind11 || exit /B

if "%CREATE_PACKAGE%"=="1" call :packageDeps || exit /B

echo.
echo Dependency prefix ready:
echo   EXT_DIR=%EXT_DIR%
if "%CREATE_PACKAGE%"=="1" echo   PACKAGE=%PKG_OUT_DIR%\ext.vcpkg.%PKG_BASELINE_ID%.%VCPKG_TRIPLET%.zip
echo.
set "BUILD_ARCH_ARG="
if /I "%VCPKG_TRIPLET:~0,6%"=="arm64-" set "BUILD_ARCH_ARG= -arm64"
set "BUILD_DYNAMIC_ARG="
if "%VCPKG_TRIPLET:-static=%"=="%VCPKG_TRIPLET%" set "BUILD_DYNAMIC_ARG= -dynamic"
echo Build example:
echo   build\build26.bat -ext "%EXT_DIR%"%BUILD_ARCH_ARG%%BUILD_DYNAMIC_ARG% -opt -npu -install

exit /B 0

REM --------------------------------------------------------------------------
:help
echo.
echo Usage: xrtdeps-vcpkg.bat [options]
echo.
echo [-help]              - List this help
echo [-extroot ^<path^>]    - Root directory for vcpkg deps (default: ^<repo^>\build\ext.vcpkg)
echo [-clean]             - Remove the extroot directory (default: ^<repo^>\build\ext.vcpkg)
echo [-triplet ^<triplet^>] - vcpkg triplet (default: x64-windows-static)
echo [-baseline [^<sha^>]]  - Use vcpkg default baseline (omit ^<sha^>) or pin to user-specified ^<sha^>
echo [-port ^<port^>]       - Extra vcpkg port to install (may be repeated)
echo [-pkg]               - Create build\pkg\ext.vcpkg.^<baseline^>.^<triplet^>.zip for publishing
echo.
echo Note: Uses first vcpkg on PATH.

exit /B 2

REM --------------------------------------------------------------------------
:writeBaselineMarker
set "BASELINE_MARKER=%EXT_DIR%\.vcpkg-baseline"
set "BASELINE_MARKER_VALUE=%VCPKG_BASELINE_SHA%"
if "%BASELINE_MARKER_VALUE%"=="" set "BASELINE_MARKER_VALUE=UNPINNED"
> "%BASELINE_MARKER%" <NUL set /P "=%BASELINE_MARKER_VALUE%"
exit /B 0

REM --------------------------------------------------------------------------
:packageDeps
set "PKG_NAME=ext.vcpkg.%PKG_BASELINE_ID%.%VCPKG_TRIPLET%.zip"
set "PKG_PATH=%PKG_OUT_DIR%\%PKG_NAME%"
python -c "import os, pathlib, zipfile; root=pathlib.Path(os.environ['XRT_EXT_ROOT']); src=root/'vcpkg_installed'/os.environ['VCPKG_TRIPLET']; out=pathlib.Path(os.environ['PKG_PATH']); out.parent.mkdir(parents=True, exist_ok=True); z=zipfile.ZipFile(out, 'w', compression=zipfile.ZIP_DEFLATED); [z.write(p, p.relative_to(root)) for p in src.rglob('*') if p.is_file()]; z.close()" || exit /B
exit /B 0

REM --------------------------------------------------------------------------
:clean
if not exist "%XRT_EXT_ROOT%\vcpkg.json" (
  echo ERROR: Refusing to delete "%XRT_EXT_ROOT%" because vcpkg.json was not found.
  exit /B 2
)
rmdir /S /Q "%XRT_EXT_ROOT%" || exit /B 2

exit /B 0
