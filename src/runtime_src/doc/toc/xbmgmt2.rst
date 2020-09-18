.. _xbmgmt2.rst:

XBMGMT Next
===========

The next generation version of **xbmgmt** tool is in a preview mode for 2020.2 release. This tool will be in production in future release replacing the current xbmgmt tool. This documents describes various commands and its usage of this new version of the tool.

The xbmgmt command options are

    - ``xbmgmt program``
    - ``xbmgmt status``
    - ``xbmgmt reset``

xbmgmt program
~~~~~~~~~~~~~~

**The supported options**

Program a partition

.. code-block:: 

    xbmgmt program [--device| -d] <bdf> --partition [partition file| uuid]  


Load from a image

.. code-block:: 

    xbmgmt program [--device| -d] <bdf>  [--update| -u] <specific image> <--image|-i> <specific image path> 
    

Revert to golden image
.. code-block:: 

    xbmgmt program --revert-to-golden


**The details of the supported options**

- The ``--device`` (or ``-d``) used to specify the device to be reset
    
    - <bdf>+ : Mandetory, has to be specified with one or more device bdf  
    - ``all``: To specify all devices ``–-device all``  or ``-d all``  can be used
- The ``--partition`` option is used to program specific partition. 
    
    - <partiton file>: 
    - <uuid>:
- The ``--update`` option can optionally be used to specify the specific image to be programmed. 
    
    - ``sc``: Satellite controller image 
    - ``flash``: Flash image 
- The ``--image`` option can be optionally used with ``--update``
  
    - <image name> : 
    - <image path> : 
- The ``--revert-to-golden`` command is used to reverts the flash image back to the golden version of the card.	


**Example commands**


.. code-block::
 
     xbmgmt program --d 0000:d8:00.0 --p ./my_partition.xsabin
 
     xbmgmt program --d 0000:d8:00.0 -u SC --image [specific image path] --revert-to-golden
 
     xbmgmt program -d [BDF] -p [partition file| uuid] -u [specific image] --i [specific image path] --revert-to-golden


xbmgmt status
~~~~~~~~~~~~~

The ``xbmgmt status`` command reports detail status information of the specified device

**The supported options**


.. code-block::

    xbmgmt status [--device| -d] <bdf>] [--report| -r] <report of interest> [--format| -f] <report format> [--output| -u] <filename>
 

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


