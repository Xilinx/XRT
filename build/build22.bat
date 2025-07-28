@ECHO OFF

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2022 Xilinx, Inc.  All rights reserved.
REM Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
setlocal enabledelayedexpansion
set SCRIPTDIR=%~dp0
set SCRIPTDIR=%SCRIPTDIR:~0,-1%
set BUILDDIR=%SCRIPTDIR%

set DEBUG=1
set RELEASE=1
set EXT_DIR=C:/Xilinx/XRT/ext.new
set CREATE_PACKAGE=0
set CREATE_SDK=0
set CMAKEFLAGS=
set NOCMAKE=0
set NOCTEST=0
set GENERATOR="Visual Studio 17 2022"
set PLATFORM=WBuild

IF DEFINED MSVC_PARALLEL_JOBS ( SET LOCAL_MSVC_PARALLEL_JOBS=%MSVC_PARALLEL_JOBS%) ELSE ( SET LOCAL_MSVC_PARALLEL_JOBS=3 )

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
  if [%1] == [-ext] (
    shift
    set EXT_DIR=%2
  ) else (
  if [%1] == [-opt] (
    set DEBUG=0
  ) else (
  if [%1] == [-npu] (
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_NPU=1
  ) else (
  if [%1] == [-noabi] (
    set CMAKEFLAGS=%CMAKEFLAGS% -DDISABLE_ABI_CHECK=1
  ) else (
  if [%1] == [-sdk] (
    set CREATE_SDK=1
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_NPU=1
  ) else (
  if [%1] == [-pkg] (
    set CREATE_PACKAGE=1
  ) else (
  if [%1] == [-nocmake] (
    set NOCMAKE=1
  ) else (
  if [%1] == [-hip] (
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_ENABLE_HIP=ON
  ) else (
    echo Unknown option: %1
    goto Help
  ))))))))))))
  shift
  goto parseArgs

:argsParsed

REM Configure CMake
if [%NOCMAKE%] == [0] (
   echo Configuring CMake project

   set CMAKEFLAGS=%CMAKEFLAGS%^
   -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS%^
   -DKHRONOS=%EXT_DIR%^
   -DBOOST_ROOT=%EXT_DIR%^
   -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

   echo cmake -B %BUILDDIR%\%PLATFORM% -G %GENERATOR% !CMAKEFLAGS! %BUILDDIR%\..\src
   cmake -B %BUILDDIR%\%PLATFORM% -G %GENERATOR% !CMAKEFLAGS! %BUILDDIR%\..\src
   IF errorlevel 1 (exit /B %errorlevel%)
)

if [%DEBUG%] == [1] (
   echo cmake --build %BUILDDIR%\%PLATFORM% --config Debug --verbose
   cmake --build %BUILDDIR%\%PLATFORM% --config Debug --verbose
   if errorlevel 1 (exit /B %errorlevel%)

   echo cmake --install %BUILDDIR%\%PLATFORM% --config Debug --prefix %BUILDDIR%\%PLATFORM%\Debug\xilinx\xrt --verbose
   cmake --install %BUILDDIR%\%PLATFORM% --config Debug --prefix %BUILDDIR%\%PLATFORM%\Debug\xilinx\xrt
   if errorlevel 1 (exit /B %errorlevel%)
)

if [%RELEASE%] == [1] (
   echo cmake --build %BUILDDIR%\%PLATFORM% --config Release --verbose
   cmake --build %BUILDDIR%\%PLATFORM% --config Release --verbose
   if errorlevel 1 (exit /B %errorlevel%)

   echo cmake --install %BUILDDIR%\%PLATFORM% --config Release --prefix %BUILDDIR%\%PLATFORM%\Release\xilinx\xrt --verbose
   cmake --install %BUILDDIR%\%PLATFORM% --config Release --prefix %BUILDDIR%\%PLATFORM%\Release\xilinx\xrt
   if errorlevel 1 (exit /B %errorlevel%)

   ECHO ====================== Create SDK ZIP archive ============================
   echo cpack -G ZIP -B %BUILDDIR%\%PLATFORM% -C Release --config %BUILDDIR%\%PLATFORM%\CPackConfig.cmake
   cpack -G ZIP -B %BUILDDIR%\%PLATFORM% -C Release --config %BUILDDIR%\%PLATFORM%\CPackConfig.cmake
   if errorlevel 1 (exit /B %errorlevel%)

   if [%CREATE_PACKAGE%]  == [1] (
      ECHO ====================== Creating MSI Archive ============================
      echo cpack -G WIX -B %BUILDDIR%\%PLATFORM% -C Release --config %BUILDDIR%\%PLATFORM%\CPackConfig.cmake
      cpack -G WIX -B %BUILDDIR%\%PLATFORM% -C Release --config %BUILDDIR%\%PLATFORM%\CPackConfig.cmake
      if errorlevel 1 (exit /B %errorlevel%)
   )
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
ECHO [-noabi]                   - Do not compile with ABI version check (make incremental builds faster)
ECHO [-opt]                     - Creates a release build
ECHO [-sdk]                     - Create NSIS XRT SDK Installer for NPU (requires NSIS installed).
echo [-package]                 - Packages the release build to a MSI archive.
ECHO [-npu]                     - Build NPU component of XRT (deployment and development)
ECHO [-hip]                     - Enable hip library build
GOTO:EOF

:Clean
IF EXIST %BUILDDIR%\%PLATFORM% (
  ECHO Removing '%BUILDDIR%\%PLATFORM%' directory...
  rmdir /S /Q %BUILDDIR%\%PLATFORM%
)
