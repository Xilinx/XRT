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
are in sync with what xrt-runner.exe is built from.
```
usage: xrt-runner.exe [options]
 [--recipe <recipe.json>] recipe file to run
 [--profile <profile.json>] execution profile
 [--iterations <number>] override all profile iterations
 [--script <script>] runner script, enables multi-threaded execution
 [--threads <number>] number of threads to use when running script (default: #jobs)
 [--dir <path>] directory containing artifacts (default: current dir)
 [--mode <latency|throughput>] execute only specified mode (default: all)
 [--progress] show progress
 [--asap] process jobs immediately (default: wait for all jobs to initialize)
 [--report [<file>]] output runner metrics to <file> or use stdout for no <file> or '-'

% xrt-runner.exe --recipe recipe.json --profile profile.json [--iterations <num>] [--dir <path>]
% xrt-runner.exe --script runner.json [--threads <num>] [--iterations <num>] [--dir <path>]

Note, [--threads <number>] overrides the default number, where default is the number of
jobs in the runner script.

Note, [--iterations <num>] overrides iterations in profile.json, but not in runner script.
If the runner script specifies iterations for a recipe/profile pair, then this value is
sticky for that recipe/profile pair.

Note, [--mode <latency|throughput>] filters execution sections in profile.json such
only specified modes are executed. If the runner script specifies a mode for a recipe/profile
pair, then this value is sticky for that recipe/profile pair.
```

### runner.json
Multi-threaded execution of the application is controlled by a
separate json file (a script) that lists what recipe and profile pairs
to execute by worker threads.

A script is an array of jobs.
The number of worker threads defaults to the number of jobs in the jobs
array, but can be overwritten by an xrt-runner.exe command
line switch `[--threads <num>]`.

```
{
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

Here `id` identifies the job in reporting, `recipe` and `profile` are
paths to the recipe/profile pair used with this job, and `dir` is the
path to the directory with artifacts referenced by the recipe and
profile.

The `iterations` key is optional, but allows overriding `iterations`
as specified in the profile itself.  Another way to override profile
`iterations` is through xrt-runner.exe `[--iterations <num>]` command
line switch.  If `iterations` is specified in the script for a
recipe/profile pair, then the script value takes precedence over the
command line switch.

Each recipe/profile pair results in the creation of an xrt::runner
object.  The runner objects are inserted into a work
queue, and each thread executes work items
(xrt::runner) from the work queue until the queue is empty.
By default each thread blocks until the queue has been populated with 
all initialized jobs.  If `[--asap]` 
is specified when invoking xrt-runner.exe, jobs are executed as soon as
possible.
