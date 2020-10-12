.. _xbmgmt2.rst:

Xbmgmt Next Generation
======================

The next generation of the ``xbmgmt`` command-line tool is in preview mode for the 2020.2 release of XRT. This version will replace the current ``xbmgmt`` in a future release of XRT. This document describes the usage of this new version of the tool.


The xbmgmt command options are

    - ``xbmgmt program``
    - ``xbmgmt status``
    - ``xbmgmt reset``

xbmgmt program
~~~~~~~~~~~~~~

**The supported options**

Update the Base partition 

.. code-block:: 

    xbmgmt program [--device|-d] <bdf> [--update|-u] [--image|-i] <image path with path>


Program a Shell Partition

.. code-block:: 

    xbmgmt program [--device| -d] <bdf> [--partition|-p] <partition file with path>  


Revert to golden image
.. code-block:: 

    xbmgmt program --revert-to-golden


**The details of the supported options**

- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <bdf>+ : Mandetory, has to be specified with one or more device bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--update`` option is used to update the base partition. This option is applicable for both the 1RP and 2RP platform. 
- The ``--image`` option can be optionally used with ``--update``. If there are multiple base partition images exist in the system, the correct base partition has be specified through ``--image`` switch
  
    - <image name with path> : 

- The ``--partition`` option is used to program shell partition, applicable for 2RP platform only.
    
    - <partiton file with path>: 
- The ``--revert-to-golden`` command is used to reverts the flash image back to the golden version of the card.	


**Example commands**


.. code-block::
 
     #Update the base partition 
     xbmgmt program --d 0000:d8:00.0 --update 
     
     #Update the base partition when multiple base partition images exist in the system
     xbmgmt program --d 0000:d8:00.0 --update --image <image full path>/<image file name>
 
     xbmgmt program --d 0000:d8:00.0 -u SC --image [specific image path] --revert-to-golden
 
     xbmgmt program -d [BDF] -p [partition file| uuid] -u [specific image] --i [specific image path] --revert-to-golden


xbmgmt status
~~~~~~~~~~~~~

The ``xbmgmt status`` command reports detail status information of the specified device

**The supported options**


.. code-block::

    xbmgmt status [--device| -d] <bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -u] <filename>
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to be validate 
    
    - <none> : Optional for a single device system. 
    - <bdf>+ : Mandetory for multiple device system, has to be specified with one or more device bdf information 
    - ``all``:To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--report`` (or ``-r``) switch is optional, by default the device scanning information is provided, supported other options 
  
    - ``scan`` (**default**): scan option shows System Configuration, XRT and Device BDF information. 
    - ``platform``: Reports platform related informati      
    - ``verbose``: Reports all
    
- The ``--format`` (or ``-f``) can be used to specify the output format
    
    - ``text`` (**default**): The output is shown in the text format, default behavior
    - ``json-2021.2``: The output is shown in json-2021.2 
- The ``--output`` (or ``-o``) can be used to dump output in a file instead of stdout
        
    - <filename> : The output file to be dumped


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

This ``xbmgmt reset`` command can be used to reset one or more devices. 


**The supported options**

.. code-block:: 

    xbmgmt reset [--device| -d] <bdf> [--type| -t] <reset type>


**The details of the supported options**

- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <bdf>+ : Mandetory, has to be specified with one or more device bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--type`` (or ``-t``) can be used to specify the reset type. Currently supported reset type
    
    - ``hot`` (**default**): Complete reset of the device
    - ``kernel``: Reset the kernel communication link
    - ``ert``: Reset the management processor
    - ``ecc``: Reset ecc memory
    - ``soft-kernel``: Reset soft kernel
         
    

**Example commands** 


.. code-block::
 
    # Reset a single device entirely (default hot reset)
    xbmgmt reset -d 0000:65:00.1
    
    # Reset kernel communication link of two devices
    xbmgmt reset -d 0000:65:00.1 0000:65:00.1 -t kernel


