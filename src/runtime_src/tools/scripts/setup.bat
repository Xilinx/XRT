@echo off
REM Script to setup environment for XRT
REM This script is installed in XILINX_XRT root and must
REM be sourced from that location

set XILINX_XRT=%~dp0

set PATH=%XILINX_XRT%\bin;%XILINX_XRT%\ext\bin;%PATH%

echo XILINX_XRT      : %XILINX_XRT%
echo PATH            : %PATH%
