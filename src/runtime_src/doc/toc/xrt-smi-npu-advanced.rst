..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.


xrt-smi advanced (NPU)
======================

This page lists **advanced** (hidden) ``xrt-smi`` options for Ryzen client NPUs.
Common options are documented in :doc:`xrt-smi-npu`.

**Disclaimer:** Advanced commands are subject to change and can destabilize the system.
Use them only if you understand the risks.

Availability of reports, tests, and configure flags depends on your device and SKU.
Run ``xrt-smi --advanced <subcommand> --help`` on your system for the exact list.

Set the environment variable ``XRTSMIAdvanced=1``, then pass ``--advanced`` before the subcommand.


xrt-smi examine (advanced)
~~~~~~~~~~~~~~~~~~~~~~~~~~

**Advanced reports** (``--report`` / ``-r``)

- ``telemetry``: Telemetry data for the device
- ``preemption``: Preemption telemetry data for the device
- ``clocks``: Clock frequency information
- ``debug``: Debug configuration settings for the device

**Advanced options**

- ``--watch``: Refresh interval in seconds between examine updates. Exit with Ctrl+C.
- ``--firmware-log``: Show status|watch firmware log data
- ``--event-trace``: Show status|watch event trace data
- ``--context-health``: Show status|watch context health data


xrt-smi validate (advanced)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Advanced tests** (``--run`` / ``-r``)

- ``df-bw``: Run bandwidth test on data fabric
- ``shim-dma-bw``: Run 2xRead/1xWrite bandwidth test for SHIM DMA
- ``sanity``: Run a small model and validate sanity of the device
- ``preemption-overhead``: Measure preemption overhead at noop and memtile levels
- ``aie-reconfig-overhead``: Run end-to-end array reconfiguration overhead through shim DMA
- ``runlist-latency``: Run end-to-end latency test using runlist
- ``runlist-throughput``: Run end-to-end throughput test using runlist
- ``quick``: Run a subset of four tests: latency, throughput, runlist-latency, and runlist-throughput
- ``tct-all-col``: Measure average TCT processing time for all columns
- ``tct-one-col``: Measure average TCT processing time for one column
- ``temporal-sharing-overhead``: Run end-to-end temporal sharing overhead test

**Advanced options**

- ``--pmode``: Modes: default, powersaver, balanced, performance, turbo
- ``--loop``: Number of iterations to run the test


xrt-smi configure (advanced)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Advanced options**

- ``--force-preemption``: Force enable|disable and see status of preemption
- ``--event-trace``: Enable|disable event tracing
- ``--firmware-log``: Enable|disable firmware logging
- ``--auto-coredump``: Enable|disable automatic coredump on error
