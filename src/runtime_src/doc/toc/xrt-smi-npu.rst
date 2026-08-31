..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.


xrt-smi (NPU)
=============

``xrt-smi`` is the XRT command-line tool for Ryzen client NPUs. On NPU it
provides the ``examine``, ``validate``, and ``configure`` subcommands, which are
described below.

The available reports and tests depend on your device and SKU. For every flag,
report, and test name supported on your system, run ``xrt-smi <subcommand> --help``.

**Global options**: These are the global options can be used with any command.

 - ``--verbose``: Turn on verbosity and shows more outputs whenever applicable
 - ``--batch``: Enable batch mode (disables escape characters)
 - ``--force``: When possible, force an operation
 - ``--help`` : Get help message
 - ``--version`` : Report the version of XRT and its drivers

Currently supported ``xrt-smi`` commands are

    - ``xrt-smi examine``
    - ``xrt-smi validate``
    - ``xrt-smi configure``


xrt-smi examine
~~~~~~~~~~~~~~~

The command ``xrt-smi examine`` can be used to find the details of the specific device.

**The supported options**


.. code-block:: shell

    xrt-smi examine [--report| -r] <report of interest> [--format| -f] <report format> [--output| -o] <filename>


**The details of the supported options**

- The ``--report`` (or ``-r``) switch can be used to view specific report(s) of interest from the following options

    - ``aie-partitions``: AIE partition information
    - ``all``: All reports supported for your device are generated
    - ``host``: Reports the host configuration and drivers (default when no report is selected)
    - ``platform``: Platform / device summary

- The ``--format`` (or ``-f``) specifies the report format. Note that ``--format`` also needs an ``--output`` to dump the report in json format. If ``--output`` is missing text format will be shown in stdout

    - ``JSON`` (**default**): The report is shown in latest JSON schema
    - ``JSON-2020.2``: The report is shown in JSON 2020.2 schema

- The ``--output`` (or ``-o``) specifies the output file to direct the output

- The ``--help`` (or ``-h``) shows the subcommand help


**Example commands**


.. code-block:: shell

    # Shows the host report
    xrt-smi examine

    # Reports the platform summary in the stdout
    xrt-smi examine --report platform

    # Reports "platform" and "aie-partitions" and dump in json format
    xrt-smi examine --report platform aie-partitions --format JSON --output n.json


xrt-smi validate
~~~~~~~~~~~~~~~~

The command ``xrt-smi validate`` validates the installed device by running
precompiled basic tests.

**The supported options**


.. code-block:: shell

    xrt-smi validate [--run| -r] <test> [--format| -f] <report format> [--output| -o] <filename>


**The details of the supported options**

- The ``--run`` (or ``-r``) specifies the particular test(s) to execute

    - ``all`` (**default**): runs all the applicable tests listed below
    - ``latency``: Run end-to-end latency test
    - ``throughput``: Run end-to-end throughput test
    - ``gemm``: Run GEMM INT8 workload and report throughput-oriented results

- The ``--format`` (or ``-f``) specifies the results format. Note that ``--format`` also needs an ``--output`` to dump the results in json format. If ``--output`` is missing text format will be shown in stdout

    - ``JSON`` (**default**): The results are shown in latest JSON schema
    - ``JSON-2020.2``: The results are shown in JSON 2020.2 schema

- The ``--output`` (or ``-o``) specifies the output file to direct the output

- The ``--help`` (or ``-h``) shows the subcommand help


**Example commands**


.. code-block:: shell

    # Run all the tests
    xrt-smi validate

    # Run "latency" test, produce text output in stdout
    xrt-smi validate --run latency

    # Run "latency" and "throughput" test and generate Json format
    xrt-smi validate --run latency throughput --format JSON --output xyz.json


xrt-smi configure
~~~~~~~~~~~~~~~~~

Command ``xrt-smi configure`` is used to configure specific settings based on the
need of user application.

**The supported options**

.. code-block:: shell

    xrt-smi configure [--pmode] <power mode> [--help]


**The details of the supported options**

- The ``--pmode`` selects the power mode of the device

    - ``default``: Set power mode to default
    - ``powersaver``: Set power mode to powersaver
    - ``balanced``: Set power mode to balanced
    - ``performance``: Set power mode to performance
    - ``turbo``: Set power mode to turbo

- The ``--help`` (or ``-h``) shows the subcommand help


**Example commands**


.. code-block:: shell

    # Set the power mode to performance
    xrt-smi configure --pmode performance

    # Restore the default power mode
    xrt-smi configure --pmode default
