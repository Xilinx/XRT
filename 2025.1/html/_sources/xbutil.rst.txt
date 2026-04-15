.. _xbutil.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
   comment:: Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.


xbutil
======

This document describes the latest ``xbutil`` commands. These latest commands are default from 21.1 release.  


For an instructive video on xbutil commands listed below click `here <https://www.youtube.com/watch?v=nvU2ZBnAaz4>`_.


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

**Note**: For applicable commands, if only one device is present on the system ``--device`` (or ``-d``) is not required. If more than one device is present in the system, ``--device`` (or ``-d``) is required.


xbutil program
~~~~~~~~~~~~~~

The ``xbutil program`` command downloads a specified xclbin binary to the programmable region on the card `<video reference> <https://youtu.be/nvU2ZBnAaz4?t=245>`_.

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

The command ``xbutil validate`` validates the installed card by running precompiled basic tests `<video reference> <https://youtu.be/nvU2ZBnAaz4?t=110>`_.

**The supported options**


.. code-block:: shell

   xbutil validate [--device| -d] <user bdf> [--run| -r] <test> [--format| -f] <report format> [--output| -o] <filename> [--param] <test>:<key>:<value>
 
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to validate 
    
    - <user bdf> :  The Bus:Device.Function of the device of interest

- The ``--run`` (or ``-r``) specifies the perticular test(s) to execute
        
    - ``all`` (**default**): runs all the tests listed below
    - ``aux-connection``: Check if auxiliary power is connected
    - ``pcie-link``: Check if PCIE link is active
    - ``sc-version``: Check if SC firmware is up-to-date
    - ``verify``: Run 'Hello World' kernel test
    - ``dma``: Run dma test
    - ``iops``: Run test to measure performance of scheduler (number of `hello world` kernel execution per second)
    - ``mem-bw``: Run 'bandwidth kernel' and check the throughput
    - ``p2p``: Run peer-to-peer test
    - ``m2m``: Run zero copy memory to memory data transfer test
    - ``hostmem-bw``: Run 'bandwidth kernel' when host memory is enabled
    - ``bist``: Run BIST test
    - ``vcu``: Run decoder test (only applicable for specific platform).
    - ``quick``: Run first four tests (Aux connection, PCIE link, SC version and Verify kernel)
    - ``aie-pl``: Run AIE PL test
  
- The ``--format`` (or ``-f``) specifies the report format. Note that ``--format`` also needs an ``--output`` to dump the report in json format. If ``--output`` is missing text format will be shown in stdout
    
    - ``JSON``: The report is shown in latest JSON schema
    - ``JSON-2020.2``: The report is shown in JSON 2020.2 schema
    
- The ``--output`` (or ``-o``) specifies the output file to direct the output

- The ``--param`` specifies the extended parameters that can be passed to a test. Valid values:
        
    - ``test``: dma
    - ``key``: block-size
    - ``value``: value in bytes


**Example commands**


.. code-block:: shell

    # Run all the tests 
    xbutil validate --device 0000:b3:00.1
 
    # Run "DMA" test, produce text output in stdout
    xbutil validate --device 0000:b3:00.1 --run DMA
 
    # Run "DMA" and "Validate Kernel" test and generates Json format
    xbutil validate --device 0000:b3:00.1 --run DMA "Verify Kernel" --format JSON --output xyz.json

    # Pass in a custom block size to dma test
    xbutil validate --device 0000:b3:00.1 --run DMA --param dma:block-size:1024


xbutil examine 
~~~~~~~~~~~~~~

The command ``xbutil examine``  can be used to find the details of the specific device `<video reference> <https://youtu.be/nvU2ZBnAaz4?t=80>`_.


**The supported options**


.. code-block:: shell

    xbutil examine [--device|-d] <user bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -o] <filename>
 


**The details of the supported options**


- The ``--device`` (or ``-d``) specifies the target device to examine 
    
    - <user bdf> :  The Bus:Device.Function of the device of interest

.. _xbutil_report_label:

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
    - ``telemetry``: Telemetry data for the device
    - ``thermal``: Reports thermal sensors present on the device

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

 
 
xbutil configure
~~~~~~~~~~~~~~~~
Command ``xbutil configure`` is used to configure specific settings based on the need of user application (requires sudo) `<video reference> <https://youtu.be/nvU2ZBnAaz4?t=280>`_.


**The supported options**

.. code-block:: shell 

    xbutil configure [ --host-mem | --p2p | --performance ] [--help]


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to examine 
    
    - <user bdf> :  The Bus:Device.Function of the device of interest
- The ``--host-mem`` or ``--p2p`` select specific configuration 
    
    - ``enable``: Enable the host-memory or p2p
    - ``disable``: Disable the host-memory or p2p
- The ``--size`` is used in conjuction with ``xbutil configure --host-mem enable`` to specify the host-memory size to be enabled
    
    - ``<size>``: Size and unit specified as a combined string 
- The ``--performance`` select specific configuration for benchmarking tests
    
    - ``low``: Set performance mode to low
    - ``medium``: Set performance mode to medium
    - ``high``: Set performance mode to high
    - ``default``: Set performance mode to default
    
 

**Example commands**


.. code-block:: shell

    # Enable Host-Memory of Size 1 GB
    sudo xbutil configure --device 0000:b3:00.1 --host-mem enable --size 1G 
    
    # Enable Host-Memory of size 256 MB
    sudo xbutil configure --device 0000:b3:00.1 --host-mem enable --size 256M
    
    # Disable previously enabled Host-Memory
    sudo xbutil configure --device 0000:b3:00.1 --host-mem disable
    
    # Enable P2P
    sudo xbutil configure --device 0000:b3:00.1 --p2p enable
 
    # Disable P2P
    sudo xbutil configure --device 0000:b3:00.1 --p2p disable
 


xbutil reset
~~~~~~~~~~~~
This ``xbutil reset`` command can be used to reset device `<video reference> <https://youtu.be/nvU2ZBnAaz4?t=350>`_.

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

