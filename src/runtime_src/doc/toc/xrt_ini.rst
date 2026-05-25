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


Runtime Log Sinks (``runtime_log``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``runtime_log`` key controls where XRT message output is sent. The following values are supported:

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - Value
     - Platform
     - Description
   * - ``null`` or empty
     - All
     - Discard all messages (silent mode)
   * - ``console``
     - All
     - Writes messages to console (default)
   * - ``syslog``
     - All
     - Routes to the OS-level centralized log on each platform:
       Linux uses the POSIX syslog (``/var/log/syslog``);
       Windows uses the Windows Application Event Log under source ``AMD_XRT``.
       Using ``syslog`` in ``xrt.ini`` works on both platforms without change.

       On Windows, messages can be filtered in Event Viewer
       (**Windows Logs → Application → Filter Current Log → Source: AMD_XRT**)
       or from the command line by source and/or severity level
       (Level: ``2`` = Error, ``3`` = Warning, ``4`` = Information):

       .. code-block:: powershell

          # PowerShell - all XRT events
          Get-EventLog -LogName Application -Source "AMD_XRT"

          # PowerShell - errors only
          Get-EventLog -LogName Application -Source "AMD_XRT" -EntryType Error

          # PowerShell - errors and warnings
          Get-EventLog -LogName Application -Source "AMD_XRT" -EntryType Error,Warning

       .. code-block:: bat

          :: Command prompt - all XRT events
          wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT']]]" /f:text

          :: Command prompt - errors only (Level=2)
          wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT'] and Level=2]]" /f:text

          :: Command prompt - errors and warnings (Level=2 or Level=3)
          wevtutil qe Application /q:"*[System[Provider[@Name='AMD_XRT'] and (Level=2 or Level=3)]]" /f:text

   * - ``<filename>``
     - All
     - Write messages to the specified file path (e.g., ``runtime_log = xrt_run.log``)

Example — redirect XRT logs to the OS system log on both Linux and Windows:

.. code-block:: ini

   [Runtime]
   runtime_log = syslog
   verbosity = 7


For a complete list of currently supported xrt.ini keys, default value, and valid key values please refer `Vitis Application Acceleration Development Flow Documentation <https://docs.amd.com/r/en-US/ug1702-vitis-accelerated-reference/xrt.ini-File>`_
