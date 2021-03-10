This directory contains low level XRT tests.

The tests are implemented using XRT native APIs
https://confluence.xilinx.com/display/XSW/XRT+Native+APIs#XRTNativeAPIs-C++DeviceAPIs

A bash script ``build.sh`` in this directory is provided to bootstrap
the build.  The script can be used both on Linux and on Windows under
WSL bash.  Feel free to add a build.bat script if necessary.

To build xclbin, use the provided xclbin.mk file with XILINX_VITIS and
XILINX_XRT set.  Specify DSA path to xpfm file. The xclbin is built in
current directory.

build.sh
CMakeLists.txt
readme.txt

# Loopback example writing / reading buffer object
00_hello
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
└── testinfo.yml

# Kernel one invocation, 2 buffer options, scalar
02_simple
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── testinfo.yml
└── xclbin.mk

# Copy kernel from one buffer object to another, scalar
03_loopback
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── testinfo.yml
└── xclbin.mk

# Swizzle a vector of int4 using parent buffer object and
# multiple kernel invocations with sub buffer objects.
04_swizzle
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── testinfo.yml
└── xclbin.mk

# Kernel writes a sequence to a buffer object
07_sequence
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── testinfo.yml
└── xclbin.mk

# Same tests implemented using OpenCL (ocl.cpp), XRT shim (xrt.cpp)
# XRT kernel APIs and shim APIs (xrtx.cpp), XRT native APIs (xrtxx.cpp)
# Performance test running multiple jobs concurrently on 1 to 8 CUs
# for specified number of seconds.  Report completed jobs at end.
100_ert_ncu
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── ocl.cpp
├── readme.txt
├── testinfo.yml
├── xaddone_hw_64.h
├── xclbin.mk
├── xrt.cpp
├── xrtx.cpp
└── xrtxx.cpp

# Multi-process test case
102_multiproc_verify
├── CMakeLists.txt
├── hello.cl
└── main.cpp

# mmult kernel 
11_fp_mmult256
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── testinfo.yml
└── xclbin.mk

# Add one kernel
13_add_one
├── CMakeLists.txt
├── kernel.cl
├── main.cpp
├── testinfo.yml
└── xclbin.mk

# Verify kernel
22_verify
├── CMakeLists.txt
├── hello.cl
├── main.cpp
├── xclbin.mk
└── testinfo.yml
