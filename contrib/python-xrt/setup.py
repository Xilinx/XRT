#!/usr/bin/env python

from setuptools import setup, Extension, find_packages

import os
import re
import sys
import platform

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def find_boost(hint=None, verbose=True, min_boost_major_version=1, min_boost_minor_version=63):
    search_dirs = [] if hint is None else hint
    search_dirs += [
        "/usr/local",
        "/usr/local/homebrew",
        "/opt/local",
        "/usr"
    ]
    for d in search_dirs:
        path = os.path.join(d, "include", "boost", "random.hpp")
        print("searching directory " + path + " ...")
        if os.path.exists(path):
            # Determine the version.
            vf = os.path.join(d, "include", "boost", "version.hpp")
            if not os.path.exists(vf):
                continue
            src = open(vf, "r").read()
            v = re.findall("#define BOOST_LIB_VERSION \"(.+)\"", src)
            if not len(v):
                continue
            v = v[0]
            boost_version = v.split("_")
            major_boost_version = int(boost_version[0])
            minor_boost_version = int(boost_version[1])
            print(bcolors.BOLD + "Found boost version " + str(major_boost_version) + "." + str(minor_boost_version) + " in location " + d + bcolors.ENDC)
            if major_boost_version >= min_boost_major_version and minor_boost_version >= min_boost_minor_version:
                print(bcolors.OKGREEN + str(major_boost_version) + "." + str(minor_boost_version) + " > " + str(min_boost_major_version) + "." + str(min_boost_minor_version) + " Boost will be used" + bcolors.ENDC)
                return d, major_boost_version, minor_boost_version
            else:
                print(bcolors.FAIL + "Boost version is too old, minimum version " + str(min_boost_major_version) + "." + str(min_boost_minor_version) + " is required" + bcolors.ENDC)
    print("*********************************************************")
    print("**           No valid boost found on system            **")
    print("*********************************************************")
    raise RuntimeError("Boost not found")
    return None, None, None

if sys.platform == "win32" or sys.platform == "cygwin":
    raise NotImplementedError("Windows/Cygwin is currently not supported")

if sys.platform == "darwin":
    raise NotImplementedError("Max OSX is currently not supported")

if sys.platform == "linux":
    print("Linux detected, continue ...")

min_required_boost_major_version = 1
min_required_boost_minor_version = 63
hint = None

if 'XRT_PYTHON_LIBRARY' in os.environ:
    print("XRT_PYTHON_LIBRARY found in the environment, adding it to boost searching list")
    hint = [os.environ['XRT_PYTHON_LIBRARY']]

boost_dir, major_boost_version, minor_boost_version = find_boost(hint=hint, min_boost_major_version=min_required_boost_major_version, min_boost_minor_version=min_required_boost_minor_version)

boost_include_dir = boost_dir + "/include"
boost_lib_dir = boost_dir + "/lib"

major_python_version = sys.version_info[0]
minor_python_version = sys.version_info[1]
python_version = str(major_python_version) + "." + str(minor_python_version)
python_version_no_separator = str(major_python_version) + str(minor_python_version)
print("Python" + python_version + " detected, compiling for Python" + python_version + "...")

xrt_inclue_path = "/opt/xilinx/xrt/include"
xrt_lib = "/opt/xilinx/xrt/lib"
if 'XILINX_XRT' in os.environ:
    print('XILINX_XRT environment detected as ' + os.environ['XILINX_XRT'] + ', using specified path for compilation ...')
    xrt_inclue_path = os.environ['XILINX_XRT'] + '/include'
    xrt_lib = os.environ['XILINX_XRT'] + '/lib'

# default to ubuntu location
python_ld = "boost_python" + python_version_no_separator

if platform.dist()[0] == 'centos':
    python_ld = 'boost_python'

include_dirs = [
    boost_include_dir,
    "/usr/include",
    "./xrt/core/include",
    xrt_inclue_path
]

libraries_hw=[
    "xrt_core",
    "xdp",
    "xilinxopencl",
    "boost_numpy",
    "boost_python"
]

libraries_sw_emu=[
    "xrt_swemu",
    "common_em",
    "xdp",
    "xilinxopencl",
    "boost_numpy",
    "boost_python"
]

libraries_hw_emu=[
    "xrt_hwemu",
    "common_em",
    "xdp",
    "xilinxopencl",
    "boost_numpy",
    "boost_python"
]

library_dirs=[
    xrt_lib,
    boost_lib_dir,
    "/usr/lib"
]

source_files = [
    "xrt/core/hal.cpp",
    "xrt/core/src/device_management_api.cpp",
    "xrt/core/src/type_conversion.cpp",
    "xrt/core/src/input_validation.cpp",
    "xrt/core/src/error_report.cpp",
    "xrt/core/src/buffer_management_api.cpp",
    "xrt/core/src/kernel_management_api.cpp",
    "xrt/core/src/register_read_write_api.cpp",
    "xrt/core/src/unmanaged_api.cpp",
    "xrt/core/src/performance_monitoring_api.cpp",
    "xrt/core/src/streaming_api.cpp"
]

cxx_args_hw = [
    "-std=c++14"
]

cxx_args_sw_emu = [
    "-std=c++14",
    "-DSW_EMU"
]

cxx_args_hw_emu = [
    "-std=c++14",
    "-DHW_EMU"
]

setup(
    name='xrt',
    version='0.0.1',
    description='Lightweight Python API for XRT',
    author='Tianhao Zhou',
    author_email='tianhao.zhou@xilinx.com',
    url='https://github.com/tianhaoz95/xrt.py',
    ext_modules=[
        Extension(
            name='xrt.core.hal',
            sources=source_files,
            library_dirs=library_dirs,
            libraries=libraries_hw,
            include_dirs=include_dirs,
            extra_compile_args=cxx_args_hw,
            language='c++',
            depends=[]
        ),
        Extension(
            name='xrt.core.sw_emu_hal',
            sources=source_files,
            library_dirs=library_dirs,
            libraries=libraries_sw_emu,
            include_dirs=include_dirs,
            extra_compile_args=cxx_args_sw_emu,
            language='c++',
            depends=[]
        ),
        Extension(
            name='xrt.core.hw_emu_hal',
            sources=source_files,
            library_dirs=library_dirs,
            libraries=libraries_hw_emu,
            include_dirs=include_dirs,
            extra_compile_args=cxx_args_hw_emu,
            language='c++',
            depends=[]
        )
    ],
    packages=find_packages()
)
