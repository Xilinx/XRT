.. _xbmgmt2.rst:

Xbmgmt Next Generation
======================

The next generation of the ``xbmgmt`` command-line tool is in preview mode for the 2020.2 release of XRT. This version will replace the current ``xbmgmt`` in a future release of XRT. This document describes the usage of this new version of the tool.

To invoke the new version please set the following environment variable

.. code::

    export XRT_TOOLS_NEXTGEN=true


The xbmgmt command options are

    - ``xbmgmt program``
    - ``xbmgmt examine``
    - ``xbmgmt reset``

**A note about multidevice system**: All the ``xbmgmt`` command supports a ``--device`` (or ``-d``) switch to specify the target device of interest. The ``xbmgmt`` commands accept the PCIe management function bdf as an argument of ``--device`` switch. The user can check the management function bdf from ``xbmgmt examine`` command.

.. code:: 

    xbmgmt --new examine
    
    Device : [0000:b3:00.0]
    ....
    ....
    Device : [0000:65:00.0]


The above output showing management function bdfs of the two devices (``0000:b3:00.0`` and ``0000:65:00.0``) can be used with ``--device`` switch.



xbmgmt program
~~~~~~~~~~~~~~

**The supported usecases with options**

Update the Base partition (applicable for 1RP platform too)

.. code-block:: 

    xbmgmt program [--device|-d] <management bdf> --update [--force|-f]


Program a Shell Partition for 2RP platform

.. code-block:: 

    xbmgmt program [--device| -d] <management bdf> --partition <partition file with path>  


Revert to golden image

.. code-block:: 

    xbmgmt program --revert-to-golden


**The details of the supported options**

- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <management bdf>+ : Mandetory, has to be specified with one or more device management bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--update`` option is used to update the base partition. This option is applicable for both the 1RP and 2RP platform. No action is performed if the card's existing base partition is already up-to-date, or in a higher version, or a different platform's partition. 
- The ``--force`` option can be used with ``--update`` to update the base partition forcefully for the above cases when it is not updated by itself. 
- The ``--partition`` option is used to program shell partition, applicable for 2RP platform only.
    
    - <partiton file with path>: 
- The ``--revert-to-golden`` command is used to reverts the flash image back to the golden version of the card.	


**Example commands**


.. code-block::
 
     #Update the base partition 
     xbmgmt program --device 0000:d8:00.0 --update 
     
     #Program the shell partition
     xbmgmt program --device 0000:d8:00.0 --partition <partition file with path>
 
     xbmgmt program --device 0000:d8:00.0 --revert-to-golden


xbmgmt examine
~~~~~~~~~~~~~~

The ``xbmgmt examine`` command reports detail status information of the specified device

**The supported options**


.. code-block::

    xbmgmt examine [--device| -d] <management bdf> [--report| -r] <report of interest> [--format| -f] <report format> [--output| -u] <filename>
 

**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to be validate 
    
    - <none> : Optional for a single device system. 
    - <management bdf>+ : Mandetory for multiple device system, has to be specified with one or more device management bdf information 
    - ``all``:To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--report`` (or ``-r``) switch is optional, by default the device scanning information is provided, supported other options 
  
    - ``scan`` (**default**): scan option shows System Configuration, XRT and Device management bdf information. 
    - ``platform``: Reports platform related informati      
    - ``verbose``: Reports all
    
- The ``--format`` (or ``-f``) can be used to specify the output format
    
    - ``text`` (**default**): The output is shown in the text format, default behavior
    - ``json``: The output is shown in json-2020.2 
- The ``--output`` (or ``-o``) can be used to dump output in a file instead of stdout
        
    - <filename> : The output file to be dumped


**Example commands** 


.. code-block:: 

    #Reports Scanning of all the devices
    xbmgmt examine 
    
    #Report all the information for a specific device
    xbmgmt examine --d 0000:d8:00.0 -r verbose
    
    #Reports platform information of two devices and dump to a file
    xbmgmt examine -d 0000:b3:00.0 0000:65:00.0 --report platform --format json --output output output.json


xbmgmt reset
~~~~~~~~~~~~

This ``xbmgmt reset`` command can be used to reset one or more devices. 


**The supported options**

.. code-block:: 

    xbmgmt reset [--device| -d] <management bdf> [--type| -t] <reset type>


**The details of the supported options**

- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <management bdf>+ : Mandetory, has to be specified with one or more device management bdf  
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
    xbmgmt reset --device 0000:65:00.0
    
    # Reset kernel communication link of two devices
    xbmgmt reset --device 0000:65:00.0 0000:5e:00.0 --type kernel


