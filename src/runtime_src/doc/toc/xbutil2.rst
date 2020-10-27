.. _xbutil2.rst:

Xbutil Next Generation
======================

The next generation of the ``xbutil`` command-line tool is in preview mode for the 2020.2 release of XRT. This version will replace the current ``xbutil`` in a future release of XRT. This document describes the usage of this new version of the tool. 

To invoke the new version please set the following environment variable

.. code::

    export XRT_TOOLS_NEXTGEN=true



The xbutil command options are

    - ``xbutil program``
    - ``xbutil validate``
    - ``xbutil examine``
    - ``xbutil reset``
    - ``xbutil advanced`` 


**A note about multidevice system**: All the ``xbutil`` command supports a ``--device`` (or ``-d``) switch to specify the target device of interest. The ``xbutil`` command accept the PCIe user function bdf as an argument of ``--device`` switch. The user can check the user function bdf from ``xbutil examine`` command.

.. code:: 

    xbutil --new examine
    ....
    ....
    Devices present
    [0000:b3:00.1] : xilinx_u200_xdma_201830_2
    [0000:65:00.1] : xilinx_u50_gen3x16_xdma_201920_3


The above output shows two devices and their user function bdf (``0000:b3:00.1`` and ``0000:65:00.1``) can be used with the ``--device`` switch

xbutil program
~~~~~~~~~~~~~~

The ``xbutil program`` command downloads a specified xclbin binary to the programmable region on the card.

**The supported options**


.. code-block:: 

    xbutil program [--device|-d] <user bdf> [--program|-p] <xclbin>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to be programmed
    
    - <none> : Optional for a single device system. 
    - <user bdf>+ : Mandetory for multiple device system, has to be specified with one or more device user bdf information 
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used 
- The ``--program`` (or ``-p``) is required to specify the .xclbin file
    
    - <xclbin file> : The xclbin file to be used to program the device


**Example commands** 


.. code-block:: 

    #Programming a only available device with a xclbin 
    xbutil program --program my_kernel.xclbin
 
    #Multiple Devices, program all the devices
    xbutil program --device all --program my_kernel.xclbin
 
    #Multiple Device, programing a single device
    xbutil program --device 0000:03:00.1 --program my_kernel.xclbin
 
    #Multiple Device, programing two devices
    xbutil program --device 0000:03:00.1 0000:d8:00.1 --program my_kernel.xclbin


xbutil validate
~~~~~~~~~~~~~~~

The command ``xbutil validate`` validates the card installation by running precompiled basic tests. 

**The supported options**


.. code-block:: 

   xbutil validate [--device| -d] <user bdf> [--run| -r] <test> [--format| -f] <report format>
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to be validate 
    
    - <none> : Optional for a single device system. 
    - <user bdf>+ : Mandetory for multiple device system, has to be specified with one or more device user bdf information 
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--run`` (or ``-r``) specifies the perticular test to execute
        
    - ``all`` (**default**): runs all the tests listed below
    - ``Kernel version``: Check if the kernel version is supported by XRT
    - ``Aux connection``: Check if auxiliary power is connected
    - ``PCIE link``: Check if PCIE link is active
    - ``SC version``: Check if SC firmware is up-to-date
    - ``Verify kernel``: Run 'Hello World' kernel test
    - ``DMA``: Run dma test
    - ``Bandwidth kernel``: Run 'bandwidth kernel' and check the throughput
    - ``Peer to peer bar``: Run P2P test
    - ``Memory to memory DMA``: Run M2M test
    - ``Host memory bandwidth test``: Run 'bandwidth kernel' when slave bridge is enabled
    - ``quick``: Run first five tests (Kernel version, Aux connection, PCIE link, SC version and Verify kernel)   
  
- The ``--format`` (or ``-f``) specifies the report format
    
    - ``text`` (**default**): The report is shown in the text format, default behavior
    - ``json``: The report is shown in json-2020.2  


**Example commands**


.. code-block:: 

    # For a single device run all the tests 
    xbutil validate
 
    # For a multiple device system run all the tests on all the devices
    xbutil validate --device all
 
    # For a multiple device system run "DMA" program on a specific device
    xbutil valiadate --device 0000:d8:00.1 --run DMA
 
    # For a multiple device system run "DMA" and "Validate Kernel" program on two devices and generates Json format
    xbutil validate --device 0000:d8:00.1 0000:03:00.1 --run DMA "Verify Kernel" --format json-2021.2


xbutil examine 
~~~~~~~~~~~~~~

The command ``xbutil examine``  can be used to find the details of the specific device(s),


**The supported options**


.. code-block:: 

    xbutil examine [--device|-d] <user bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -o] <filename>
 


**The details of the supported options**


- The ``--device`` (or ``-d``) specifies the target device to be validate 
    
    - <none> : Optional for a single device system. 
    - <user bdf>+ : Mandetory for multiple device system, has to be specified with one or more device user bdf information 
    - ``all``:To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--report`` (or ``-r``) switch can be used to view specific report(s) of interest from the following options
          
    - ``scan`` (**default**): Shows System Configuration, XRT and Device user bdf information. 
    - ``aie``: Reports AIE kernels metadata from the .xclbin
    - ``electrical``: Reports  Electrical and power sensors present on the device
    - ``debug-ip-status``: Reports information related to Debug-IPs inserted during the kernel compilation
    - ``firewall``: Reports the current firewall status
    - ``host``: Reports the host configuration and drivers
    - ``fan``: Reports fans present on the device
    - ``memory``: Reports memory topology of the XCLBIN (if XCLBIN is already loaded) 
    - ``thermal``: Reports thermal sensors present on the device
    - ``verbose``: Reports everything
- The ``--format`` (or ``-f``) can be used to specify the output format
    
    - ``text`` (**default**): The output is shown in the text format, default behavior
    - ``json``: The output is shown in json-2020.2 

- The ``--output`` (or ``-o``) can be used to dump output in a file instead of stdout
        
    - <filename> : The output file to be dumped


**Example commands**


.. code-block:: 

    # Examine all the devices and produces all the reports
    xbutil examine
 
 
    # Examine a specific device and report electrical information in the stdout
    xbutil examine --device 0000:d8:00.0 --run electrical
 
    # Example a list of devices and reports a list of information and dump in a file json format
    xbutil examine --device 0000:d8:00.0 0000:d8:00.1 --run electrical firewall --format json --output my_reports.json
 
 
xbutil reset
~~~~~~~~~~~~
This ``xbutil reset`` command can be used to reset one or more devices. 

**The supported options**

.. code-block:: 

    xbutil reset [--device| -d] <user bdf> [--type| -t] <reset type>

**The details of the supported options**


- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <user bdf>+ : Mandetory, has to be specified with one or more device user bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--type`` (or ``-t``) can be used to specify the reset type. Currently supported reset type
    
    - ``hot`` (**default**): Complete reset of the device

**Example commands**


.. code-block::
 
    xbutil reset --device 0000:65:00.1
    
    xbutil reset --device 0000:65:00.1 --type hot
    


xbutil advanced
~~~~~~~~~~~~~~~

The ``xbutil advanced`` commands are the group of commands only recommended for the advanced users. 

As a disclaimer, the formats of these commands can change significantly as we know more about the advnced use-cases. 

**The supported options**

Read from Memory

.. code-block:: 

    xbutil advanced [--device| -d] <user bdf> --read-mem <address> <size> [--output] <output file>

Fill Memory with binary value

.. code-block:: 

    xbutil advanced [--device| -d] <user bdf> --write-mem <address> <size> [--fill] <binary data> 


Fill Memory from a file content

.. code-block:: 

    xbutil advanced [--device| -d] <user bdf> --write-mem <address> <size>  [--input] <file>


P2P Enable, disable or valiadte

.. code-block:: 

    xbutil advanced [--device| -d] <user bdf> --p2p [enable|disable|validate]



**The details of the supported options**


- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <user bdf>+ : Mandetory, has to be specified with one or more device user bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--read-mem`` is used to read from perticular memory location. It has to use with following arguments
    
    - <address> <number of bytes> : The read location and the size of the read. 
- The ``--output`` can be used with ``--read-mem`` to dump the read data to a file instead of console
    
    - <filename> : When specified the output of ``--read-mem`` commands are dumped into the user provided file
- The ``--write-mem`` is used to write to the perticular memory location. It has to use with following arguments
    
    - <address> <number of bytes> : The write location and the size of the write. 
- The ``--fill`` can be used with ``--write-mem`` to fill the memory location with a perticular binary value
        
    - <uint8> : The filled value in byte
- The ``--input`` can be used with ``--write-mem`` to write the memory location from a file content
        
    - <binary file> : The binary file 
- The ``--p2p`` can be used to enable, disable or validate p2p operation

    - enable: Enable the p2p
    - disable: Disable the p2p
    - validate: Validate the p2p
        

**Example commands**


.. code-block::
 
    xbutil advanced --device 0000:65:00.1 --read-mem 0x100 0x30
    
    xbutil advanced --device 0000:65:00.1 --read-mem 0x100 0x30 --output foo.bin
    
    xbutil advanced --device 0000:65:00.1 --write-mem 0x100 0x10 --fill 0xAA
    
    xbutil advanced --device 0000:65:00.1 --write-mem 0x100 0x20 --input foo.bin
    
    xbutil advanced --device 0000:65:00.1 --p2p enable
    
    xbutil advanced --device 0000:65:00.1 --p2p disble
    
    xbutil advanced --device 0000:65:00.1 --p2p validate
    
    
    



