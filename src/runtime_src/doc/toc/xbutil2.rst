.. _xbutil2.rst:

XBUTIL Next
===========

The next generation version of ``xbutil`` tool is in preview mode for 2020.2 release. This tool will be in production in future release replacing the current ``xbutil`` tool. This document describes various commands and usage of this new version of the tool. 

The xbutil command options are

    - ``xbutil program``
    - ``xbutil validate``
    - ``xbutil examine``
    - ``xbutil reset``
    - ``xbutil advanced`` 


xbutil program
~~~~~~~~~~~~~~

The ``xbutil program`` command downloads a specified xclbin binary to the programmable region on the card.

**The supported options**


.. code-block:: 

    xbutil program [--device|-d] <bdf> [--program|-p] <xclbin>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to be programmed
    
    - <none> : Optional for a single device system. 
    - <bdf>+ : Mandetory for multiple device system, has to be specified with one or more device BDF information 
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used 
- The ``--program`` (or ``-p``) is required to specify the .xclbin file
    
    - <xclbin file> : The xclbin file to be used to program the device


**Example commands** 


.. code-block:: 

    #Programming a only available device with a xclbin 
    xbutil program -p my_kernel.xclbin
 
    #Multiple Devices, program all the devices
    xbutil program -d all -p my_kernel.xclbin
 
    #Multiple Device, programing a single device
    xbutil program -d 0000:03:00.1 --p my_kernel.xclbin
 
    #Multiple Device, programing two devices
    xbutil program -d 0000:03:00.1 0000:d8:00.1 --p my_kernel.xclbin


xbutil validate
~~~~~~~~~~~~~~~

The command ``xbutil validate`` validates the card installation by running precompiled basic examples. 

**The supported options**


.. code-block:: 

   # Single Device
   xbutil validate [--device| -d] <bdf> [--run| -r] <test> [--format| -f] <report format>
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to be validate 
    
    - <none> : Optional for a single device system. 
    - <bdf>+ : Mandetory for multiple device system, has to be specified with one or more device BDF information 
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--run`` (or ``-r``) specifies the perticular test to execute
        
    - ``all`` (**default**): runs all the tests
    - ``DMA``: runs DMA test
    - ``Verify kernel``: Runs simple kernel test
- The ``--format`` (or ``-f``) specifies the report format
    
    - ``text`` (**default**): The report is shown in the text format, default behavior
    - ``json-2021.2``: The report is shown in json-2021.2 


**Example commands**


.. code-block:: 

    # For a single device run all the tests 
    xbutil validate
 
    # For a multiple device system run all the tests on all the devices
    xbutil validate --d all
 
    # For a multiple device system run "DMA" program
    xbutil valiadate --d 0000:d8:00.0 --run DMA
 
    # For a multiple device system run "DMA" and "Validate Kernel" program for two devices and generates Json format
    xbutil validate --d 0000:d8:00.0 0000:d8:00.1 --run DMA "Verify Kernel" -f json-2021.2


xbutil examine 
~~~~~~~~~~~~~~

The command ``xbutil examine``  can be used to find the details of the specific device(s),


**The supported options**


.. code-block:: 

    # Single Device
    xbutil examine [--device|-d] <bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -o] <filename>
 


**The details of the supported options**


- The ``--device`` (or ``-d``) specifies the target device to be validate 
    
    - <none> : Optional for a single device system. 
    - <bdf>+ : Mandetory for multiple device system, has to be specified with one or more device bdf information 
    - ``all``:To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--report`` (or ``-r``) switch can be used to view specific report(s) of interest from the following options
          
    - ``scan`` (**default**): scan option shows System Configuration, XRT and Device BDF information. 
    - ``aie``: Reports information related to AIE kernels
    - ``electrical``: Reports information related to Volate, current and Power
    - ``debug-ip-status``: Reports information related to Debug IP inserted during the kernel compilation
    - ``firewall``: Reports the current firewall status
    - ``host``: Reports the host configuration and drivers
    - ``mechanical``: 
    - ``memory``: Report memory information 
    - ``thermals``: Report thermal 
    - ``verbose``: Reports everything
- The ``--format`` (or ``-f``) can be used to specify the output format
    
    - ``text`` (**default**): The output is shown in the text format, default behavior
    - ``json-2021.2``: The output is shown in json-2021.2 

- The ``--output`` (or ``-o``) can be used to dump output in a file instead of stdout
        
    - <filename> : The output file to be dumped


**Example commands**


.. code-block:: 

    # Examine all the devices and produces all the reports
    xbutil examine
 
 
    # Examine a specific device and report electrical information in the stdout
    xbutil examine --d 0000:d8:00.0 --r electrical
 
    # Example a list of devices and reports a list of information and dump in a file Json format
    xbutil examine --d 0000:d8:00.0 0000:d8:00.1 --r electrical firewall -f json-2021.2 -o my_reports.json
 
 
xbutil reset
~~~~~~~~~~~~
This ``xbutil reset`` command can be used to reset one or more devices. 

**The supported options**

.. code-block:: 

    xbutil reset [--device| -d] <bdf> [--type| -t] <reset type>

**The details of the supported options**


- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <bdf>+ : Mandetory, has to be specified with one or more device bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--type`` (or ``-t``) can be used to specify the reset type. Currently supported reset type
    
    - ``hot`` (**default**): Complete reset of the device

**Example commands**


.. code-block::
 
    xbutil reset -d 0000:65:00.1
    
    xbutil reset -d 0000:65:00.1 -t hot
    


xbutil advanced
~~~~~~~~~~~~~~~

The ``xbutil advanced`` commands are the group of commands only recommended for the advanced users. 

As a disclaimer, the formats of these commands can change significantly as we know more about the advnced use-cases. 

**The supported options**

Read from Memory

.. code-block:: 

    xbutil advanced [--device| -d] <bdf> --read-mem <address> <size> [--output] <output file>

Fill Memory with binary value

.. code-block:: 

    xbutil advanced [--device| -d] <bdf> --write-mem <address> <size> [--fill] <binary data> 


Fill Memory from a file content

.. code-block:: 

    xbutil advanced [--device| -d] <bdf> --write-mem <address> <size>  [--input] <file>


P2P Enable, disable or valiadte

.. code-block:: 

    xbutil advanced [--device| -d] <bdf> --p2p [enable|disable|validate]



**The details of the supported options**


- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <bdf>+ : Mandetory, has to be specified with one or more device bdf  
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
 
    xbutil advanced -d 0000:65:00.1 --read-mem 0x100 0x30
    
    xbutil advanced -d 0000:65:00.1 --read-mem 0x100 0x30 --output foo.bin
    
    xbutil advanced -d 0000:65:00.1 --write-mem 0x100 0x10 --fill 0xAA
    
    xbutil advanced -d 0000:65:00.1 --write-mem 0x100 0x20 --input foo.bin
    
    xbutil advanced -d 0000:65:00.1 --p2p enable
    
    xbutil advanced -d 0000:65:00.1 --p2p disble
    
    xbutil advanced -d 0000:65:00.1 --p2p validate
    
    
    



