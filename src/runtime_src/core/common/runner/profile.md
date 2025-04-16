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

1. [bindings](#bindings)
2. [execution](#execution)

The `bindings` section defines how external resources are created,
initialized, and bound to a run-recipe.

The `execution` section controls how the run-recipe is executed and
how many times along with what should be done in each iteration.

## Bindings

The bindings section specifies how external buffers should be created,
initialized, and validated. The section is an array of binding elements,
where each binding element must reference a resource in the run-recipe.

In its simplest form, a binding element references a file from which
the resource should be created.  The xrt::runner will create an
`xrt::bo` for each specified binding element.  The size of the buffer
is determined from the size of the file, and the initial value of 
the buffer is initialized from the content of the file.

```
  "bindings": [
    { 
      "name": "wts",
      "file": "wts.bin",
      "bind": true
    },
    { ... }
  ],
```

The `bind` key specifies if the buffer should be re-bound to run
recipe in each iteration of the recipe (more about this in the
execution section).

If no `file` is specified, the `size` of the buffer must be instead be
present in the binding element.  Normally the size will appear along
with an element that specifies how the buffer should be initialized.
For example, here the binding is initialized with 1024 bytes of random
data:

```
    {
      "name": "ifm",
      "size": 1024,
      "bind": true,
      "init": {
        "random": true
      }
    },
```

It is also possible to specify the `size` even when a `file` is provided.
In this case the specified size take precendence over the size of the file, 
and the buffer created and bound to the recipe will be of specified
size.  The data copied into the buffer and the minimum of the specified size
or the size of the file.

```
    {
      "name": "ifm",
      "size": 4096,
      "bind": true
    },
```

The initialization can be a fixed `pattern` as well:

```
      "init": {
        "pattern": "A"
      }
```

After executon, a buffer can be validated against golden data. This is
specifed using a `validate` element for the binding.

```
    {
      "name": "ofm",
      "size": 4,
      "bind": true,
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
      "bind": false,
      "init": true,
      "wait": true,
      "validate": true
    }
  }
```

The iteration element specifies what should happen before after each
iteration of the run recipe.

- `bind` indicates if buffers should be re-bound to the
recipe before an iteration.
- `init` indicates if buffer should be initialized per what is
specified in the binding element.
- `wait` says that execution should wait for completion between
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


