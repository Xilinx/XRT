.. _xrt_ini.rst:

Configuration File xrt.ini
**************************

XRT uses various parameters to control execution flow, debug, profiling, and message logging during host application and kernel execution in software emulation, hardware emulation, and system run on the acceleration board. These control parameters are optionally specified in a runtime initialization file **xrt.ini**

XRT looks for xrt.ini in host executable path and current directory in that order. It stops search when an xrt.ini is found. If xrt.ini is not found, XRT built-in defaults are used as described in the table below.

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


**API Support**: From 2020.2 release the runtime configuration options can also be provided through C or C++ APIs. 

C APIs 

    - ``xrtIniStringSet``: Set a key and correspondong string value pair
    - ``xrtIniUintSet`` : Set a key and corresponding integer value pair

C++ API

    - ``xrt::ini::set``

Example

.. code-block:: ini

    xrt::ini::set("Runtime.runtime_log", "console");
    xrt::ini::set("Runtime.verbosity", 5);


The following table lists all supported groups, keys, valid key values, and short descriptions on the function of the keys.

Runtime Group
=============

+---------------------+------------------------------+-------------------------------------------+
|  Key                |  Valid Values                |             Description                   |
+=====================+==============================+===========================================+
| api_checks          |  [true|false]                |Enable or disable OpenCL API checks:       |
|                     |                              |                                           |
|                     |                              |     - true: enable                        |
|                     |                              |     - false: disable                      |
|                     |                              |                                           |
|                     |                              |Default: true                              |
+---------------------+------------------------------+-------------------------------------------+
| runtime_log         |[null|console|syslog|filename]|Specify where the runtime logs are printed:|
|                     |                              |                                           |
|                     |                              |     - null: Do not print any logs.        |
|                     |                              |     - console: Print logs to stdout       |
|                     |                              |     - syslog: Print logs to Linux syslog  |
|                     |                              |     - filename: Print logs to the         |
|                     |                              |       specified file.                     |
|                     |                              |       Example, runtime_log=my_run.log     |
|                     |                              |                                           |
|                     |                              |Default: console                           |
+---------------------+------------------------------+-------------------------------------------+
| cpu_affinity        | [{N,N,...}]                  |Pin all runtime threads to specified CPUs. |
|                     |                              |                                           |
|                     |                              |Example: cpu_affinity = {4,5,6}            |
+---------------------+------------------------------+-------------------------------------------+
| verbosity           | [0|1|2|3|4|5|6|7]            |verbosity level of log messages. Higher    |
|                     |                              |number implies more verbosity              |
|                     |                              |                                           |
|                     |                              |Default: 4                                 |
+---------------------+------------------------------+-------------------------------------------+
|exclusive_cu_context | [false|true]                 |When setting true OpenCL process holds     |
|                     |                              |exclusive access of the Compute Units      |
|                     |                              |                                           |
|                     |                              |Default: false                             |
+---------------------+------------------------------+-------------------------------------------+








Debug Group
===========

+----------------------+------------------------------+------------------------------------------------------+
|  Key                 |  Valid Values                |             Description                              |
+======================+==============================+======================================================+
| profile              |  [true|false]                |Enable or disable OpenCL host code profile. Set true  |
|                      |                              |to generate profile_summary report                    |
|                      |                              |                                                      |
|                      |                              |Default: false                                        |
+----------------------+------------------------------+------------------------------------------------------+
| timeline_trace       |  [true|false]                |Enable or disable profile timeline trace. Set true to |
|                      |                              |generate timeline_trace report                        |
|                      |                              |                                                      |
|                      |                              |Default: false                                        |
+----------------------+------------------------------+------------------------------------------------------+
| data_transfer_trace  |  [coarse|fine|off]           |Enable device-level AXI transfers trace:              |
|                      |                              |                                                      |
|                      |                              |     - coarse: Shows CU transfer activity             |
|                      |                              |     - fine: Shows all AXI level burst data transfer  |
|                      |                              |     - off: Does not show device-level AXI transfer   |
|                      |                              |                                                      |
|                      |                              |Default: off                                          |
+----------------------+------------------------------+------------------------------------------------------+
| stall_trace          |[dataflow|memory|pipe|all|off]|Specifies type of stalls to be captured in timeline   |
|                      |                              |trace report:                                         |
|                      |                              |                                                      |
|                      |                              |     - dataflow: Stall related to intra-kernel streams|
|                      |                              |     - memory: Stall related to memory transfer       |
|                      |                              |     - pipe: Inter-kernel pipes, applicable to OpenCL |
|                      |                              |       kernel                                         |
|                      |                              |     - all: All type of stalls                        |
|                      |                              |     - off: Does not show stalls                      |
|                      |                              |                                                      |
|                      |                              |Default: off                                          |
+----------------------+------------------------------+------------------------------------------------------+
| app_debug            | [true|false]                 |If true, enable xprint and xstatus command during     |
|                      |                              |debugging with xgdb                                   |
|                      |                              |                                                      |
|                      |                              |Default: false                                        |
+----------------------+------------------------------+------------------------------------------------------+
| trace_buffer_size    |[N {K|M|G}]                   |Specifies the size of DDR/HBM memory for storing trace|
|                      |                              |data:                                                 |
|                      |                              |                                                      |
|                      |                              |     - N: Integer                                     |
|                      |                              |     - K|M|G: Units Kilobyte or Megabyte or Gigabyte  |
|                      |                              |                                                      |
|                      |                              |Note:                                                 |
|                      |                              |                                                      |
|                      |                              |   - This option only applicable in hardware flow     |
|                      |                              |   - If no unit is given byte is assumed              |
|                      |                              |                                                      |
|                      |                              |Example: trace_buffer_size=100M                       |
|                      |                              |                                                      |
|                      |                              |Default: 1M                                           |
+----------------------+------------------------------+------------------------------------------------------+
| lop_trace            |[false|true]                  | Enables or disables low overhead profiling.          |
|                      |                              |                                                      |
|                      |                              |     - false: Disable low overhead profiling          |
|                      |                              |     - true : Enable low overhead profiling           |
|                      |                              |                                                      |
|                      |                              | Default: false                                       |
|                      |                              |                                                      |
+----------------------+------------------------------+------------------------------------------------------+
| continuous_trace     |[false|true]                  |Enables the continuous offload of the device data     |
|                      |                              |while the application is running. In the event of a   |
|                      |                              |crash/hang a trace file will be available to help     |
|                      |                              |debugging.                                            |
|                      |                              |                                                      |
|                      |                              |     - false: Disable continous trance                |
|                      |                              |     - true : Enable continuous trace                 |
|                      |                              |                                                      |
|                      |                              | Default: false                                       |
+----------------------+------------------------------+------------------------------------------------------+
|continuous_trace_inte-|[N]                           |Specifies the interval in millisecond to offload      |
|rval_ms               |                              |the device data in continous trace mode (see above)   |
|                      |                              |                                                      |
|                      |                              | Default: 10                                          |
+----------------------+------------------------------+------------------------------------------------------+

Emulation Group
===============

+---------------------------+----------------------------+---------------------------------------------------+
|  Key                      |  Valid Values              |             Description                           |
+===========================+============================+===================================================+
| aliveness_message_interval|  [N]                       |Specify the interval in seconds that aliveness     |
|                           |                            |messages need to be printed.                       |
|                           |                            |                                                   |
|                           |                            |Default:300                                        |
+---------------------------+----------------------------+---------------------------------------------------+
| print_infos_in_console    |  [true|false]              |Controls the printing of emulation info messages   |
|                           |                            |to users console.                                  |
|                           |                            |                                                   |
|                           |                            |  Emulation info messages are always logged into a |
|                           |                            |  file called emulation_debug.log                  |
|                           |                            |                                                   |
|                           |                            |     - true = print in users console               |
|                           |                            |     - false = do not print in user console        |
|                           |                            |                                                   |
|                           |                            |Default: true                                      |
+---------------------------+----------------------------+---------------------------------------------------+
| print_warning_in_console  |  [true|false]              |Controls the printing of emulation warning messages|
|                           |                            |to users console.                                  |
|                           |                            |                                                   |
|                           |                            | Emulation warning messages are always logged into |
|                           |                            | a file called emulation_debug.log                 |
|                           |                            |                                                   |
|                           |                            |     - true = print in users console               |
|                           |                            |     - false = do not print in user console        |
|                           |                            |                                                   |
|                           |                            |Default: true                                      |
+---------------------------+----------------------------+---------------------------------------------------+
| print_errors_in_console   |  [true|false]              |Controls the printing of emulation error messages  |
|                           |                            |to users console.                                  |
|                           |                            |                                                   |
|                           |                            | Emulation error messages are always logged into a |
|                           |                            | file called emulation_debug.log                   |
|                           |                            |                                                   |
|                           |                            |     - true = print in users console               |
|                           |                            |     - false = do not print in user console        |
|                           |                            |                                                   |
|                           |                            |Default: true                                      |
+---------------------------+----------------------------+---------------------------------------------------+
|launch_waveform            |  [off|batch|gui]           |Specify how the waveform is saved and displayed    |
|                           |                            |during emulation:                                  |
|                           |                            |                                                   |
|                           |                            |   - off: Do not launch simulator waveform GUI, and|
|                           |                            |     do not save wdb file                          |
|                           |                            |   - batch: Do not launch simulator waveform GUI,  |
|                           |                            |     but save wdb file                             |
|                           |                            |   - gui: Launch simulator waveform GUI, and save  |
|                           |                            |     wdb file                                      |
|                           |                            |                                                   |
|                           |                            |Default: off                                       |
|                           |                            |                                                   |
|                           |                            | Note: The kernel needs to be compiled with debug  |
|                           |                            | enabled for the waveform to be saved and          |
|                           |                            | displayed in the simulator GUI.                   |
+---------------------------+----------------------------+---------------------------------------------------+
|timeout_scale              |[na|ms|sec|min]             |Specify the time scaling unit of timeout specified |
|                           |                            |clPollStreams command, otherwise Emulation does not|
|                           |                            |support timeout specified in clPollStreams command |
|                           |                            |                                                   |
|                           |                            | Default:na (not applicable)                       |
+---------------------------+----------------------------+---------------------------------------------------+
