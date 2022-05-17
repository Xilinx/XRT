.. _nagios_plugin.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.


xrt nagios plugin
======

The ``xrt nagios plugin`` tool is a Nagios plugin developed to work with a Nagios infrastructure monitoring system.

For more information on Nagios click `here <https://www.nagios.org/>`_.

The plugin places text into the standard output that can be parsed by a Nagios XL database or displayed by a Nagios Core instance.

When invoking the plugin through nagios please use the xrt nagios script in the same directory to properly setup all environmental variables.

The current information returned by the tool is a device's :
    - Mechanical data
    - Thermal data
    - Memory data
    - Electrical data 


**Options**: These are the options can be used. 

 - ``--help`` : Get help message

 - The ``--device`` (or ``-d``) specifies the target device to query for data
    
    - <user bdf> :  The Bus:Device.Function of the device of interest

**Example commands** 


.. code-block:: shell

     xrt_nagios_plugin --device 0000:b3:00.1
