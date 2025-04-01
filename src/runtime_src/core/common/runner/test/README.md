<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved. -->
# Runner tests

This directory contains runner test code.

## recipe.cpp

A test wrapper for creating a `runner` from a `run-recipe.json`.  Used
for debugging purposes, basically validates that the run-recipe can be
parsed and that resources can be created.

## runner.cpp

A complete host code for creating a runner and executing the execution
section of the recipe.

The code will execute an argument recipe with external resources bound
through command line arguments.

```
% runner.exe [-r name:path]* [-b name:path]* [-g name:path]* -recipe <recipe>
```

The recipe references resources through `name` matching.  External resources
must be made available to the runner in two ways:

1. The resource must be bound to the runner after the runner has been created.
2. The resource must be in-memory in a repository passed to the runner constructor.

The runner.cpp file supports creating `xrt::bo` external objects from
a binary file specified through `--buffer name:path` command line switch.
This triggers the host code to create an `xrt::bo` and populate it
with the content of the file pointed to by `path`.  The host code
binds this resource to the runner using 1) above before the runner is
executed.  The `--buffer` switch can be repeated any number of times.

The runner.cpp supports loading external resources, for example elf
files, into memory before calling the constructor of the runner.  This
is done using the `--resource name:path` command line switch and is the
2) method above.  The content of the file pointed to by `path` is read
into memory and associated with `name` in an artifacts resposotory
passed as argument the runner constructor.  The `--resource` switch can
be specified any number of times.

Fianlly, the runner supports loading golden data to be compared with
the content of an external buffer populated by the runner.  This is
done using the `--golden name:path` command line switch.  The `name` must
match that of a external buffer created with `--buffer`.  The `path`
identifies a file with golden data.  The golden data is compared to
the content of the external buffer after the runner has completed
execution.

The host code has 3 steps:

1. Create artifacts repository from `--resource` switches
2. Create an xrt_core::runner object from artifacts repo and `recipe`
3. Create external buffer resources from the `--buffer` switches
4. Bind the external resources to the runner
5. Execute runner 
6. Wait for runner to complete
7. Compare golden data specified in `--golden` switches.


## Build instructions

```
% mkdir build
% cd build
% cmake -DXILINX_XRT=c:/users/stsoe/git/stsoe/XRT-MCDM/build/WDebug/xilinx/xrt \
        -DXRT_ROOT=c:/users/stsoe/git/stsoe/XRT-MCDM/src/xrt ..
% cmake --build . --config Debug
```

