.. _xbutil.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.


xbutil
======

This document describes the latest ``xbutil`` commands. These latest commands are default from 21.1 release.   


P.S: The older version of the commands can only be executed by adding ``--legacy`` switch.


**Global options**: These are the global options can be used with any command. 

 - ``--verbose``: Turn on verbosity and shows more outputs whenever applicable
 - ``--batch``: Enable batch mode
 - ``--force``: When possible, force an operation
 - ``--help`` : Get help message
 - ``--version`` : Report the version of XRT and its drivers

Currently supported ``xbutil`` commands are

    - ``xbutil program``
    - ``xbutil validate``
    - ``xbutil examine``
    - ``xbutil configure``
    - ``xbutil reset``


xbutil program
~~~~~~~~~~~~~~

The ``xbutil program`` command downloads a specified xclbin binary to the programmable region on the card.

**The supported options**


.. code-block:: shell

    xbutil program [--device|-d] <user bdf> [--user|-u] <xclbin>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <user bdf> :  The Bus:Device.Function of the device of interest
    
- The ``--user`` (or ``-u``) is required to specify the .xclbin file
    
    - <xclbin file> : The xclbin file with full-path to program the device


**Example commands** 


.. code-block:: shell

     xbutil program --device 0000:b3:00.1 --user ./my_kernel.xclbin
 

xbutil validate
~~~~~~~~~~~~~~~

The command ``xbutil validate`` validates the installed card by running precompiled basic tests. 

**The supported options**


.. code-block:: shell

   xbutil validate [--device| -d] <user bdf> [--run| -r] <test> [--format| -f] <report format> [--output| -o] <filename>
 
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to validate 
    
    - <user bdf> :  The Bus:Device.Function of the device of interest

- The ``--run`` (or ``-r``) specifies the perticular test(s) to execute
        
    - ``all`` (**default**): runs all the tests listed below
    - ``Aux connection``: Check if auxiliary power is connected
    - ``PCIE link``: Check if PCIE link is active
    - ``SC version``: Check if SC firmware is up-to-date
    - ``Verify kernel``: Run 'Hello World' kernel test
    - ``DMA``: Run dma test
    - ``iops``: Run test to measure performance of scheduler (number of `hello world` kernel execution per second)
    - ``Bandwidth kernel``: Run 'bandwidth kernel' and check the throughput
    - ``Peer to peer bar``: Run peer-to-peer test
    - ``Memory to memory DMA``: Run zero copy memory to memory data transfer test
    - ``Host memory bandwidth test``: Run 'bandwidth kernel' when host memory is enabled
    - ``bist``: Run BIST test
    - ``vcu``: Run decoder test (only applicable for specific platform). 
    - ``quick``: Run first four tests (Aux connection, PCIE link, SC version and Verify kernel)   
  
- The ``--format`` (or ``-f``) specifies the report format. Note that ``--format`` also needs an ``--output`` to dump the report in json format. If ``--output`` is missing text format will be shown in stdout
    
    - ``JSON``: The report is shown in latest JSON schema
    - ``JSON-2020.2``: The report is shown in JSON 2020.2 schema
    
- The ``--output`` (or ``-o``) specifies the output file to direct the output


**Example commands**


.. code-block:: shell

    # Run all the tests 
    xbutil validate --device 0000:b3:00.1
 
    # Run "DMA" test, produce text output in stdout
    xbutil validate --device 0000:b3:00.1 --run DMA
 
    # Run "DMA" and "Validate Kernel" test and generates Json format
    xbutil validate --device 0000:b3:00.1 --run DMA "Verify Kernel" --format JSON --output xyz.json


xbutil examine 
~~~~~~~~~~~~~~

The command ``xbutil examine``  can be used to find the details of the specific device,


**The supported options**


.. code-block:: shell

    xbutil examine [--device|-d] <user bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -o] <filename>
 


**The details of the supported options**


- The ``--device`` (or ``-d``) specifies the target device to examine 
    
    - <user bdf> :  The Bus:Device.Function of the device of interest
- The ``--report`` (or ``-r``) switch can be used to view specific report(s) of interest from the following options
          
    - ``aie``: Reports AIE kernels metadata from the .xclbin
    - ``aieshim``: Reports AIE shim tile status
    - ``all``: All known reports are generated
    - ``debug-ip-status``: Reports information related to Debug-IPs inserted during the kernel compilation
    - ``dynamic-regions``: Information about the xclbin and the compute units (default when ``--device`` is provided)
    - ``electrical``: Reports  Electrical and power sensors present on the device
    - ``error``: Asyncronus Error present on the device
    - ``firewall``: Reports the current firewall status
    - ``host``: Reports the host configuration and drivers (default when ``--device`` is not provided)
    - ``mailbox``: Mailbox metrics of the device
    - ``mechanical``: Mechanical sensors on and surrounding the device
    - ``memory``: Reports memory topology of the XCLBIN (if XCLBIN is already loaded) 
    - ``pcie-info`` : Pcie information of the device
    - ``platform``: Platforms flashed on the device (default when ``--device`` is provided)
    - ``qspi-status``: QSPI write protection status
    - ``thermal``: Reports thermal sensors present on the device
    - ``cmc-status``: Reports cmc status of the device

- The ``--format`` (or ``-f``) specifies the report format. Note that ``--format`` also needs an ``--output`` to dump the report in json format. If ``--output`` is missing text format will be shown in stdout
    
    - ``JSON``: The report is shown in latest JSON schema
    - ``JSON-2020.2``: The report is shown in JSON 2020.2 schema

- The ``--output`` (or ``-o``) specifies the output file to direct the output



**Example commands**


.. code-block:: shell

    # Shows ``xbutil examine --host``
    xbutil examine
 
    # Reports electrical information in the stdout
    xbutil examine --device 0000:b3:00.1 --report electrical
 
    # Reports "electrical" and "firewall" and dump in json format
    xbutil examine --device 0000:b3:00.1  --report electrical firewall --format JSON --output n.json

 
 
xbutil reset
~~~~~~~~~~~~
This ``xbutil reset`` command can be used to reset device. 

**The supported options**

.. code-block:: shell

    xbutil reset [--device| -d] <user bdf> [--type| -t] <reset type>

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to reset 
    
    - <user bdf> :  The Bus:Device.Function of the device of interest
    
- The ``--type`` (or ``-t``) can be used to specify the reset type. Currently only supported reset type is
    
    - ``hot`` (**default**): Complete reset of the device

**Example commands**


.. code-block:: shell
 
    xbutil reset --device 0000:65:00.1

