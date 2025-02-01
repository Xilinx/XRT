@ECHO OFF

REM Copyright (C) 2022 Xilinx, Inc
REM SPDX-License-Identifier: Apache-2.0
set SCRIPTDIR=%~dp0
set BUILDDIR=%SCRIPTDIR%

set DEBUG=1
set RELEASE=1
set EXT_DIR=C:/Xilinx/XRT/ext.new
set CREATE_PACKAGE=0
set CREATE_SDK=0
set CMAKEFLAGS=
set NOCMAKE=0
set NOCTEST=0
set XCLMGMT_DRIVER=
set XCLMGMT2_DRIVER=
set XOCLUSER_DRIVER=
set XOCLUSER2_DRIVER=

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
  if [%1] == [-xclmgmt] (
    set XCLMGMT_DRIVER=%2
    shift
  ) else (
  if [%1] == [-xocluser] (
    set XOCLUSER_DRIVER=%2
    shift
  ) else (
  if [%1] == [-xclmgmt2] (
    set XOCLMGMT2_DRIVER=%2
    shift
  ) else (
  if [%1] == [-xocluser2] (
    set XOCLUSER2_DRIVER=%2
    shift
  ) else (
  if [%1] == [-nocmake] (
    set NOCMAKE=1
  ) else (
  if [%1] == [-hip] (
    set CMAKEFLAGS=%CMAKEFLAGS% -DXRT_ENABLE_HIP=ON
  ) else (
    echo Unknown option: %1
    goto Help
  ))))))))))))))))
  shift
  goto parseArgs

:argsParsed

set CMAKEFLAGS=%CMAKEFLAGS% -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS% -DKHRONOS=%EXT_DIR% -DBOOST_ROOT=%EXT_DIR% -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ECHO CMAKEFLAGS=%CMAKEFLAGS%

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
ECHO [-noabi]                   - Do not compile with ABI version check (make incremental builds faster)
ECHO [-opt]                     - Creates a release build
ECHO [-sdk]                     - Create NSIS XRT SDK Installer for NPU (requires NSIS installed).
echo [-package]                 - Packages the release build to a MSI archive.
ECHO                              Note: Depends on the WIX application. 
ECHO [-xclmgmt arg]             - The directory to the xclmgmt drivers (used with [-package])
ECHO [-xocluser arg]            - The directory to the xocluser drivers (used with [-package])
ECHO [-xclmgmt2 arg]            - The directory to the xclmgmt2 drivers (used with [-package])
ECHO [-xocluser2 arg]           - The directory to the xocluser2 drivers (used with [-package])
ECHO [-npu]                     - Build NPU component of XRT (deployment and development)
ECHO [-hip]                     - Enable hip library build
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

REM --------------------------------------------------------------------------
:DebugBuild
echo ====================== Windows Debug Build ============================
set CMAKEFLAGS=%CMAKEFLAGS% -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=%BUILDDIR%/WDebug/xilinx
ECHO CMAKEFLAGS=%CMAKEFLAGS%

MKDIR %BUILDDIR%\WDebug
PUSHD %BUILDDIR%\WDebug

if [%NOCMAKE%] == [0] (
   cmake -G "Visual Studio 17 2022" %CMAKEFLAGS% ../../src
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

IF NOT [%XCLMGMT_DRIVER%] == [] (
  ECHO Packaging xclbmgmt driver directory: %XCLMGMT_DRIVER%
  set CMAKEFLAGS=%CMAKEFLAGS% -DXCL_MGMT=%XCLMGMT_DRIVER%
)

IF NOT [%XCLMGMT2_DRIVER%] == [] (
  ECHO Packaging xclbmgmt2 driver directory: %XCLMGMT2_DRIVER%
  set CMAKEFLAGS=%CMAKEFLAGS% -DXCL_MGMT2=%XCLMGMT2_DRIVER%
)

IF NOT [%XOCLUSER_DRIVER%] == [] (
  ECHO Packaging xocluser directory: %XOCLUSER_DRIVER%
  set CMAKEFLAGS=%CMAKEFLAGS% -DXOCL_USER=%XOCLUSER_DRIVER%
)

IF NOT [%XOCLUSER2_DRIVER%] == [] (
  ECHO Packaging xocluser2 directory: %XOCLUSER2_DRIVER%
  set CMAKEFLAGS=%CMAKEFLAGS% -DXOCL_USER2=%XOCLUSER2_DRIVER%
)

ECHO CMAKEFLAGS=%CMAKEFLAGS%

if [%NOCMAKE%] == [0] (
   cmake -G "Visual Studio 17 2022" %CMAKEFLAGS% ../../src
   IF errorlevel 1 (POPD & exit /B %errorlevel%)
)

cmake --build . --verbose --config Release --target install
IF errorlevel 1 (POPD & exit /B %errorlevel%)

ECHO ====================== Zipping up Installation Build ============================
cpack -G ZIP -C Release

if [%CREATE_SDK%] == [1] (
  ECHO ====================== Creating SDK Installer ============================
  cpack -G NSIS -C Release
)

popd
GOTO:EOF
