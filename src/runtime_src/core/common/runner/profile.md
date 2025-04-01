<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved. -->
# Execution profile for XRT runner

An execution profile is an extention to a run recipe (see
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

