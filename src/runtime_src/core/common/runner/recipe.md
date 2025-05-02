<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved. -->
# Run recipe for XRT

A run recipe defines a graph model that can be executed by XRT.

This directory contains a stand-alone `xrt::runner` class that reads and 
executes a run recipe json file.   The idea is to have tools, e.g. VAIML
generate the run recipe along with xclbin and control code for kernels.

The schema of the recipe json is defined in `schema/recipe.schema.json`. The
implementation of the runner drove some of the definition of the json
format.

A run recipe is associated with exactly one configuration (xclbin or
config elf) which, when loaded into a region (partition) on the
device, can run the recipe.

# JSON format

There are three sections in the run recipe.

1. [header](#header)
2. [resources](#resources)
3. [execution](#execution)

The `header` trivially contains the path (full name) of the
configuration data that should be loaded before resources can be
created or the recipe can be executed.

The `resources` section defines all buffer objects, kernel objects,
and cpu function objects used to execute the recipe. The resources are
created as the run recipe is loaded. External input and output buffer
may be bound later during the execution stage of recipe.

The `execution` section defines how the resources are connected
together during execution. It simply executes kernels and cpu
functions that were previously defined in the resource section with
arguments that were also defined in the resource section.  Execution
of kernels can consume partial buffer input and produce partial buffer
output per `size` and `offset` fields defined as part of specifying the
kernel arguments.

## Header

For the time being, the header stores nothing but the path to the
xclbin.  The xclbin contains the kernel meta data used by XRT when
xrt::kernel objects are created.  The xclbin contains PDIs for each
kernel, the PDIs are loaded by firmware prior to running a kernel.

The header section can be amended with other meta data as needed.

```
{
  "header": {
    "xclbin": "design.xclbin",
  },
  
  ...
}
```

The runner will use the xclbin from the `header` section to create an
xrt::hw_context, which is subsequently used to create xrt::kernel
objects.

## Resources

The resources section is a complete list of all objects that are used
when the recipe is executed. Each kernel used in the `execution`
section must be listed in the resources section.  All kernel argument
buffers used by kernels in the `execution` section must be listed in
the resources section.  Also all functions executed on the CPU must
be listed in the resources section.

### Kernel functions

Kernels listed in the resources section result in runner creating
`xrt::kernel` objects.  In XRT, the kernel objects are identified by
name, which must match a kernel instance name in the xclbin.

Kernels are constructed from the instance name and what control code
the kernel should execute.  The hardware context associated with the
kernel is created by the runner from the xclbin specified in the
recipe `header` section, so kernels in the resources section must
contain just the kernel instance name and the full path to an ELF with
the control code.

```
  "resources": {
    "kernels": [
      {
        "name": "k1",
        "instance": "DPU",
        "ctrlcode": "no-ctrl-packet.elf"
      }
    ]
  },
```

The name of the kernel in resources section must be unique in the list
of kernel instances, the name is used in the `execution` section to refer 
to which instance should be executed.

If a kernel is instantiated from the same instance kernel name and same
control code, then only one such kernel instance needs to be listed in
the resources section.  Listing multiple kernel instances referring to
the same xclbin kernel and using the same control code is not error,
but is not necessary.

### CPU functions

Functions to be executed on the CPU are listed in the resource section
along with a path to a library containing the individual function.
The library will be runtime loaded (dlopen); it will expose functions
through a function pointer that is returned through a query lookup
method, which it returned through a library entry (extern "C") function.

CPU function arguments are expected to be `xrt::bo` objects, for
example format converting functions will take an input buffer and
and populate an output buffer, both buffers must be specified in the
resource buffer section of the recipe.

A library path is relative to the install location of XRT based on 
the environment value of `XILINX_XRT` or from its inferred location if
not set.  On windows, the inferred location would be the driver store.

```
  "resources": {
    "cpus": [
      {
        "name": "convert_ifm",
        "library_path": "umd/convert.dll"
      },
      {
        "name": "convert_ofm",
        "library_path": "umd/convert.dll"
      },
      {
        "name": "average_pool",
        "library_path": "umd/operators.dll"
      }
    ]
  },
```

### Buffer

The buffer instances listed in the resources section refer to
`xrt::bo` objects that are used during execution of kernels. The
buffers can be graph inputs or outputs, which refer to application
created input and output tensors, or they can be internal buffers used
during execution of the compiled graph at the discretion of the
compiler (VAIML).

#### Buffer types

The `type` of a buffer is one of

- input
- output
- internal
- weight
- spill
- unknown

For all pratical purposes the `type` is ignored by the xrt::runner
when it creates the recipe.  The only enforcement is that internal
buffers must specify a size so that the recipe can create an xrt::bo
object for the internal buffer.  All other buffers are treated as
external and must be bound to the recipe by the framework.  Note that
binding can be implicit by using a [profile](profile.md) to explicit 
through the xrt::runner interface.

#### External buffers (graph input and output)

External buffers (input and output) are created by the framework /
application outside of the recipe and bound to the recipe during
execution.  If the recipe buffer element doesn't specify a buffer size, 
then the runner does not create `xrt::bo` objects for
external buffers, but instead relies on the framework
to bind these buffers to runner object created from the recipe.  The
external buffers must still be listed in the resources section and
specify a name that can be used when execution sets kernel arguments.

```
  "resources": {
    "buffers": [
      {
        "name": "wts",
        "type": "input",
      },
      {
        "name": "ifm",
        "type": "input",
      },
      {
        "name": "ofm",
        "type": "output",
      }
    ]
  }

``` 
If a buffer `size` is specified as in:

```
      {
        "name": "ofm",
        "type": "output",
        "size": 8196
      }
```
then the runner will create an `xrt::bo` internally for the specified
buffer, even if the buffer is specified as "output" it is treated as
internal by the runner.  The application framework can still bind an
external buffer the runner object created from the recipe, but doesn't
have to.

The `name` of the buffers in the resources section must be unique.
The name is used in the `execution` section to refer to kernel or cpu
buffer arguments.

#### Internal buffers

Internal buffers are created and managed by the runner. These are
buffers that are used internally within a graph to carry data from one
kernel or cpu execution to another.

These buffers are created and managed by runner, hence unlike the
external buffers, the size of internal buffer size must be specified
in the recipe.

```
  "resources": {
    "buffers": [
      {
        "name": "ifm_int",
        "type": "internal",
        "size": "1024"
      },
      {
        "name": "ofm_int",
        "type": "internal",
        "size": "1024"
      },
      {
        "name": "b0",
        "type": "internal",
        "size": "1024"
      },
      {
        "name": "b1",
        "type": "internal:,
        "size": "1024"
      },
      {
        "name": "b2",
        "type": "internal",
        "size": "1024"
      }
    ]
  }

``` 
The `size` is currently specified in bytes.

## Execution

The execution section is an ordered list of kernel or cpu instances
with arguments from the resources section. 

Before the runner can execute the recipe in the execution section, all
graph inputs and outputs must be bound to the recipe. As mentioned
earlier, external inputs and outputs are defined by the framework that
uses the runner.  Typically these external inputs and outputs are not
available at the time when the runner is initialized from the recipe
json.  In other words, the runner can be created even before the
framework has created input and output tensors, but it can of course
not be executed until the inputs and outputs are defined. The runner
API has methods that must be called to bind the external inputs and
outputs.

Arguments to a run can be a sub-buffer of the corresponding
resource.  A buffer in the resources section refer to the full buffer,
but a run can use just a portion of the resource.  By default
a run argument will use the full buffer, but optional attributes in
the json for a buffer can specify the size and an offset into the
resource buffer.

As an example below, the kernel resource `k1` is executed twice with 
3 arguments. The 3rd input is a sub-buffer of the `ifm_int` resource, the
4th is the full resource `wts`, and the finally the 5th is a
sub-buffer of `ofm_int`.

The example illustrates the calling of a CPU function from the `cpu`
resources section.  The CPU function calls are passed buffers from the
resources section and scalar values as needed.

```
  "execution": {
    "runs": [
      {
        "name": "convert_ifm",
        "where": "cpu",
        "arguments" : [
            { "name": "ifm", "argidx": 0 },
            { "name": "ifm_int", "argidx": 1 }
         ],
         "constants" : [
            { "value": "nchw2nchw4c", "type": "string", "argidx": 2 }
         ]
        ]
      },
      {
        "name": "k1",
        "arguments" : [
            { "name": "ifm_int", "size": 512, "offset": 0, "argidx": 3 },
            { "name": "wts", "argidx": 4 },
            { "name": "ofm_int", "size": 512, "offset": 512, "argidx": 5 }
        ]
      },
      {
        "name": "k1",
        "arguments" : [
            { "name": "ifm_int", "size": 512, "offset": 512, "argidx": 3 },
            { "name": "wts", "argidx": 4 },
            { "name": "ofm_int", "size": 512, "offset": 0, "argidx": 5 }
        ]
      },
      {
        "name": "convert_ofm",
        "where": "cpu"
        "arguments" : [
            { "name": "ofm_int", "argidx": 0 },
            { "name": "ofm", "argidx": 1 }
         ],
         "constants" : [
            { "value": "nchw4c2nchw", "argidx": 2 }
         ]
        ]
      },
      ...
    ]
  }
```

The runner internally creates sub-buffers out of the specified
resource buffers for each run. Both external and internal
resource buffers can be sliced and diced as required.

The runner creates `xrt::run` or `xrt_core::cpu::run` objects out of
the specified execution runs.  The runner creates a CPU or NPU runlist
for each contiguous sequence of CPU runs or NPU runs specified in the
run recipe. The runlist is inserted into a vector of runlists where
each individual runlist will be executed in sequence, when the
framework calls the runner API execute method.

In addition to the buffer arguments referring to resource buffers, the
xclbin kernels and cpu functions may have additional arguments that
need to be set. For example the current DPU kernel have 8 arguments
and some of these must be set to some sentinel value.  Here the
argument with index 0, represents the kernel opcode which specifies
the type of control packet used for the kernel resource object.  The
value `3` implies transaction buffer.

```
  "execution": {
    "runs": [
      {
        "name": "k1",
        "arguments" : [
            { "name": "wts", "argidx": 4 },
            { "name": "ifm", "argidx": 3 },
            { "name": "ofm", "argidx": 5 }
        ],
        "constants" : [
            { "value": "3", "type": "int", "argidx": 0 },
            { "value": "0", "type": "int", "argidx": 1 },
            { "value": "0", "type": "int", "argidx": 2 },
            { "value": "0", "type": "int", "argidx": 6 },
            { "value": "0", "type": "int", "argidx": 7 }
        ]
      }
    ]
  }
```

# Complete run recipe

For illustration here is a simple complete run recipe.json file that
has been validated on NPU.  There are no internal buffer and external
input and output are consumed during one kernel execution.  See the 
`runner/test/recipe.json` for an example leveraging cpu functions.

```
{
  "header": {
    "xclbin": "design.xclbin",
  },
  "resources": {
    "buffers": [
      {
        "name": "wts",
        "type": "input",
      },
      {
        "name": "ifm",
        "type": "input",
      },
      {
        "name": "ofm",
        "type": "output",
      }
    ],
    "kernels": [
      {
        "name": "k1",
        "instance": "DPU",
        "ctrlcode": "no-ctrl-packet.elf"
      }
    ]
  },
  "execution": {
    "runs": [
      {
        "name": "k1",
        "arguments" : [
            { "name": "wts", "argidx": 4 },
            { "name": "ifm", "argidx": 3 },
            { "name": "ofm", "argidx": 5 }
         ],
         "constants": [
            { "value": "3", "type": "int", "argidx": 0 },
            { "value": "0", "type": "int", "argidx": 1 },
            { "value": "0", "type": "int", "argidx": 2 },
            { "value": "0", "type": "int", "argidx": 6 },
            { "value": "0", "type": "int", "argidx": 7 }
        ]
      }
    ]
  }
}
```

# Runner API

The runner is constructed from a recipe json file and a device object.
The runner is a standard XRT C++ first class object with the following
API.  Include documentation will be beefed up when the runner code is 
moved to public XRT.

```
class runner_impl;
class runner
{
  std::shared_ptr<runner_impl> m_impl;  // probably unique_ptr is enough
public:
  // ctor - Create runner from a recipe json
  runner(const xrt::device& device, const std::string& recipe);

  // bind_input() - Bind a buffer object to an input tensor
  void
  bind_input(const std::string& name, const xrt::bo& bo);

  // bind_output() - Bind a buffer object to an output tensor
  void
  bind_output(const std::string& name, const xrt::bo& bo);

  // execute() - Execute the runner
  void
  execute();

  // wait() - Wait for the execution to complete
  void
  wait();
};
```

# CPU library requirements

The run recipe can refer to functions executed on the CPU.  These
functions should be implemented in a shared library that can be 
loaded at runtime by the runner based on `resources/cpus` section.

A referenced library is loaded by the runner, which subsequently looks
for exported entry point (symbol) called `open` to initialize the shared 
library. The `open()` is supposed to return function objects for callback 
functions within the library.   At present time, only one callback function
is required is the `lookup()` function, which the runner 
uses to lookup functions referenced in the recipe resources section.

The `lookup()` function must return the callable function that the
runner is requesting along with the number of arguments this function
expects.  If the function the runner is looking for is not available,
then the `lookup()` function should throw an exception (TODO: define
the exact exception to throw).  The reason the `lookup()` function is
not itself an exported "extern C" function like `open()` is that the
call semantics must be C++ with the bells and whistles that follow
(exceptions).

The signature of the `extern "C"` exported `open()` function and the 
C++ signature of the `lookup()` function is defined in `xrt_runner.h`
under `namespace xrt::cpu { ... }`.

```
/**
 * The xrt::runner supports execution of CPU functions as well
 * as xrt::kernel objects.
 *
 * The CPU functions are implemented in runtime loaded dynamic
 * libraries. A library must define and export a function that
 * initializes a callback structure with a lookup function.
 *
 * The signature of the lookup function must be
 * @code
 *  void lookup_fn(const std::string& name, xrt::cpu::lookup_args* args)
 * @endcode
 * where the name is the name of the function to lookup and args is a
 * structure that the lookup function must populate with the function
 * information.
 *
 * The arguments to the CPU functions are elided via std::any and
 * the signature of the CPU functions is fixed to
 * @code
 *  void cpu_function(std::vector<std::any>& args)
 * @endcode
 * Internally, the CPU library unwraps the arguments and calls the
 * actual function.
 */
namespace xrt::cpu {
/**
 * struct lookup_args - argument structure for the lookup function
 *
 * The lookup function takes as arguments the name of the function
 * to lookup along with lookup_args to be populated with information
 * about the function.
 *
 * @num_args - number of arguments to function
 * @callable - a C++ function object wrapping the function
 *
 * The callable library functions uses type erasure on their arguments
 * through a std::vector of std::any objects.  The callable must
 * unwrap the std::any objects to its expected type, which is
 * cumbersome, but type safe. The type erased arguments allow the
 * runner to be generic and not tied to a specific function signature.
*/
struct lookup_args
{
  std::uint32_t num_args;
  std::function<void(std::vector<std::any>&)> callable;
};

/**
 * struct library_init_args - argument structure for libray initialization
 *
 * The library initialization function is the only function exported
 * from the run time loaded library.  The library initialization
 * function is called by the runner when a resource references a
 * function in a library and the library is not already loaded.
 *
 * @lookup_fn - a callback function to be populated with the
 *   lookup function.
 *
 * The library initialization function is C callable exported symbol,
 * but returns a C++ function pointer to the lookup function.
*/
struct library_init_args
{
  std::function<void(const std::string&, lookup_args*)> lookup_fn;
};

/**
 * library_init_fn - type of the library initialization function
 * The name of the library initialization function is fixed to
 * "library_init".
*/
using library_init_fn = void (*)(library_init_args*);
} // xrt::cpu

```

A unit test for the cpu library and corresponding sample run recipe
that references the cpu library is under `test/cpulib.cpp` and
`test/main.cpp`

# Recipe json validation against schema

A schema for the recipe json is available in
[schema](schema/recipe.schema.json).  A recipe can be validated
against the schema by running [schema-validator.py](test/schema-validator.py):
```
% python3 schema-validator.py recipe.json schema\recipe.schema.json
JSON is valid against the schema.
```
