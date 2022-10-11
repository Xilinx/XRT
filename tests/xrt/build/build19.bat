@ECHO OFF

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
REM Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

set SCRIPTDIR=%~dp0
set BUILDDIR=%SCRIPTDIR%

if "%XILINX_XRT%" == "" (
  echo Please set XILINX_XRT [source xrt setup.bat]
  GOTO:EOF
)

IF DEFINED MSVC_PARALLEL_JOBS ( SET LOCAL_MSVC_PARALLEL_JOBS=%MSVC_PARALLEL_JOBS%) ELSE ( SET LOCAL_MSVC_PARALLEL_JOBS=3 )

set DEBUG=1
set RELEASE=1
set NOCMAKE=0

:parseArgs
  if [%1] == [] (
    goto argsParsed
  ) else (
  if [%1] == [-clean] (
    goto Clean
  ) else (
  if [%1] == [-help] (
    goto Help
  ) else (
  if [%1] == [-dbg] (
    set RELEASE=0
  ) else (
  if [%1] == [-opt] (
    set DEBUG=0
  ) else (
  if [%1] == [-nocmake] (
    set NOCMAKE=1
  ) else (
    echo Unknown option: %1
    goto Help
  ))))))

:argsParsed

if [%DEBUG%] == [1] (
   call :DebugBuild
   if errorlevel 1 (exit /B %errorlevel%)
)

if [%RELEASE%] == [1] (
   call :ReleaseBuild
   if errorlevel 1 (exit /B %errorlevel%)
)

goto :EOF

REM --------------------------------------------------------------------------
:Help
ECHO.
ECHO Usage: build22.bat [options]
ECHO.
ECHO [-help]                    - List this help
ECHO [-clean]                   - Remove build directories
ECHO [-dbg]                     - Creates a debug build
ECHO [-opt]                     - Creates a release build
ECHO [-nocmake]                 - Do not generate makefiles
GOTO:EOF

REM --------------------------------------------------------------------------
:Clean
PUSHD %BUILDDIR%
IF EXIST WDebug (
  ECHO Removing 'WDebug' directory...
  rmdir /S /Q WDebug
)
IF EXIST WRelease (
  ECHO Removing 'WRelease' directory...
  rmdir /S /Q WRelease
)
POPD
GOTO:EOF

set CMAKEFLAGS=-DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS

REM --------------------------------------------------------------------------
:DebugBuild
echo ====================== Windows Debug Build ============================
set CMAKEFLAGS=%CMAKEFLAGS% -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=%BUILDDIR%/WDebug/xilinx
ECHO CMAKEFLAGS=%CMAKEFLAGS%

MKDIR %BUILDDIR%\WDebug
PUSHD %BUILDDIR%\WDebug

if [%NOCMAKE%] == [0] (
   cmake -G "Visual Studio 16 2019" %CMAKEFLAGS% ../..
   IF errorlevel 1 (POPD & exit /B %errorlevel%)
)

cmake --build . --verbose --config Debug --target install
IF errorlevel 1 (POPD & exit /B %errorlevel%)

POPD
GOTO:EOF

REM --------------------------------------------------------------------------
:ReleaseBuild
ECHO ====================== Windows Release Build ============================
set CMAKEFLAGS=%CMAKEFLAGS% -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%BUILDDIR%/WRelease/xilinx

MKDIR %BUILDDIR%\WRelease
PUSHD %BUILDDIR%\WRelease

if [%NOCMAKE%] == [0] (
   cmake -G "Visual Studio 16 2019" %CMAKEFLAGS% ../..
   IF errorlevel 1 (POPD & exit /B %errorlevel%)
)

cmake --build . --verbose --config Release --target install
IF errorlevel 1 (POPD & exit /B %errorlevel%)

POPD
GOTO:EOF
