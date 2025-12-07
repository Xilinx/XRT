#!/usr/bin/fish

set -gx XILINX_XRT "/opt/xilinx/xrt"
if test -n "$LD_LIBRARY_PATH"
  set -gx LD_LIBRARY_PATH "$XILINX_XRT/lib:$LD_LIBRARY_PATH"
else
  set -gx LD_LIBRARY_PATH "$XILINX_XRT/lib"
end

if test -n "$PATH"
  set -gx PATH "$XILINX_XRT/bin:$PATH"
else
  set -gx PATH "$XILINX_XRT/bin"
end

if test -n "PYTHONPATH"
  set -x PYTHONPATH "$XILINX_XRT/python:$PYTHONPATH"
else
  set -x PYTHONPATH "$XILINX_XRT/python"
end
