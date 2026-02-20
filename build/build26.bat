@ECHO OFF

REM SPDX-License-Identifier: Apache-2.0
REM Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

setlocal
set "DO_CLEAN=0"
set "SCRIPTDIR=%~dp0"
set "SCRIPTDIR=%SCRIPTDIR:~0,-1%"
set "BUILDDIR=%SCRIPTDIR%"
set "DEBUG=1"
set "RELEASE=1"
set "EXT_DIR=%BUILDDIR%\ext.vcpkg\vcpkg_installed\x64-windows"
set "EXT_DIR_USER=0"
set "CI_EXT_DIR=C:\Xilinx\XRT\ext.new"
set "CMAKEFLAGS="
set "NOCMAKE=0"
set "STAGE=0"
set "CREATE_SDK=0"
set "CREATE_PACKAGE=0"
set "INSTALL_ROOT="

IF DEFINED MSVC_PARALLEL_JOBS ( SET "LOCAL_MSVC_PARALLEL_JOBS=%MSVC_PARALLEL_JOBS%" ) ELSE ( SET "LOCAL_MSVC_PARALLEL_JOBS=4" )

REM --------------------------------------------------------------------------
:parseArgs
if "%~1"=="" goto argsParsed

if /I "%~1"=="-help"     goto help
if /I "%~1"=="-ext"      goto parseExt
if /I "%~1"=="-install"  goto parseInstall
if /I "%~1"=="-clean"    ( set "DO_CLEAN=1" & shift & goto parseArgs )
if /I "%~1"=="-dbg"      ( set "RELEASE=0" & shift & goto parseArgs )
if /I "%~1"=="-opt"      ( set "DEBUG=0" & shift & goto parseArgs )
if /I "%~1"=="-stage"    ( set "STAGE=1" & shift & goto parseArgs )
if /I "%~1"=="-sdk"      ( set "CREATE_SDK=1" & set "CMAKEFLAGS=%CMAKEFLAGS% -DXRT_NPU=1" & shift & goto parseArgs )
if /I "%~1"=="-pkg"      ( set "CREATE_PACKAGE=1" & shift & goto parseArgs )
if /I "%~1"=="-npu"      ( set "CMAKEFLAGS=%CMAKEFLAGS% -DXRT_NPU=1" & shift & goto parseArgs )
if /I "%~1"=="-noabi"    ( set "CMAKEFLAGS=%CMAKEFLAGS% -DDISABLE_ABI_CHECK=1" & shift & goto parseArgs )
if /I "%~1"=="-hip"      ( set "CMAKEFLAGS=%CMAKEFLAGS% -DXRT_ENABLE_HIP=ON" & shift & goto parseArgs )
if /I "%~1"=="-nocmake"  ( set "NOCMAKE=1" & shift & goto parseArgs )

echo Unknown option: %1
goto help

REM --------------------------------------------------------------------------
:parseExt
shift

if "%~1"=="" (
  echo ERROR: -ext requires a path as argument
  exit /B 2
)

set "EXT_DIR=%~1"
set "EXT_DIR_USER=1"
shift
goto parseArgs

REM --------------------------------------------------------------------------
:parseInstall
shift

set "INSTALL_ARG=%~1"
if "%INSTALL_ARG%"==""       ( set "INSTALL_ROOT=C:\Xilinx\XRT" & goto parseArgs )
if "%INSTALL_ARG:~0,1%"=="-" ( set "INSTALL_ROOT=C:\Xilinx\XRT" & goto parseArgs )

set "INSTALL_ROOT=%INSTALL_ARG%"
shift
goto parseArgs

REM --------------------------------------------------------------------------
:argsParsed
if "%DO_CLEAN%"=="1" goto clean
call :pickGenerator

REM Prefer repo-local vcpkg deps when present; otherwise fall back to CI ext.new.
if "%EXT_DIR_USER%"=="0" (
  if not exist "%EXT_DIR%\" (
    if exist "%CI_EXT_DIR%\" set "EXT_DIR=%CI_EXT_DIR%"
  )
)

if not exist "%EXT_DIR%\" (
  echo ERROR: dependency prefix not found: "%EXT_DIR%"
  echo        Use -ext ^<path^>, or run src\runtime_src\tools\scripts\xrtdeps-vcpkg.bat
  exit /B 2
)

if "%INSTALL_ROOT%"==""   goto :skipInstallCheck
if not "%DEBUG%"=="1"     goto :skipInstallCheck
if not "%RELEASE%"=="1"   goto :skipInstallCheck
echo ERROR: -install requires -dbg or -opt (cannot install both configs into one prefix)
exit /B 2

:skipInstallCheck
if "%DEBUG%"=="1" call :doBuild Debug WDebug
if errorlevel 1 exit /B

if "%RELEASE%"=="1" call :doBuild Release WRelease
if errorlevel 1 exit /B

goto :EOF

REM --------------------------------------------------------------------------
:pickGenerator
if not "%GENERATOR%"=="" goto :EOF

cmake --help 2>NUL | findstr /C:"Visual Studio 18 2026" >NUL
if errorlevel 1 goto UseVS17

set "GENERATOR=Visual Studio 18 2026"
goto :EOF

:UseVS17
set "GENERATOR=Visual Studio 17 2022"
goto :EOF

REM --------------------------------------------------------------------------
:doBuild
set "CFG=%~1"
set "DIR=%~2"
set "XRT_INSTALL_PREFIX=%BUILDDIR%\%DIR%\xilinx\xrt"
if not "%INSTALL_ROOT%"=="" set "XRT_INSTALL_PREFIX=%INSTALL_ROOT%"

if "%NOCMAKE%"=="0" goto doConfig
goto doBuildOnly

REM --------------------------------------------------------------------------
:doConfig
echo Configuring %CFG%...
set "LOCAL_CMAKEFLAGS=%CMAKEFLAGS%"

if not "%EXT_DIR:vcpkg_installed=%"=="%EXT_DIR%" set "LOCAL_CMAKEFLAGS=%LOCAL_CMAKEFLAGS% -DCMAKE_PREFIX_PATH=%EXT_DIR%"
set "PROTOC_EXE=%EXT_DIR%\tools\protobuf\protoc.exe"
if exist "%PROTOC_EXE%" (
  set "LOCAL_CMAKEFLAGS=%LOCAL_CMAKEFLAGS% -DProtobuf_PROTOC_EXECUTABLE=%PROTOC_EXE%"
) else (
  if not "%EXT_DIR:vcpkg_installed=%"=="%EXT_DIR%" (
    echo ERROR: protoc.exe not found: "%PROTOC_EXE%"
    exit /B 2
  )
)

cmake -B "%BUILDDIR%\%DIR%" -G "%GENERATOR%" ^
  -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS% ^
  -DCMAKE_VS_GLOBALS="VcpkgEnabled=false" ^
  -DKHRONOS="%EXT_DIR%" ^
  -DBOOST_ROOT="%EXT_DIR%" ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
  %LOCAL_CMAKEFLAGS% ^
  "%BUILDDIR%\..\src"
if errorlevel 1 exit /B

REM --------------------------------------------------------------------------
:doBuildOnly
echo Building %CFG%...
cmake --build "%BUILDDIR%\%DIR%" --config %CFG% --verbose
if errorlevel 1 exit /B

echo Installing %CFG%...
cmake --install "%BUILDDIR%\%DIR%" --config %CFG% --prefix "%XRT_INSTALL_PREFIX%"
if errorlevel 1 exit /B

if "%STAGE%"=="1" (
  call :stageExt "%CFG%" "%XRT_INSTALL_PREFIX%"
  if errorlevel 1 exit /B
)

set "DO_ZIP=0"
if /I "%CFG%"=="Release" (
  if "%CREATE_SDK%"=="1" set "DO_ZIP=1"
  if "%CREATE_PACKAGE%"=="1" set "DO_ZIP=1"
  if "%DO_ZIP%"=="1" call :doCpackZip "%BUILDDIR%\%DIR%" "%CFG%" || exit /B
  if "%CREATE_PACKAGE%"=="1" call :doCpackWix "%BUILDDIR%\%DIR%" "%CFG%" || exit /B
)

exit /B 0

REM --------------------------------------------------------------------------
:stageExt
set "CFG=%~1"
set "XRT_ROOT=%~2"

set "VCPKG_BIN=%EXT_DIR%\bin"
if /I "%CFG%"=="Debug" if exist "%EXT_DIR%\debug\bin\" set "VCPKG_BIN=%EXT_DIR%\debug\bin"

set "XRT_EXT_BIN=%XRT_ROOT%\ext\bin"
if not exist "%XRT_EXT_BIN%\" mkdir "%XRT_EXT_BIN%" >NUL 2>NUL

copy /Y "%VCPKG_BIN%\boost_filesystem*.dll"      "%XRT_EXT_BIN%\" || exit /B
copy /Y "%VCPKG_BIN%\boost_program_options*.dll" "%XRT_EXT_BIN%\" || exit /B
copy /Y "%VCPKG_BIN%\libprotobuf*.dll"           "%XRT_EXT_BIN%\" || exit /B

exit /B 0

REM --------------------------------------------------------------------------
:doCpackZip
set "CPACK_BUILD_DIR=%~1"
set "CPACK_CFG=%~2"

echo ====================== Creating SDK ZIP archive ============================
echo cpack -G ZIP -B "%CPACK_BUILD_DIR%" -C %CPACK_CFG% --config "%CPACK_BUILD_DIR%\CPackConfig.cmake"
cpack -G ZIP -B "%CPACK_BUILD_DIR%" -C %CPACK_CFG% --config "%CPACK_BUILD_DIR%\CPackConfig.cmake"

exit /B

REM --------------------------------------------------------------------------
:doCpackWix
set "CPACK_BUILD_DIR=%~1"
set "CPACK_CFG=%~2"

echo ====================== Creating MSI Archive ==============================
echo cpack -G WIX -B "%CPACK_BUILD_DIR%" -C %CPACK_CFG% --config "%CPACK_BUILD_DIR%\CPackConfig.cmake"
cpack -G WIX -B "%CPACK_BUILD_DIR%" -C %CPACK_CFG% --config "%CPACK_BUILD_DIR%\CPackConfig.cmake"

exit /B

REM --------------------------------------------------------------------------
:help
echo.
echo Usage: build26.bat [options]
echo.
echo [-help]             - List this help
echo [-clean]            - Remove build artifact directories
echo [-dbg]              - Debug build only
echo [-opt]              - Release build only
echo [-ext ^<path^>]       - Sets EXT_DIR for dependencies (usually a vcpkg prefix)
echo [-install [^<path^>]] - Install prefix (default: C:\Xilinx\XRT)
echo [-stage]            - Copy a small set of runtime DLLs into ^<prefix^>\ext\bin
echo [-sdk]              - Build NPU and create a ZIP archive via CPack (Release only)
echo [-pkg]              - Create ZIP and MSI archives via CPack (Release only)
echo [-npu]              - Build NPU component of XRT
echo [-noabi]            - Disable ABI check
echo [-hip]              - Enable HIP build
echo [-nocmake]          - Skip re-configure; build/install only
echo.
echo Note:
echo     Default EXT_DIR uses build\ext.vcpkg if present; otherwise uses C:\Xilinx\XRT\ext.new (CI).
echo     To install vcpkg deps into build\ext.vcpkg, run:
echo     src\runtime_src\tools\scripts\xrtdeps-vcpkg.bat
goto :EOF

REM --------------------------------------------------------------------------
:clean
if exist "%BUILDDIR%\WDebug" rmdir /S /Q "%BUILDDIR%\WDebug"
if exist "%BUILDDIR%\WRelease" rmdir /S /Q "%BUILDDIR%\WRelease"
echo Build directories cleaned.

exit /B 0
