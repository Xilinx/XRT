.. _xbmgmt.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.

xbmgmt
======

This document describes the latest ``xbmgmt`` commands. These latest commands are default from 21.1 release.   


P.S: The older version of the commands can only be executed by adding ``--legacy`` switch. The documentation link of legacy version: `Vitis Application Acceleration Development Flow Documentation <https://www.xilinx.com/html_docs/xilinx2021_1/vitis_doc/Chunk778393017.html>`_


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


xbmgmt configure
~~~~~~~~~~~

The ``xbmgmt configure`` command provides advanced options for configuring a device

**The supported options**

Configuring a device's memory settings with a premade image

.. code-block:: shell

    xbmgmt dump [--device| -d] <management bdf> [--input] <filename with .ini extension>


Enabling/Disabling memory retention on a device

.. code-block:: shell

    xbmgmt configure [--device| -d] <management bdf> --retention [ENABLE|DISABLE]


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device 
    
    - <management bdf> : The Bus:Device.Function of the device of interest


- The ``--input`` specifies an INI file with the memory configuration.
- The ``--retention`` option enables / disables memory retention.


**Example commands** 


.. code-block:: shell


    #Configure a device's memory settings using an image
    xbmgmt configure --device 0000:b3:00.0 -o /tmp/memory_config.ini
    
    #Enable a device's memory retention 
    xbmgmt configure --device 0000:b3:00.0 --retention ENABLE


xbmgmt dump
~~~~~~~~~~~

The ``xbmgmt dump`` command dump out content of the specified option 

**The supported options**

Dumping the output of system configuration.

.. code-block:: shell

    xbmgmt dump [--device| -d] <management bdf> [--config| -c] [--output| -o] <filename>
    

Dumping the output of programmed system image

.. code-block:: shell

    xbmgmt dump [--device| -d] <management bdf> [--flash| -f] [--output| -o] <filename with .ini extension>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device 
    
    - <management bdf> : The Bus:Device.Function of the device of interest


- The ``--flash`` (or ``-f``) option dumps the output of programmed system image.
- The ``--config`` (or ``-c``) option dumps the output of system configuration.
- The ``--output`` (or ``-o``) specifies the output file to direct the dumped output. For ``--config`` the output file must have extension .ini
    

**Example commands** 


.. code-block:: shell

      
    #Dump programmed system image data
    xbmgmt dump --device 0000:b3:00.0 --flash -o /tmp/flash_dump.txt
    
    #Dump system configaration 
    xbmgmt dump --device 0000:b3:00.0 --config -o /tmp/config_dump.ini


xbmgmt examine
~~~~~~~~~~~~~~

The ``xbmgmt examine`` command reports detail status information of the specified device

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

Program the Base partition (applicable for 1RP platform too)

.. code-block:: shell

    xbmgmt program [--device|-d] <management bdf> [--base|-b] 

Program the Base partition when multiple base partitions are installed in the system

.. code-block:: shell

    xbmgmt program [--device|-d] <management bdf> [--base|-b] [--image|-i] <partition name>

Program the Shell Partition for 2RP platform

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> [--shell|-s] <shell partition file with path>  


Program the user partition with an XCLBIN file

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> [--user|-u] <XCLBIN file with path>  


Revert to golden image

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> --revert-to-golden


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <management bdf> : The Bus:Device.Function of the device of interest
 
- The ``--base`` option is used to update the base partition. This option is applicable for both the 1RP and 2RP platform. No action is performed if the card's existing base partition is already up-to-date, or in a higher version, or a different platform's partition. 

- The ``--image`` option is used with ``--base`` option if multiple base packages are installed in the system. The specific base partition can be specified by the name (or name with full-path)

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

The ``xbmgmt reset`` command can be used to reset device. 


**The supported options**

.. code-block:: shell

    xbmgmt reset [--device| -d] <management bdf> 


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to reset
    
    - <management bdf> : The Bus:Device.Function of the device of interest
    

**Example commands**


.. code-block:: shell
 
    xbmgmt reset --device 0000:65:00.0

