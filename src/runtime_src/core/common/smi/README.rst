..
   SPDX-License-Identifier: Apache-2.0
   Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

========
xrt-smi
========

``xrt-smi`` is the XRT command-line tool for Ryzen client NPUs (among other targets). On Ryzen, the tool exposes **examine**, **validate**, and **configure** subcommands. The tables below list options and report/test names that are shown **without** ``--advanced`` (the *common* tier in the Ryzen SMI configuration).

Use ``xrt-smi <subcommand> --help`` for the full option list, including hidden and advanced-only choices.

-------------------------------------------------------------------------------

examine (Ryzen — common)
-------------------------------------------------------------------------------

These options apply to ``xrt-smi examine`` on Ryzen NPUs.

=======================  =====  ============================================
Option                   Short  Description
=======================  =====  ============================================
``--device``             ``-d`` Bus:Device.Function of the device (e.g. ``0000:d8:00.0``).
``--format``             ``-f`` Output format: ``JSON`` (default) or ``JSON-2020.2``.
``--output``             ``-o`` Write report output to the given file.
``--help``               ``-h`` Subcommand help.
``--report``             ``-r`` One or more reports (see table below).
=======================  =====  ============================================

**Reports** (values for ``--report`` / ``-r``) — Strix / default Ryzen:

=======================  =====================================================
Report                  Description
=======================  =====================================================
``aie-partitions``      AIE partition information.
``all``                 All known reports for this configuration.
``host``                Host information (default report when none specified).
``platform``            Platform / device summary.
=======================  =====================================================

**NPU3:** ``telemetry`` and ``preemption`` are also *common* reports (not advanced-only). **Phoenix:** same report set as Strix / default Ryzen above.

-------------------------------------------------------------------------------

validate (Ryzen — common)
-------------------------------------------------------------------------------

These options apply to ``xrt-smi validate`` on Ryzen NPUs.

=======================  =====  ============================================
Option                   Short  Description
=======================  =====  ============================================
``--device``             ``-d`` Bus:Device.Function of the device.
``--format``             ``-f`` Output format: ``JSON`` (default) or ``JSON-2020.2``.
``--output``             ``-o`` Write results to the given file.
``--help``               ``-h`` Subcommand help.
``--run``                ``-r`` One or more validate tests (see table below).
=======================  =====  ============================================

**Tests** (values for ``--run`` / ``-r``) — Strix family and NPU3 (common):

=======================  =====================================================
Test                    Description
=======================  =====================================================
``all``                 Run all applicable validate tests (default when none specified).
``latency``             End-to-end latency test.
``throughput``          End-to-end throughput test.
``gemm``                GEMM INT8 workload; reports TOPS-style results.
=======================  =====================================================

**Phoenix:** common tests are ``all``, ``latency``, and ``throughput`` only (no ``gemm`` in the Phoenix menu).

-------------------------------------------------------------------------------

configure (Ryzen — common)
-------------------------------------------------------------------------------

These options apply to ``xrt-smi configure`` on Ryzen NPUs.

=======================  =====  ============================================
Option                   Short  Description
=======================  =====  ============================================
``--device``             ``-d`` Bus:Device.Function of the device.
``--help``               ``-h`` Subcommand help.
``--pmode``              —      Power mode: ``default``, ``powersaver``, ``balanced``, ``performance``, or ``turbo``.
=======================  =====  ============================================

-------------------------------------------------------------------------------

See the FleXible RunTime ``README.rst`` under the top-level ``xrt/`` directory in this repository for a broader project overview.
