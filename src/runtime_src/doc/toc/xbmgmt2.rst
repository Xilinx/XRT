.. _xbmgmt2.rst:

xbmgmt (Next Generation)
========================

This document describes the new next-generation ``xbmgmt`` commands. These new commands are default from 21.1 release.   

P.S: Legacy ``xbmgmt`` commands are still available and can be invoked by ``xbmgmt --legacy <command>``.


**Global options**: These are the global options can be used with any command. 

 - ``--verbose``: Turn on verbosity and shows more outputs whenever applicable
 - ``--batch``: Enable batch mode
 - ``--force``: When possible, force an operation

The next-generation ``xbmgmt`` commands are

    - ``xbmgmt dump``
    - ``xbmgmt examine``
    - ``xbmgmt program``
    - ``xbmgmt reset``


xbmgmt dump
~~~~~~~~~~~


xbmgmt examine
~~~~~~~~~~~~~~

The ``xbmgmt examine`` command reports detail status information of the specified device

**The supported options**


.. code-block:: shell

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


.. code-block:: shell

    #Reports Scanning of all the devices
    xbmgmt examine 
    
    #Report all the information for a specific device
    xbmgmt examine --d 0000:d8:00.0 -r verbose
    
    #Reports platform information of two devices and dump to a file
    xbmgmt examine -d 0000:b3:00.0 0000:65:00.0 --report platform --format json --output output output.json



xbmgmt program
~~~~~~~~~~~~~~

**The supported usecases with options**

Update the Base partition (applicable for 1RP platform too)

.. code-block:: shell

    xbmgmt program [--device|-d] <management bdf> --update [--force|-f]


Program a Shell Partition for 2RP platform

.. code-block:: shell

    xbmgmt program [--device| -d] <management bdf> --partition <partition file with path>  


Revert to golden image

.. code-block:: shell

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


.. code-block:: shell
 
     #Update the base partition 
     xbmgmt program --device 0000:d8:00.0 --update 
     
     #Program the shell partition
     xbmgmt program --device 0000:d8:00.0 --partition <partition file with path>
 
     xbmgmt program --device 0000:d8:00.0 --revert-to-golden




xbmgmt reset
~~~~~~~~~~~~

This ``xbmgmt reset`` command can be used to reset device. 


**The supported options**

.. code-block:: shell

    xbmgmt reset [--device| -d] <management bdf> 


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to reset
    
    - <management bdf> : The Bus:Device.Function of the device of interest
    

**Example commands**


.. code-block:: shell
 
    xbmgmt reset --device 0000:65:00.0

