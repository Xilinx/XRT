<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved. -->
# Execution profile for XRT runner

An execution profile is an extension to a run recipe (see
[recipe](recipe.md)).  It automates the run recipe by binding
resources to the XRT runner that executes the run recipe.

While the `xrt::runner` class can be used stand-alone by an
application or framework that explicitly manages external resources,
the execution recipe extends the runner to also manage the external
resources.

An execution profile is useful for testing of a run recipe.  It allows
for one external application controlling execution of a run recipe by
defining:

- how data is bound to resources
- how validation is performed
- how many times a run-recipe is executed and with what data

# JSON format

There are two sections of profile json:

1. [qos](#qos)
2. [bindings](#bindings)
3. [executions](#executions)

The `bindings` section defines how external resources are created,
initialized, and bound to a run-recipe.

The `execution` section controls how the run-recipe is executed and
how many times along with what should be done in each iteration.

## QoS

This section is optional.

Simple key/value pairs designating configuration parameters for
hardware context creation. 
```
  "qos": {
    "gops": 10,
    "fps": 30
  },
```
The json schema doesn't enforce key names or value ranges, XRT will
warn but ignore unrecognized keys. Improper values are implementation 
defined.

## Bindings

This section is optional if all buffer resources specified in the
recipe are specified with `size` (see [recipe](recipe.md#buffer)).

The bindings section specifies how external buffers should be created,
initialized, and validated. The section is an array of binding elements,
where each binding element must reference a resource in the run-recipe.

A binding element references the name of the resource buffer to bind
to the recipe along with various attributes.  The xrt::runner will
create an `xrt::bo` for each specified binding element.  The size of
the buffer is optional optional when the buffer is initialized from a
file, but is otherwise required.
```
  "bindings": [
    { 
      "name": "wts",
      "size": bytes
      ...
    },
    { ... }
  ],
```
A binding element has some simple attributes and more complex
nested attributes

### Simple attributes of a binding element

The simple attributes are json key-value pairs:
```
    { 
      "name": "wts",     // name of a resource buffer
      "size": bytes
      "rebind": bool,
      "reinit": bool,
    },
```

- `name` identifies the resource buffer in the recipe.
- `size` (optional with file initialization) specifies the size
of the `xrt::bo` created and bound to the recipe.
- `rebind` indicates if the buffer should be re-bound to the run
recipe in each iteration of the recipe (see execution section for more
details). All buffers are by default bound to the recipe upon
creation.
- `reinit` indicates if the buffer should be re-initialized in each
iteration of the recipe (see execution section for more details). All
buffers are by default initialized when created if their binding
element has an "init" element.

### Nested elements of a binding element

The binding element support the following optional nested elements:
```
      "init": { ... }
      "validate": { ... }
```

- `init` (optional) specifies how the `xrt::bo` created by the runner
should be initialized. There are multiple ways to initialize a buffer.
- `validate` (optional) specifies if the buffer content should be 
 validated and how after an execution of the recipe.
 
### Initialization of a resource buffer

A bindings element results in the creation of the `xrt::bo` buffer
object.  Upon creation, the buffer object is initialized per the init
sub-element.  There are several ways to initialize a buffer only one
of which can be specified.

#### Random initialization

```
      "init": {
        "random": true // random initialization of the full buffer
      }
```
Random initialization is using a `std::random_device` along with
`rd()` to create a pseudo random value for each byte of the buffer.
If random initialization is used, the `size` of the buffer must also
have been specified.

#### File initialization

```
      "init": {
        "file": "<path or repo key>",
        "skip": bytes    // skip number of bytes in file
        "begin": offset, // offset to start writing at (default: 0)
        "end": offset    // offset to end writing at (default: bo.size())
      }
```
File initialization implies that the resource buffer should be
initialized with content from `file`.  The `file` must reference
a key that locates a file on disk or in an artifacts repository used
during construction of the `xrt::runner`.  If the binding element
specifies a `size` value, then this size takes precedence over the
size of the file, otherwise the size of the file will be the size of
the buffer. The optional `skip` element allows skipping first bytes 
of the file during initialization of the buffer.

The range defined by `[begin, end[` (default: `[0, bo.size()[`) are 
the bytes of the buffer that will be populated with data from the file.

All the bytes of the buffer range are initialized regardless of the size 
of the `file`.

If the file (minus skip bytes) is smaller than the buffer range, then 
the file wraps around and continues to initialize the buffer range.

If the file is larger than the buffer range then only buffer range
bytes of the file are used.

If a bindings element specifies `reinit`, then the buffer is
reinitialized with bytes from the file in each iteration of the
recipe.  The initialization in an iteration picks up from an offset
into the file at the point where the previous iteration stopped
copying.  Again, the file wraps around when end-of-file is reached
without filling all the bytes of the buffer range.

#### Strided initialization

```
      "init": {
        "stride": 1,     // stride bytes
        "value": 239,    // value to write at each stride
        "begin": offset, // offset to start writing at (default: 0)
        "end": offset    // offset to end writing at (default: bo.size())
      }
```
A buffer can
also be initialized with a fixed pattern at every `stride` byte.  This 
is referred to as strided buffer initialization.

In the above example, the buffer range specified by `begin` and `end`
is written with the byte value `239` at every byte (`stride` is
1). The value can be any uint64_t value, but must be a decimal value
in order to be valid json.  To initialize a buffer with with
0xdeadbeef, convert to decimal and write:

```
      "init": {
        "stride": 4,
        "value": 3735928559
      }
```

#### Validation

After execution, a buffer can be validated against golden data. This is
specified using a `validate` element for the binding.

```
    {
      "validate": {
        "file": "ofm.bin"
        "skip": bytes    // skip number of bytes in file
        "begin": offset, // bo offset to start validation at
        "end": offset,   // bo offset to end validation at
      }
    }
```

Validating can be against the content of a file as shown in above
example.  The optional `skip` element allows skipping first bytes 
of the file prior to validating against buffer data.  The range
defined by `[begin, end[` (default: `[0, bo.size()[`) are 
the bytes of the buffer that will be validated against the file
data.

, or it can be against another resource from the run recipe, in
which case the validation element must contain a name reference
instead of a file:

```
      "validate": {
        "name": "ifm"
      }
```

The validate element will be enhanced to cover other validation
specifics as needed.

## Executions

The `executions` section of a profile profile is an array of one or
more executions of a recipe, where each [execution](#execution)
defines how the recipe should be executed.

```
  "executions": [
    { ... },
    { ... },
    ...
  ]
```

### Execution

The execution section of a profile is a sub-element of 
the profiles `executions` array.  It specified how many times the recipe
should be executed and how.  It controls what should happen after each
iteration and before next iteration. If `iterations` is not specified,
then the recipe will execute one iteration.

```
  {
    "name": "myexecution", // custom id for this execution
    "iterations": 500,     // default one iteration
    "verbose": false,      // disable reporting of cpu time
    "validate": true,      // validate after all iterations
    "runlist_threshold": 1 // when to use xrt::runlist
    "mode": mode           // latency, throughput, or validate
    "depth": depth         // clone the recipe runlist
    "iteration" : {
    }
  }
```
This section is optional.  If not present, the recipe will execute
one iteration.

- `iterations` (default: `1`) specifies how many times the recipe
  should execute. The specified value must be greater than 0.
- `verbose` (default: `true`) controls printing of metrics post all
  iterations. By default the profile execution will display to stdout
  elapsed, throughput, and latency computed from running the recipe
  specified number of iterations.
- `validate` (default: `false`) enables validation per binding
  elements upon completion of all iterations. If mode is set to
  validate then this element is implicitly `true` (see details
  in mode section).
- `runlist_threshold` (default: `6`) specifies when to
  xrt::runlist. xrt::runner controls when to use xrt::runlist versus a
  list of separate xrt::run objects. A value of `0` disables
  xrt::runlist completely, any other value is used to trigger when to
  use xrt::runlist based on corresponding number of recipe run objects.
- `mode` (optional) runs the recipe in specified mode. The recipe can
  be run in latency, throughput or validate mode (see details below).
- `depth` (default: 1 or 2). Specifies how many times the recipe runs should
  be cloned. All `runs` specified in a [recipe](recipe.md#execution) are
  treated as a single runlist.  In `throughput` mode the recipe runlist
  is default instantiated twice, but `depth` can be used to create more
  instances if that is necessary to keep the hardware busy.

#### mode
The `mode` element is optional but if present must be one of `latency`
or `throughput`:

- `latency` mode. In latency the runner treats the runs specified in
  the recipe execution section [recipe](recipe.md#execution)a as a
  single runlist. This runlist is executed `iterations` number of
  times, waiting for each iteration to complete before starting the
  next.  The latency is measured as as the average time it takes for a
  single iteration of the runlist.
- `throughput` mode. In throughput mode, the runner attempts to keep
  the hardware busy. It treats the runs specified in the recipe
  execution section [recipe](recipe.md#execution) as a single
  runlist. This runlist is instantiated `depth` number of times, where
  `depth` defaults to `2` if not specified. Each instantiated runlist
  is submitted for execution `iterations` number of times. In the
  first iteration all runlist instances are submitted one after the
  other.  In subsequent iterations, when a runlist completes,
  it is immediately resubmitted.  The throughput is measured as the
  number of runlists completed in a second.  The `depth` is
  significant to ensure that the hardware is kept busy, a `depth` of
  `1` is really measuring latency but still reported as throughput if
  `mode` is set to throughput.
- `validate` mode. In validate mode, the runner at a minimum enables
   validation per binding elements upon completion of specified or
   default number of iterations.

#### iteration
The `iteration` sub-element is optional, but if present specifies what
should happen before after each iteration of the run recipe.  Note,
that the iteration sub-element is ignored if `latency` or `throughput`
is specified.

```
  "execution" : {
    "iterations": 500,
    "iteration" : {
      "bind": false,    // re-bind binding buffers
      "init": true,     // re-initialize binding buffers
      "wait": true,     // wait for recipe completion
      "sleep": 1000,    // sleep between iterations
      "validate": true  // validate binding buffers
    }
  }
```

- `bind` indicates if buffers should be re-bound to the
recipe before an iteration.  Only buffers whose binding element
specifies `rebind` are re-bound.  All buffers are by default
bound to the recipe upon creation.
- `init` indicates if buffer should be re-initialized before an
iteration. Only buffers whose binding element has an `init`
element and specifies `reinit` are re-initialized.  All buffers are by
default initialized upon creation if their binding element has an
`init` element.
- `wait` says that execution should wait for completion between
iterations and after last iteration.
- `sleep` specifies how many milliseconds to sleep in between iterations
of the recipe.  If both `wait` and `sleep` are specified, sleep will
be applied after wait completes.
iterations and after last iteration.
- `validate` means buffer validation per what is specified in
the binding element.

