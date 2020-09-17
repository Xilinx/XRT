.. _xbutil2.rst:

XBUTIL (Next Generation Xilinx Board Utility Tool)
==================================================

The xbutil command options are

    - xbutil program
    - xbutil validate
    - xbutil examine
    - xbutil reset


xbutil program
~~~~~~~~~~~~~~

The **xbutil program** command downloads a specified xclbin binary to the programmable region on the card.

**The supported options**


.. code-block:: 

    xbutil program --device [BDF] --program [xclbin file]
    xbutil program -d [BDF] -p [xclbin file]


**The details of the supported options**


    - The --device (or -d) specifies the target device to be programmed
    
         - Optional for a single device system. 
         - Mandetory for multiple device system, has to be specified with one or more device BDF information 
         - To specify all devices –device all  or -d all  can be used 
    - The --program (or -p) is required to specify the .xclbin file


**Example commands** 


.. code-block:: 

    # Single Device
    xbutil program --p my_kernel.xclbin
 
    #Multiple Devices, program all the devices
    xbutil -program -d all --p my_kernel.xclbin
 
    #Multiple Device, programing a single device
    xbutil program --d 0000:d8:00.0 --p my_kernel.xclbin
 
    #Multiple Device, programing two devices
    xbutil program --d 0000:d8:00.0 0000:d8:00.1 --p my_kernel.xclbin


xbutil validate
~~~~~~~~~~~~~~~

The command **xbutil validate** validates the card installation by running precompiled basic examples. 

**The supported options**


.. code-block:: 

   # Single Device
   xbutil validate --device [BDF] --run [program] --format [output file format]
 
   xbutil validate --d [BDF] --r [program] --f [output file format]

**The details of the supported options**


    - The --device or -d switch must be specified with BDF
    - The --run or -r switch is optional, if not specified all the program is run
    
        - The user can select the desired program by this option
    - The --format or -f switch is optional, if not specified a text file is generated


**Example commands**


.. code-block:: 

    # For a single device run all the program
    xbutil valiadte
 
    # For a multiple device system run all the program
    xbutil valiadte --d all
 
    # For a multiple device system run "DMA" program
    xbutil valiadte --d 0000:d8:00.0 --run DMA
 
    # For a multiple device system run "DMA" and "Validate Kernel" program for two devices and generates Json format
    xbutil valiadte --d 0000:d8:00.0 0000:d8:00.1 --run DMA "Verify Kernel" -f json-2021.2


xbutil examine 
~~~~~~~~~~~~~~

The command **xbutil examine**  can be used to find the details of the specific device(s),


**The supported options**


.. code-block:: 

    # Single Device
    xbutil examine --device [BDF] --report [Report of interest] --format [o/p file format] --output [o/p file]
 
    xbutil examine --d [BDF] --r [Report of interest] --f [o/p file format] --o [o/p file]


**The details of the supported options**


    - The --device (or -d) switch is optional, when not specified the details of all the cards are shown 
    - The --report (or -r) switch is optional, if not specified all possible reports are generated. The user can select specific report(s) of interest from the following options
          
          - verbose: Reports everything, default
          - aie: Reports information related to AIE kernels
          - electrical: Reports information related to Volate, current and Power
          - debug-ip-status: Reports information related to Debug IP inserted during the kernel compilation
          - firewall: Reports the current firewall status
          - host: Reports the host configuration and drivers
          - mechanical: 
          - scan
          - thermals: Report thermal 
    - The --format or -f switch is optional, if not specified a text file is generated. Other supported formal is json-2021.2
    - The --output (or -o) is optional, if not specified the report is shown in stdout. 


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
This command can be used to reset one or more devices. 

**The supported options**

.. code-block:: 

    xbutil reset --device [BDF] --type [Reset type]
    xbutil reset -d [BDF] -t [Reset type]

**The details of the supported options**


    - The --device (or -d) used to specify the device to be reset
    - The --type (or -t) can be used to specify the reset type. Currently supported reset type
    
         - hot: A hot reset (default)

**Example commands**


.. code-block::
 
    xbutil reset -d 0000:65:00.1
    
    xbutil reset -d 0000:65:00.1 -t hot
    
