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
defininng:

- how data is bound to resources
- how validation is performed
- how many times a run-recipe is executed and with what data

# JSON format

There are two sections of profile json:

1. [qos](#qos)
2. [bindings](#bindings)
3. [execution](#execution)

The `bindings` section defines how external resources are created,
initialized, and bound to a run-recipe.

The `execution` section controls how the run-recipe is executed and
how many times along with what should be done in each iteration.

## QoS

Simple key/value pairs designating configuration parameters for
hardware context creation. 
```
  "qos": {
    "gops": 10,
    "fps": 30
  },
```
The json schema doesn't enforce key names or value ranges, XRT will
warn but ignore unrecoqnized keys. Improper values are implementation 
defined.

## Bindings

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
A binding elememnt has some simple attributes and more complex
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
object.  Upon creation, the buffer object is initialzied per the init
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
        "skip": bytes // skip number of bytes in file
      }
```
File initialization implies that the resource buffer should be
intialized with content from `file`.  The `file` must reference
a key that locates a file on disk or in an artifacts repository used
during construction of the `xrt::runner`.  If the binding element
specifies a `size` value, then this size takes precendence over the
size of the file, otherwise the size of the file will be the size of
the buffer. The optional `skip` element
allows skipping first bytes of the file during initialization of the
buffer.

All the bytes of a buffer are initialized regardless of the size of the
`file`.

If the file (minus skip bytes) is smaller than the buffer, then 
the file wraps around and continues to initialize the buffer.

If the file of larger than the buffer then only buffer size bytes
of the file are used.

If a bindings element specifies `reinit`, then the buffer is
reinitialized with bytes from the file in each iteration of the
recipe.  The initalization in an iteration picks up from an offset
into the file at the point where the previous iteration stopped
copying.  Again, the file wraps around when end-of-file is reached
without filling all the bytes of the buffer.

#### Strided initialization

```
      "init": {
        "stride": 1,    // stide bytes
        "value": 239,   // value to write at each stride
        "begin": 0,     // begining of range to write at
        "end": 524288   // end of range
      }
```
A buffer can
also be initialized with a fixed pattern at every `stride` byte.  This 
is referred to as strided buffer initialization.

In the above example, the buffer range specified by `begin` and `end`
is written with the byte value `239` at every byte (`stride` is
1). The value can be any uint64_t value, but must be a decimal value
in order to be valid json.  To initalize a buffer with with
0xdeadbeef, convert to decimal and write:

```
      "init": {
        "stride": 4,
        "value": 3735928559
      }
```

#### Validation

After executon, a buffer can be validated against golden data. This is
specifed using a `validate` element for the binding.

```
    {
      "validate": {
        "file": "ofm.bin"
      }
    }
```

Validating can be against the content of a file as shown in above
example, or it can be against another resource from the run recipe, in
which case the validation element must contain a name reference
instead of a file:

```
      "validate": {
        "name": "ifm"
      }
```

The validate element will be enhanced to cover other validation
specifics as needed.

## Execution

The execution section of a profile specifies how many times the recipe
should be executed and how.  It controls what should happen after each
iteration and before next iteration.

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

The iteration element specifies what should happen before after each
iteration of the run recipe.

- `bind` indicates if buffers should be re-bound to the
recipe before an iteration.  Only buffers whos binding element
specifies `rebind` are re-bound.  All buffers are by default
bound to the recipe upon creation.
- `init` indicates if buffer should be re-initialized before an
iteration. Only buffers whos binding element specifies has an `init`
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

# Instatiating an xrt::runner with a profile

An execution profile is useless without a run recipe.  But a run
recipe can have several execution profiles, and less likely a
profile could be used with multiple recipes.

The profile (and the recipe) may reference file artifacts. These
artifacts can be passed to the runner in two different ways. (1) the
runner can be instantied with a path to a directory containing the
referenced artifacts (files), or (2) it can be instantiated with an
artifacts repository that have all the artifacts in memory pre-loaded.

See [runner.h](runner.h) or details.


