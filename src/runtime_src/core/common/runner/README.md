<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved. -->
# Runner infrastructure

This directory contains xrt::runner infrastructure. The runner is
broken into two json components.  First is the recipe that defines a
model executed by the xrt::runner.  Second is the profile that defines
under what constraints the model is executed.

- [recipe](recipe.md)
- [profile](profile.md)

## Instantiating an xrt::runner 

An execution profile is useless without a run recipe.  But a run
recipe can have several execution profiles, and less likely a
profile could be used with multiple recipes.

The profile (and the recipe) may reference file artifacts. These
artifacts can be passed to the runner in two different ways. (1) the
runner can be instantiated with a path to a directory containing the
referenced artifacts (files), or (2) it can be instantiated with an
artifacts repository that have all the artifacts in memory pre-loaded.

See [runner.h](runner.h) or details.

## Reporting

The runner infrastructure reports metrics upon request.  The metrics
are currently loosely defined and most metrics are collected upon 
request.

Below is a sample of what is reported by the runner and returned
as a json std::string. 

The schema will change before it is finalized and versioned.

```
{
  "cpu": {
    "elapsed": 491726,    # execution elapsed time (us)
    "latency": 21,        # execution computed latency (us)
    "throughput": 45793   # execution average throughput (op/s)
  },
  "hwctx": {
    "columns": 0          # Number of columns (not implemented)
  },
  "resources": {
    "buffers": 5,         # Number of xrt::bo objects created
    "kernels": 1,         # Number of xrt::kernel objects created
    "runs": 9,            # Number of xrt::run objects created
    "total_buffer_size": 100 # Total buffer size in bytes
  },
  "xclbin": {
    "uuid": "44968c89-7cfa-a9d6-38e0-17cfa13445f2"
  }
}
```

## xrt_runner.exe
As part of building XRT, the runner infrastructure builds a runner executable that
can be used to execute recipe and profile pairs.

The executable has two modes, (1) execution of a single recipe/profile
pair, (2) multi-threaded execution of multiple recipe and profile
pairs.

To use locally built xrt-runner.exe, it is important that KMD and UMD
is in sync with what xrt-runner.exe is built from.
```
xrt-runner.exe --help
usage: xrt-runner.exe [options]
 [--recipe <recipe.json>] recipe file to run
 [--profile <profile.json>] execution profile
 [--script <script>] runner script, enables multi-threaded execution
 [--dir <path>] directory containing artifacts (default: current dir)
 [--report] print metrics

% xrt-runner.exe --recipe recipe.json --profile profile.json [--dir <path>] [--report]
% xrt-runner.exe --script runner.json [--dir <path>] [--report]
```

### runner.json
Multi-threaded execution of the applcation is controlled by a separate json file that lists 
what recipe and profile pairs to execute and on how many threads.

```
{
  "threads": <number>
  jobs: [
    {
      "id": "custom string",
      "recipe": "<path>/recipe.json",
      "profile": "<path>/profile.json",
      "dir": "<path> artifacts referenced by recipe and profile"
    },
    ...
  ]
}
```

Each recipe/profile pair results in the creation of an xrt::runner
object.  All xrt::runner objects are created and inserted into a work
queue before execution starts.  Each thread executes work items
(xrt::runner) from the work queue until the queue is empty.

