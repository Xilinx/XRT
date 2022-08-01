.. _xrt_ini.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.


Configuration File xrt.ini
**************************

XRT uses various parameters to control execution flow, debug, profiling, and message logging during host application and kernel execution in software emulation, hardware emulation, and system run on the acceleration board. These control parameters are optionally specified in a runtime initialization file **xrt.ini**

XRT looks for xrt.ini file in the following order: 
    
   - The host executable path.
   - Current directory (from where application is executed) path. 

XRT stops search when an xrt.ini is found. If xrt.ini is not found, XRT built-in defaults are used.

If desired, the environment variable as shown below can be used to specify a custom xrt.ini path 

.. code-block:: ini 

   export XRT_INI_PATH=/path/to/xrt.ini



Runtime Initialization File Format
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The runtime initialization file is a text file with groups of keys and their values. Any line beginning with a semicolon (**;**) or a hash (**#**) is a comment. The group names, keys, and key values are all case in-sensitive.

There are three group of keys as below

  - **Runtime**: The keys in this group impact general XRT flow
  - **Debug**: The keys in this group are used to generate and configure the debug related files such as profile report and timeline trace
  - **Emulation**: The keys in this group are related to the Emulation flow only

The following is a simple example that turns on profile timeline trace and sends the runtime log messages to the console.

.. code-block:: ini

   #Start of Runtime group
   [Runtime]
   runtime_log = console

   #Start of Debug group
   [Debug]
   timeline_trace = true


**API Support**: From 2020.2 release the runtime configuration options can also be provided through Native XRT APIs. 


    - ``xrt::ini::set``

Example

.. code-block:: ini

    xrt::ini::set("Runtime.runtime_log", "console");
    xrt::ini::set("Runtime.verbosity", 5);


For a complete list of currently supported xrt.ini keys, default value, and valid key values please refer `Vitis Application Acceleration Development Flow Documentation <https://www.xilinx.com/html_docs/xilinx2021_1/vitis_doc/xrtini.html?#tpi1504034339424__section_tnh_pks_rx>`_
