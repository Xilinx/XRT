Re: PR to support dynamic CU selection

- Create kernel with multiple CUs connected to different memory banks.
- Use clCreateBuffer explicitly targeting memory bank.
- Use clSetKernelArg to filter kernel CUs to those that support the
  requested connectivity.
- Execute kernel.

- Also test error condition where requested connectivity is not
  supported.

To build and run locally
% env XILINX_XRT=/opt/xilin/xrt make host.exe
% env XILINX_XRT=/opt/xilinx/xrt XILINX_SDX=<TA path> make MODE=hw DSA=xilinx_vcu1525_dynamic_5_1 xclbin
% [run.sh] ./host.exe addone.xclbin
