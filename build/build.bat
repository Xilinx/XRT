@ECHO OFF

set BOOST=C:/Xilinx/XRT/ext
set KHRONOS=C:/Xilinx/XRT/ext

if DEFINED MSVC_PARALLEL_JOBS ( SET LOCAL_MSVC_PARALLEL_JOBS=%MSVC_PARALLEL_JOBS%) ELSE ( SET LOCAL_MSVC_PARALLEL_JOBS=3 )

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
  if errorlevel 1 (exit /B %errorlevel%)

  call:ReleaseBuild
  if errorlevel 1 (exit /B %errorlevel%)

  goto:EOF
)


if "%1" == "" (
  call:DebugBuild
  if errorlevel 1 (exit /B %errorlevel%)

  call:ReleaseBuild
  if errorlevel 1 (exit /B %errorlevel%)

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
if exist WDebug (
  echo Removing 'WDebug' directory...
  rmdir /S /Q WDebug
)
if exist WRelease (
  echo Removing 'WRelease' directory...
  rmdir /S /Q WRelease
)
GOTO:EOF


REM --------------------------------------------------------------------------
:DebugBuild
echo ====================== Windows Debug Build ============================
mkdir WDebug
pushd WDebug

echo MSVC Compile Parallel Jobs: %LOCAL_MSVC_PARALLEL_JOBS%

cmake -G "Visual Studio 15 2017 Win64" -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS% -DKHRONOS=%KHRONOS% -DBOOST_ROOT=%BOOST% -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
if errorlevel 1 (popd & exit /B %errorlevel%)

cmake --build . --verbose --config Debug
if errorlevel 1 (popd & exit /B %errorlevel%)

cmake --build . --verbose --config Debug --target install
if errorlevel 1 (popd & exit /B %errorlevel%)

popd
GOTO:EOF

REM --------------------------------------------------------------------------
:ReleaseBuild
echo ====================== Windows Release Build ============================
mkdir WRelease
pushd WRelease

echo MSVC Compile Parallel Jobs: %LOCAL_MSVC_PARALLEL_JOBS%

cmake -G "Visual Studio 15 2017 Win64" -DMSVC_PARALLEL_JOBS=%LOCAL_MSVC_PARALLEL_JOBS% -DKHRONOS=%KHRONOS% -DBOOST_ROOT=%BOOST% -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../../src
if errorlevel 1 (popd & exit /B %errorlevel%)

cmake --build . --verbose --config Release
if errorlevel 1 (popd & exit /B %errorlevel%)

cmake --build . --verbose --config Release --target install
if errorlevel 1 (popd & exit /B %errorlevel%)

popd
GOTO:EOF
