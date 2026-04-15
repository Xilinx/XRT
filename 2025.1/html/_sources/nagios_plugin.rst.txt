.. _nagios_plugin.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.


xrt nagios plugin
======

The ``xrt nagios plugin`` tool is a Nagios plugin developed to work with a Nagios infrastructure monitoring system.

For more information on Nagios click `here <https://www.nagios.org/>`_.

The plugin places the requested device report text in a JSON format into the standard output.


**Options**: These are the options can be used. 

 - The ``--device`` (or ``-d``) specifies the target device to query for data

    - <user bdf> :  The Bus:Device.Function of the device of interest

- The ``--report`` (or ``-r``) switch can be used to view specific report(s) of interest from the following options

    - See :ref:`xbutil examine report options <xbutil_report_label>` for a list of valid reports

**Example commands** 


.. code-block:: shell

     ./nagios_plugin.sh --device 0000:b3:00.1 --report platform
     ./nagios_plugin.sh -d b3:00 -r platform
