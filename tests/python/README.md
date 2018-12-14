# Python binding using Ctypes
## Introduction
Ctypes is a foreign function library for Python. It provides C compatible data types, 
and allows calling functions in DLLs or shared libraries. It can be used to wrap these 
libraries in pure Python.

## Structure

```
XRT   
│
└───src
│   │
│   └───python
│   |   │   xclhal2_binding.py
│   |   │   xclbin_binding.py
│   │
│   └───runtime_src
│       │   ......
│ 
└───tests
│   │
│   └───python
│   |   │   README.md
│   |   │
│   |   └───00_hello
│   |   │   |   main.py
│   |   │   |   utils.py
│   |   │   |   Makefile
│   │
│   └───xrt
│   |   │
│   |   └───00_hello
│   |   │   |   ......

```

## Prerequisites
1. Python
2. Enum package: pip install Enum

## Using source files
Calling functions from xclhal2_binding.py and xclbin_binding.py
* import source file:
>> sys.path.append('< path of the source file >') <br>
from < source file > import *
* All functions from the source file are now callable 

## Run 00_hello
>> ./build.sh <br/>
cd XRT/tests/python/00_hello <br/>
cp kernel.xclbin . <br/>
python main.py -k kernel.xclbin

## Makefile for 00_hello
1. run <kernel.xclbin>: runs 00_hello/main.py -k kernel.xclbin
2. clean: cleans up all .pyc files
3. help: prints help for the 00_hello test
