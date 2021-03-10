To build and run locally

[run.sh]: is local XRT build loader script
For example .../git/XRT/build/run.sh

# build executables
# [run.sh] <path>/XRT/tests/xrt/build.sh
# The execs are located under
# <path>/XRT/tests/xrt/build/<OS>/Debug/100_ert_ncu

# build xclbin
% pwd
<here>
% [run.sh] make -f xclbin.mk DSA=... MODE=... xclbin

# run
% [run.sh] xrt.exe -k kernel.hw.xclbin -jobs 32 -seconds 1 cus 8
