.. _xbmgmt.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.

xbmgmt
======

This document describes the latest ``xbmgmt`` commands. These latest commands are default from 21.1 release.   



For an instructive video on xbmgmt commands listed below click `here <https://www.youtube.com/watch?v=ORYSrYegX_g>`_.

**Global options**: These are the global options can be used with any command. 

 - ``--verbose``: Turn on verbosity and shows more outputs whenever applicable
 - ``--batch``: Enable batch mode
 - ``--force``: When possible, force an operation
 - ``--help`` : Get help message
 - ``--version`` : Report the version of XRT and its drivers

Currently supported ``xbmgmt`` commands are

    - ``xbmgmt configure``
    - ``xbmgmt dump``
    - ``xbmgmt examine``
    - ``xbmgmt program``
    - ``xbmgmt reset``

**Note**: For applicable commands, if only one device is present on the system ``--device`` (or ``-d``) is not required. If more than one device is present in the system, ``--device`` (or ``-d``) is required.


xbmgmt configure
~~~~~~~~~~~

The ``xbmgmt configure`` command provides advanced options for configuring a device's memory, clock, and DDR memory retention settings. A .ini file input is required for configuration except for DDR memory retention. The .ini file can be located in a directory of your choosing with the path specified in the command.

- TIP: Instead of creating a .ini file from scratch, use ``xbmgmt dump --config`` (see the ``xbmgmt dump`` section) to generate the file contents which can be edited accordingly.

**Command Options**

The supported options are ``--input`` and ``--retention``. Command usage is below.

.. code-block:: shell

    xbmgmt configure [--device| -d] <management bdf> [--input] <path to filename with .ini extension>


Enabling/Disabling clock throttling on a device

- When enabled, clock throttling reduces the kernel clock frequency dynamically when either thermal or electrical sensors exceed defined threshold values. By lowering the clock frequency, clock throttling reduces the required power and subsequently generated heat. Only when all sensor values fall below their respective clock throttling threshold values will the kernel clock be restored to full performance.
- Default clock throttling threshold values are available in `<UG1120> <https://docs.xilinx.com/r/en-US/ug1120-alveo-platforms>`_ for supported platforms.
- The contents of the .ini file for clock throttling configuration should be similar to the example provided below. Underneath the first line, ``[Device]``, specify one or more key-value pairings as needed.

.. code-block:: ini

    [Device]
    throttling_enabled=true
    throttling_power_override=200
    throttling_temp_override=90

- The definition of the three key-value pairings are given below.

    - ``throttling_enabled`` : When set to ``true``, clock throttling will be enabled. When set to ``false``, clock throttling will be disabled, and no clock throttling will occur. The default value is ``false``.
    - ``throttling_power_override`` : Provide a power threshold override in watts for clock throttling to activate. The default threshold value is given in `<UG1120> <https://docs.xilinx.com/r/en-US/ug1120-alveo-platforms>`_.
    - ``throttling_temp_override`` : Provide a temperature threshold override in Celsius for clock throttling to activate. The default threshold value is given in `<UG1120> <https://docs.xilinx.com/r/en-US/ug1120-alveo-platforms>`_.

- If a pairing is not listed in the .ini file, the default value (or the updated value from previous usage of ``xbmgmt configure --input``) is used.
- Thresholds can be set higher or lower as necessary (e.g. debugging purposes). Note that cards still have built-in card and clock shutdown logic with independent thresholds to protect the cards.
- To check clock throttling settings, use ``xbmgmt examine`` with the ``cmc`` report.


Enabling/Disabling DDR memory retention on a device

.. code-block:: shell

    xbmgmt configure [--device| -d] <management bdf> --retention [ENABLE|DISABLE]


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device 
    
    - <management bdf> : The Bus:Device.Function of the device of interest


- The ``--input`` specifies an INI file with configuration details (e.g. memory, clock throttling).
- The ``--retention`` option enables / disables DDR memory retention.


**Example commands** 


.. code-block:: shell


    #Configure a device's memory settings using an image
    xbmgmt configure --device 0000:b3:00.0 --input /tmp/memory_config.ini

    #Configure a device using edited output .ini from xbmgmt dump --config
    xbmgmt configure --device 0000:b3:00.0 --input /tmp/config.ini

    #Enable a device's DDR memory retention
    xbmgmt configure --device 0000:b3:00.0 --retention ENABLE


xbmgmt dump
~~~~~~~~~~~

The ``xbmgmt dump`` command dumps out content of the specified option

**The supported options**

Dumping the output of system configuration.

.. code-block:: shell

    xbmgmt dump [--device| -d] <management bdf> [--config| -c] [--output| -o] <filename with .ini extension>
    

Dumping the output of programmed system image

.. code-block:: shell

    xbmgmt dump [--device| -d] <management bdf> [--flash| -f] [--output| -o] <filename with .bin extension>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device 
    
    - <management bdf> : The Bus:Device.Function of the device of interest


- The ``--flash`` (or ``-f``) option dumps the output of programmed system image. Requires a .bin output file by ``-o`` option.
- The ``--config`` (or ``-c``) option dumps the output of system configuration. Requires a .ini output file by ``-o`` option.
- The ``--output`` (or ``-o``) specifies the output file to direct the dumped output.
    

**Example commands** 


.. code-block:: shell

      
    #Dump programmed system image data
    xbmgmt dump --device 0000:b3:00.0 --flash -o /tmp/flash_dump.bin
    
    #Dump system configuration. This .ini file can be edited and used as input for xbmgmt configure.
    xbmgmt dump --device 0000:b3:00.0 --config -o /tmp/config_dump.ini

    #Example .ini file contents from xbmgmt dump --config.
    #Only edit the throttling_enabled, throttling_power_override, and throttling_temp_override values when editing clock throttling settings.
    [Device]
    mailbox_channel_disable=0x0
    mailbox_channel_switch=0x0
    xclbin_change=0
    cache_xclbin=0
    throttling_enabled=true
    throttling_power_override=200
    throttling_temp_override=90


xbmgmt examine
~~~~~~~~~~~~~~

The ``xbmgmt examine`` command reports detail status information of the specified device `<video reference> <https://youtu.be/ORYSrYegX_g?t=137>`_.

**The supported options**


.. code-block:: shell

    xbmgmt examine [--device| -d] <management bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -o] <filename>
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <management bdf> : The Bus:Device.Function of the device of interest

- The ``--report`` (or ``-r``) switch can be used to view specific report(s) of interest from the following options
          
    -  ``all``: All known reports are produced
    - ``firewall``: Firewall status
    - ``host``: Host information
    - ``mailbox``: Mailbox metrics of the device
    - ``mechanical``: Mechanical sensors on and surrounding the device
    - ``platform``: Platform information
    - ``cmc``: Reports cmc status of the device, such as clock throttling information

- The ``--format`` (or ``-f``) specifies the report format. Note that ``--format`` also needs an ``--output`` to dump the report in json format. If ``--output`` is missing text format will be shown in stdout
    
    - ``JSON``: The report is shown in latest JSON schema
    - ``JSON-2020.2``: The report is shown in JSON 2020.2 schema

- The ``--output`` (or ``-o``) specifies the output file to direct the output
    

**Example commands** 


.. code-block:: shell

      
    #Report all the information for a specific device
    xbmgmt examine --device 0000:d8:00.0 --report all
    
    #Reports platform information in JSON format
    xbmgmt examine --device 0000:b3:00.0 --report platform --format JSON --output output.json



xbmgmt program
~~~~~~~~~~~~~~

**The supported usecases and their options**

Program the Base partition (applicable for 1RP platform too) `<video reference> <https://youtu.be/ORYSrYegX_g?t=193>`_

.. code-block:: shell

    xbmgmt program [--device|-d] <management bdf> [--base|-b] 

Program the Base partition when multiple base partitions are installed in the system

.. code-block:: shell

    xbmgmt program [--device|-d] <management bdf> [--base|-b] [--image|-i] <partition name>

Program the Shell Partition for 2RP platform `<video reference> <https://youtu.be/ORYSrYegX_g?t=300>`_

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> [--shell|-s] <shell partition file with path>  


Program the user partition with an XCLBIN file

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> [--user|-u] <XCLBIN file with path>  


Revert to golden image `<video reference> <https://youtu.be/ORYSrYegX_g?t=280>`_

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> --revert-to-golden


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <management bdf> : The Bus:Device.Function of the device of interest
 
- The ``--base`` option is used to update the base partition. This option is applicable for both the 1RP and 2RP platforms. No action is performed if the card's existing base partition is already up-to-date, in a higher version, or a different platform's partition. The option ``--base`` only works if only one base partition package is also installed on the host system. In case of multiple base partitions are installed on the system an additional ``--image`` option is required (discussed next).   

- The ``--image`` option is used with the ``--base`` option if multiple base partitions are installed on the system. Multiple base partitions installed on the system can be viewed by executing the command ``xbmgmt examine --device <bdf> --report platform`` (shown under **Flashable partitions installed in system** section). The user then choose the desired base partition for programming the platform and execute the full command as ``xbmgmt program --device <bdf> --base --image <base partition name>``. 

- The ``--shell`` option is used to program shell partition, applicable for 2RP platform only. The user can get the full path of installed shell partition in the system from the json file generated by ``xbmgmt examine -r platform --format json --output <output>.json`` command 

    - <shell partition with path> : The shell partition with full path to program the shell partition

- The ``--user`` (or ``-u``) is required to specify the .xclbin file
    
    - <xclbin file> : The xclbin file with full-path to program the device
    
- The ``--revert-to-golden`` command is used to reverts the flash image back to the golden version of the card.	


**Example commands**


.. code-block:: shell
 
     #Program the base partition 
     xbmgmt program --device 0000:d8:00.0 --base
     
     
     #Program the base partition 
     xbmgmt program --device 0000:d8:00.0 --base --image xilinx-u250-gen3x16-base
     
     #Program the shell partition
     xbmgmt program --device 0000:d8:00.0 --shell <partition file with path>
 
     xbmgmt program --device 0000:d8:00.0 --revert-to-golden




xbmgmt reset
~~~~~~~~~~~~

The ``xbmgmt reset`` command can be used to reset device . 


**The supported options**

.. code-block:: shell

    xbmgmt reset [--device| -d] <management bdf> 


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to reset
    
    - <management bdf> : The Bus:Device.Function of the device of interest
    

**Example commands**


.. code-block:: shell
 
    xbmgmt reset --device 0000:65:00.0

