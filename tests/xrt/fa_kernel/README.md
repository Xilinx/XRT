This test is for Fast Adapter(FA) kernel

## Test 1
This test is used to test hardware before XRT fully support FA kernel.

User space application directly construct FA descriptor and control FA kernel.
Map PLRAM and FA adapter register space to user's application, without interrupt.

Assume the kernel function is AES (same as U.2 1M IOPS PoC demo)
Only use XRT APIs to allocate buffer on the device.
Source code: user_polling.cpp

Run test:
./user_polling -k <xclbin>

## Compile
Source setup.sh after install XRT package.
``` bash
$ make
```

