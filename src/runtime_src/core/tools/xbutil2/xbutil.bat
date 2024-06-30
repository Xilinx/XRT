@echo off
setlocal

REM Working variables
set XRT_PROG=xrt-smi
echo ----------------------------------------------------------------------
echo                               WARNING:
echo                xbutil has been renamed to xrt-smi
echo        Please migrate to using xrt-smi instead of xbutil.
echo:
echo    Commands, options, arguments and their descriptions can also be 
echo                    reported via the --help option.
echo ----------------------------------------------------------------------

REM -- Examine the options
set XRTWRAP_PROG_ARGS=
  :parseArgs
    if [%1] == [] (
      goto argsParsed
    ) else (
      REM New option
      if [%1] == [-new] ( 
        echo INFO: The 'new' option is only valid for the linux version of xbmgmt
        shift
      ) else (
      if [%1] == [--new] ( 
        echo INFO: The 'new' option is only valid for the linux version of xbmgmt
        shift
      ) else (
        REM Unknown option, must be associated with the program
        set XRTWRAP_PROG_ARGS=%XRTWRAP_PROG_ARGS% %1
        shift
      )
    ))
    goto parseArgs
  :argsParsed

%XRT_PROG% %XRTWRAP_PROG_ARGS%

