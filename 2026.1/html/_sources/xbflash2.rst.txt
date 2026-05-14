.. _xbflash2.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2022 Xilinx, Inc. All rights reserved.

xbflash2
========

The Xilinx (R) Board Flash utility (xbflash2) is a standalone command line utility to flash a custom image onto given device. This document describes the latest ``xbflash2`` commands.

In 2022.1 release, this utility is Early Access with limited validation.

This tool supported for all Alveo platforms.

This tool doesn't require XRT Package and it doesn't come with XRT package, it comes as separate xbflash package.

``xbflash2`` tool is available in the Alveo card web page, getting started session, xbflash2 tab.

For example: https://www.xilinx.com/products/boards-and-kits/alveo/u50.html#xbflash2

After xbflash package installation, content goes to ``/usr/local/bin``

This tool is verified and supported only on XDMA PCIe DMA designs.

**Global options**: These are the global options can be used with any command. 

 - ``--help`` : Get help message to use this application.
 - ``--verbose``: Turn on verbosity and shows more outputs whenever applicable.
 - ``--batch``: Enable batch mode.
 - ``--force``: When possible, force an operation.

Currently supported ``xbflash2`` commands are

    - ``xbflash2 program``    
    - ``xbflash2 dump``


xbflash2 program
~~~~~~~~~~~~~~~~

The ``xbflash2 program`` command programs the given acceleration image into the device's shell.

**The supported options**

Updates the image(s) for a given device.

.. code-block:: shell

    xbflash2 program [--help|-h] --[ spi | qspips ] [commandArgs]

**The details of the supported options**

- The ``--help`` (or ``-h``) gets help message tp use this sub-command.
- The ``--spi`` option is used for spi flash type.
- The ``--qspips`` option is used for qspips flash type.


xbflash2 program --spi
~~~~~~~~~~~~~~~~~~~~~~

The ``xbflash2 program --spi`` command programs the given acceleration image into the device's shell for spi flash type.

**The supported usecases and their options**

Program the image(.mcs) to the device's shell.

.. code-block:: shell

    xbflash2 program --spi [--image|-i] <.mcs file path> [--device|-d] <management bdf> [--dual-flash|-u] [--bar|-b] <BAR index> [--bar-offset|-s] <BAR offset>

Revert to golden image. Resets the FPGA PROM back to the factory image.

.. code-block:: shell

    xbflash2 program --spi [--revert-to-golden|-r] [--device|-d] <management bdf> [--dual-flash|-u] [--bar|-b] <BAR index> [--bar-offset|-s] <BAR offset>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <management bdf> : The Bus:Device.Function of the device of interest
 
- The ``--dual-flash`` (or ``-u``)  option specifies if the card is dual flash supported.

- The ``--bar`` (or ``-b``)  option specifies BAR-index, default is 0.

- The ``--bar-offset`` (or ``-s``)  option specifies BAR-offset, default is 0x40000.

- The ``--image`` (or ``-i``)  option Specifies MCS image path to update the persistent device. 
   
- The ``--revert-to-golden`` (or ``-r``)  command is used to reverts the flash image back to the golden version of the card.


**Example commands**


.. code-block:: shell
 
     #Program the mcs image. 
     xbflash2 program --spi --device 0000:3b:00.0 --image <mcs path>     
     
     #Program the mcs image.
     xbflash2 program --spi --device 0000:3b:00.0 --image <mcs path> --bar 0 --bar-offset 0x10000
     
     #Program the image for dual-flash type.
     xbflash2 program --spi --device 0000:5e:00.1 --image <primary.mcs path> --image <secondary.mcs path> --bar 0 --bar-offset 0x40000 --dual-flash
     
     #Revert to golden image
     xbflash2 program --spi --device 0000:d8:00.0 --revert-to-golden --bar 0 --bar-offset 0x40000 --dual-flash


xbflash2 program --qspips
~~~~~~~~~~~~~~~~~~~~~~~~~

The ``xbflash2 program --qspips`` command programs the given acceleration image into the device's shell for qspips flash type.

**The supported usecases and their options**

Program the image(boot.bin) to the device's shell.

.. code-block:: shell

    xbflash2 program --qspips [--image|-i] <boot.bin path> [--device|-d] <management bdf> [-offset|-a] <offset on flash> [--flash-part|-p] <qspips-flash-type> [--bar|-b] <BAR index> [--bar-offset|-s] <BAR offset>

Erase flash on the device.

.. code-block:: shell

    xbflash2 program --qspips [--erase|-e] [--length|-l] <length> [--device|-d] <management bdf> [-offset|-a] <offset on flash> [--flash-part|-p] <qspips-flash-type> [--bar|-b] <BAR index> [--bar-offset|-s] <BAR offset>


**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <management bdf> : The Bus:Device.Function of the device of interest

- The ``--offset`` (or ``-a``)  option specifies offset on flash to start, default is 0.

- The ``--flash-part`` (or ``-p``)  option specifies qspips-flash-type, default is qspi_ps_x2_single.

- The ``--bar`` (or ``-b``)  option specifies BAR-index for qspips, default is 0.

- The ``--bar-offset`` (or ``-s``)  option specifies BAR-offset for qspips, default is 0x40000.

- The ``--length`` (or ``-l``)  option specifies length-to-erase, default is 96MB.

- The ``--image`` (or ``-i``)  option specifies boot.bin image path to update the persistent device.
   
- The ``--erase`` (or ``-e``)  command is used to erase flash on the device.


**Example commands**


.. code-block:: shell
 
     #Program the boot.bin image. 
     xbflash2 program --qspips --device 0000:3b:00.0 --image <boot.bin path>

     #Program the boot.bin image. 
     xbflash2 program --qspips --device 0000:3b:00.0 --image <boot.bin path> --offset 0x0 --bar-offset 0x10000 --bar 0 
     
     #Erase flash on the device
     xbflash2 program --spi --device 0000:d8:00.0 --erase --length 0x06000000 --offset 0x0 --bar 0 --bar-offset 0x40000


xbflash2 dump
~~~~~~~~~~~~~

The ``xbflash2 dump`` command reads the image(s) for a given device for a given length and outputs the same to given file. It is applicable for only QSPIPS flash..

**The supported options**

Reads the image(s) for a given device and dump out content of the specified option.

.. code-block:: shell

    xbflash2 dump [--help|-h] --[ qspips ] [commandArgs]

**The details of the supported options**

- The ``--help`` (or ``-h``) gets help message tp use this sub-command.
- The ``--qspips`` option is used for qspips flash type.


xbflash2 dump --qspips
~~~~~~~~~~~~~~~~~~~~~~

The ``xbflash2 dump --qspips`` command dump out content to the given ouput file. 

**The supported usecases and their options**

Reads the image(s) for a given device for a given length and outputs the same to given file.

.. code-block:: shell

    xbflash2 dump --qspips [--device|-d] <management bdf> [-offset|-a] <offset on flash> [--length|-l] <length to read> [--flash-part|-p] <qspips-flash-type> [--bar|-b] <BAR index> [--bar-offset|-s] <BAR offset> [--output|-o] <output file path>
    
**The details of the supported options**

- The ``--device`` (or ``-d``) specifies the target device to program
    
    - <management bdf> : The Bus:Device.Function of the device of interest

- The ``--offset`` (or ``-a``)  option specifies offset on flash to start, default is 0.

- The ``--length`` (or ``-l``)  option specifies length-to-read, default is 128MB.

- The ``--flash-part`` (or ``-p``)  option specifies qspips-flash-type, default is qspi_ps_x2_single.

- The ``--bar`` (or ``-b``)  option specifies BAR-index for qspips, default is 0.

- The ``--bar-offset`` (or ``-s``)  option specifies BAR-offset for qspips, default is 0x40000.

- The ``--output`` (or ``-o``)  option to specify output file path to save read contents..


**Example commands** 


.. code-block:: shell

      
    #Dump out content to the given ouput file
    xbflash2 dump --qspips --device 0000:3b:00.0 --offset 0x0 --length 0x08000000 --bar-offset 0x10000 --bar 0 --output /tmp/flash_dump.txt
    
