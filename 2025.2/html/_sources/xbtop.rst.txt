.. _xbtop.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.

xbtop
=====

The ``xbtop`` command outputs card statistics including memory topology and DMA transfer metrics, CU usage, and power related data. This command is similar to the Linux top command. When running, it continues to operate until ``q`` is entered in the terminal window.

The command has the following option:

- The ``--device`` (or ``-d``) specifies the target device bdf
- The ``--refresh_rate`` (or ``-r``) specifies refresh rate in second (default 1 sec). 

The command ``xbtop`` shows the following reports in multiple pages (pages can be changed by entering ``n`` or ``p`` key in the terminal window)

Page 1: Memory

   - Device Memory Usage
   - Memory Topology
   - DMA Transfer Metrics
Page 2: Dynamic Region
   
   - Compute Usage
Page 3: Power


