Re: CR-988905
Create kernel with multiple CUs connected to different memory banks.
Create sub-device for each CU and a context for each sub-device.
Use clCreateBuffer without specifying memory bank.
Execute kernel.

To build and run locally

% env XILINX_XRT=/opt/xilin/xrt make host.exe
% env XILINX_XRT=/opt/xilinx/xrt XILINX_SDX=<TA path> make MODE=hw DSA=xilinx_vcu1525_dynamic_5_1 xclbin
% [run.sh] ./host.exe addone.xclbin
