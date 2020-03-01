To build and run locally

[run.sh]: is local XRT build loader script
For example .../git/XRT/build/run.sh

# build executables
% cd ..
% mkdir build
% cd build
% [run.sh] cmake ..
% make

# build xclbin
% [run.sh] make -f xclbin.mk DSA=... MODE=... xclbin

# run
% [run.sh] xrt.exe -k kernel.hw.xclbin -jobs 32 -seconds 1 cus 8
