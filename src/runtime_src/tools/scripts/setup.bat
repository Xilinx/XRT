@echo off
REM Script to setup environment for XRT
REM This script is installed in XILINX_XRT root and must
REM be sourced from that location

set XILINX_XRT=%~dp0
set OCL_ROOT=C:\Xilinx\XRT\ext

set PATH=%XILINX_XRT%/bin;%OCL_ROOT%\bin;%PATH%

echo XILINX_XRT      : %XILINX_XRT%
echo OCL_ROOT        : %OCL_ROOT%
echo PATH            : %PATH%
