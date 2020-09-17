.. _xbmgmt2.rst:

XBMGMT (Next Generation Xilinx Board Management Tool)
=====================================================

The xbmgmt command options are

    - xbmgmt program
    - xbmgmt status
    - xbmgmt reset

xbutil program
~~~~~~~~~~~~~~

**The supported options**


.. code-block:: 

    xbmgmt program --device [BDF] --partition [partition file| uuid] --update [specific image] --image [specific image path] 
 
    xbutil program -d [BDF] -p [partition file| uuid] -u [specific image] --i [specific image path] 
    
    xbutil program --revert-to-golden


**The details of the supported options**


     - The --device (or -d) switch ihas to use by specific BDF
     - The --partition option can optionally be used to program specific partition. This command supports partition specification by full path of the partition file, or UUID
     - The --update option can optionally be used to specify the specific image to be programmed. Supported options are 
     - sc: Satellite controller image 
     - flash: Flash image 
     - The --image option to specify  


**Example commands**


.. code-block::
 
     xbmgmt program --d 0000:d8:00.0 --p ./my_partition.xsabin
 
     xbmgmt program --d 0000:d8:00.0 -u SC --image [specific image path] --revert-to-golden
 
     xbmgmt program -d [BDF] -p [partition file| uuid] -u [specific image] --i [specific image path] --revert-to-golden


xbmgmt status
~~~~~~~~~~~~~

**The supported options**


.. code-block::

    xbmgmt status --device [BDF] --report [report of interest] --format [output file format] --output [output file name]
 
    xbmgmt status -d [BDF] -r [report of interest] -f [output file format] --o [output file name]


**The details of the supported options**

    - The --device (or -d) switch can be specified to select the specific device of interest. Otherwise status of all the devices are reported
    - The --report (or -r) switch is optional, by default the device scanning information is provided, supported other options 
    
        - verbose: Reports all
        - platform: Reports platform related information
        - scan: Report device scanning information
        
    - The --output (or -o) is optional, if not specified the report is shown in stdout. 


**Example commands** 


.. code-block:: 

    #Reports Scanning of all the devices
    xbmgmt status 
    
    #Report all the information for a specific device
    xbmgmt status --d 0000:d8:00.0 -r verbose
    
    #Reports platform information of two devices and dump to a file
    xbmgmt status -d 0000:b3:00.0 0000:65:00.0 =report platform -f json -o output output.json


xbmgmt reset
~~~~~~~~~~~~

**The supported options**

.. code-block:: 

    xbutil reset --device [BDF] --type [Reset type]
    xbutil reset -d [BDF] -t [Reset type]


**The details of the supported options**


    - The --device (or -d) used to specify the device to be reset
    - The --type (or -t) can be used to specify the reset type. Currently only supported reset types are
      
         - hot (default): Reset the entire device
         - kernel: Reset the kernel communication link
         - ert: Reset the management processor
         - ecc: Reset ecc memory
         - soft-kernel: Reset soft kernel
         
    

**Example commands** 


.. code-block::
 
    # Reset a single device entirely (default hot reset)
    xbutil reset -d 0000:65:00.1
    
    # Reset kernel communication link of two devices
    xbutil reset -d 0000:65:00.1 0000:65:00.1 -t kernel


