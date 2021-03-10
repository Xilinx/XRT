@ECHO OFF

SET BOOST=C:/Xilinx/XRT/ext
SET KHRONOS=C:/Xilinx/XRT/ext

IF DEFINED MSVC_PARALLEL_JOBS ( SET LOCAL_MSVC_PARALLEL_JOBS=%MSVC_PARALLEL_JOBS%) ELSE ( SET LOCAL_MSVC_PARALLEL_JOBS=3 )

IF "%1" == "clean" (
  GOTO Clean
)

IF "%1" == "-clean" (
  GOTO Clean
)

IF "%1" == "-help" (
  GOTO Help
)

IF "%1" == "-debug" (
  GOTO DebugBuild
)

IF "%1" == "-release" (
  GOTO ReleaseBuild
)


IF "%1" == "-all" (
  CALL:DebugBuild
  IF errorlevel 1 (exit /B %errorlevel%)

  CALL:ReleaseBuild
  IF errorlevel 1 (exit /B %errorlevel%)

  goto:EOF
)


IF "%1" == "" (
  CALL:DebugBuild
  IF errorlevel 1 (exit /B %errorlevel%)

  CALL:ReleaseBuild
  IF errorlevel 1 (exit /B %errorlevel%)

  GOTO:EOF
)

ECHO Unknown option: %1
GOTO Help


REM --------------------------------------------------------------------------
:Help
ECHO.
ECHO Usage: build.bat [options]
ECHO.
ECHO [-help]                    - List this help
ECHO [-clean^|clean]             - Remove build directories
ECHO [-debug]                   - Creates a debug build
ECHO [-release]                 - Creates a release build
ECHO.
ECHO Additional options to be used afer with the '-release' option:
ECHO   [-package]               - Packages the release build to a MSI archive.
ECHO                              Note: Depends on the WIX application. 
ECHO   [-xclmgmt arg]           - The directory to the xclmgmt drivers (used with packaging)
ECHO   [-xocluser arg]          - The directory to the xocluser drivers (used with packaging)
ECHO   [-xclmgmt2 arg]          - The directory to the xclmgmt2 drivers (used with packaging)
ECHO   [-xocluser2 arg]         - The directory to the xocluser2 drivers (used with packaging)
GOTO:EOF

REM --------------------------------------------------------------------------
:Clean
IF EXIST WDebug (
  ECHO Removing 'WDebug' directory...
  rmdir /S /Q WDebug
)
IF EXIST WRelease (
  ECHO Removing 'WRelease' directory...
  rmdir /S /Q WRelease
)
GOTO:EOF


REM --------------------------------------------------------------------------
:DebugBuild
echo ====================== Windows Debug Build ============================
MKDIR WDebug
PUSHD WDebug

ECHO MSVC Compile Parallel Jobs: %LOCAL_MSVC_PARALLEL_JOBS%

cmake -G "Visual Studio 15 2017 Win64" -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS% -DKHRONOS=%KHRONOS% -DBOOST_ROOT=%BOOST% -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
IF errorlevel 1 (POPD & exit /B %errorlevel%)

cmake --build . --verbose --config Debug
IF errorlevel 1 (POPD & exit /B %errorlevel%)

cmake --build . --verbose --config Debug --target install
IF errorlevel 1 (POPD & exit /B %errorlevel%)

POPD
GOTO:EOF

REM --------------------------------------------------------------------------
:ReleaseBuild
ECHO ====================== Windows Release Build ============================
MKDIR WRelease
PUSHD WRelease

ECHO MSVC Compile Parallel Jobs: %LOCAL_MSVC_PARALLEL_JOBS%

SET XCLMGMT_DRIVER=""
SET XCLMGMT2_DRIVER=""
SET XOCLUSER_DRIVER=""
SET XOCLUSER2_DRIVER=""
SET CREATE_PACKAGE=false

REM Evaluate the additional options
REM Warning: Do not put any echo statements in the "IF" blocks.  Doing so
REM          will result in expansion issues. e.g. the variable will not be set
SHIFT
:shift_loop_release
IF "%1" == "-package" (
  SET CREATE_PACKAGE=true
  SHIFT
  GOTO:shift_loop_release
)

IF "%1" == "-xclmgmt" (
  SET XCLMGMT_DRIVER="%2"
  SHIFT
  SHIFT
  GOTO:shift_loop_release
)

IF "%1" == "-xclmgmt2" (
  SET XCLMGMT2_DRIVER="%2"
  SHIFT
  SHIFT
  GOTO:shift_loop_release
)


IF "%1" == "-xocluser" (
  SET XOCLUSER_DRIVER="%2"
  SHIFT
  SHIFT
  GOTO:shift_loop_release
)

IF "%1" == "-xocluser2" (
  SET XOCLUSER2_DRIVER="%2"
  SHIFT
  SHIFT
  GOTO:shift_loop_release
)

REM Unknown option 
IF NOT "%1" == "" (
  POPD
  ECHO Unknown option: %1
  GOTO Help
)

IF NOT "%XCLMGMT_DRIVER%" == "" (
  ECHO Packaging xclbmgmt driver directory: %XCLMGMT_DRIVER%
)

IF NOT "%XCLMGMT2_DRIVER%" == "" (
  ECHO Packaging xclbmgmt2 driver directory: %XCLMGMT2_DRIVER%
)

IF NOT "%XOCLUSER_DRIVER%" == "" (
  ECHO Packaging xocluser directory: %XOCLUSER_DRIVER%
)

IF NOT "%XOCLUSER2_DRIVER%" == "" (
  ECHO Packaging xocluser2 directory: %XOCLUSER2_DRIVER%
)

cmake -G "Visual Studio 15 2017 Win64"  -DXCL_MGMT=%XCLMGMT_DRIVER% -DXOCL_USER=%XOCLUSER_DRIVER% -DXCL_MGMT2=%XCLMGMT2_DRIVER% -DXOCL_USER2=%XOCLUSER2_DRIVER% -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS% -DKHRONOS=%KHRONOS% -DBOOST_ROOT=%BOOST% -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
IF errorlevel 1 (POPD & exit /B %errorlevel%)

cmake --build . --verbose --config Release
IF errorlevel 1 (POPD & exit /B %errorlevel%)

cmake --build . --verbose --config Release --target install
IF errorlevel 1 (POPD & exit /B %errorlevel%)

ECHO ====================== Zipping up Installation Build ============================
cpack -G ZIP -C Release

IF "%CREATE_PACKAGE%" == "true" (
  ECHO ====================== Creating MSI Archive ============================
  cpack -G WIX -C Release
)

popd
GOTO:EOF


