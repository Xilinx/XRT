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

if "%1" == "-debug" (
  goto DebugDriverBuild
)

if "%1" == "-release" (
  goto ReleaseDriverBuild
)

if "%1" == "-all" (
  call:DebugDriverBuild
  if errorlevel 1 (exit /B %errorlevel%)

  call:ReleaseDriverBuild
  if errorlevel 1 (exit /B %errorlevel%)

  goto:EOF
)


if "%1" == "" (
  call:DebugDriverBuild
  if errorlevel 1 (exit /B %errorlevel%)

  call:ReleaseDriverBuild
  if errorlevel 1 (exit /B %errorlevel%)

  GOTO:EOF
)

ECHO Unknown option: %1
GOTO Help


REM --------------------------------------------------------------------------
:Help
echo.
echo Usage: builddrv.bat [options]
echo.
echo [-help]                    List this help
echo [clean^|-clean]             Remove build directories
GOTO:EOF

REM --------------------------------------------------------------------------
:Clean
if exist WDrvDebug (
  echo Removing 'WDrvDebug' directory...
  rmdir /S /Q WDrvDebug
)
if exist WDrvRelease (
  echo Removing 'WDrvRelease' directory...
  rmdir /S /Q WDrvRelease
)
GOTO:EOF


REM --------------------------------------------------------------------------
:DebugDriverBuild
echo ====================== Windows Debug Driver Build ============================
mkdir WDrvDebug
echo ====== Building Debug xclmgmt driver =======
msbuild /t:clean /t:build ../src/runtime_src/core/pcie/driver/windows/xocl/mgmt/xclmgmt.sln /p:Configuration=Debug
echo ====== Copying Debug xclmgmt driver =======
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\mgmt\sys\wdkbld\x64\Debug\xclmgmt .\WDrvDebug\xclmgmt /s /i /T /y
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\mgmt\sys\wdkbld\x64\Debug\xclmgmt\* .\WDrvDebug\xclmgmt\* /y
echo ====== Building Debug xocluser driver =======
msbuild /t:clean /t:build ../src/runtime_src/core/pcie/driver/windows/xocl/user/Src/XoclUserM2/Src/XoclUser.sln /p:Configuration=Debug
echo ====== Copying Debug xocluser driver =======
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\user\Src\XoclUserM2\Src\x64\Debug\XoclUser .\WDrvDebug\XoclUser /s /i /T /y
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\user\Src\XoclUserM2\Src\x64\Debug\XoclUser\* .\WDrvDebug\XoclUser\* /y
GOTO:EOF

REM --------------------------------------------------------------------------
:ReleaseDriverBuild
echo ====================== Windows Release Driver Build ============================
mkdir WDrvRelease
pushd WDrvRelease
popd
echo ====== Building Release xclmgmt driver =======
msbuild /t:clean /t:build ../src/runtime_src/core/pcie/driver/windows/xocl/mgmt/xclmgmt.sln /p:Configuration=Release
echo ====== Copying Release xclmgmt driver =======
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\mgmt\sys\wdkbld\x64\Release\xclmgmt .\WDrvRelease\xclmgmt /s /i /T /y
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\mgmt\sys\wdkbld\x64\Release\xclmgmt\* .\WDrvRelease\xclmgmt\* /y
echo ====== Building Release xocluser driver =======
msbuild /t:clean /t:build ../src/runtime_src/core/pcie/driver/windows/xocl/user/Src/XoclUserM2/Src/XoclUser.sln /p:Configuration=Release
echo ====== Copying Release xocluser driver =======
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\user\Src\XoclUserM2\Src\x64\Release\XoclUser .\WDrvRelease\XoclUser /s /i /T /y
xcopy ..\src\runtime_src\core\pcie\driver\windows\xocl\user\Src\XoclUserM2\Src\x64\Release\XoclUser\* .\WDrvRelease\XoclUser\* /y
GOTO:EOF
GOTO:EOF
