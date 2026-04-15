.. _xbtools_map.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.

Utility Migration Guide 
***********************

This document maps the legacy ``xbutil``/``xbmgmt`` commands to the new ``xbutil``/``xbmgmt`` commands. It lists out the new ``xbutil``/``xbmgmt`` calls that replace the existing calls. A few points: 

 1) The new utilities are complete redesign and architecture, hence there may not be exact 1-to-1 mapping. 
 2) The help menus (``--help``) of these new utilities have also been improved and are highly descriptive with regards to both the commands and options. Please refer to them as needed.
 3) The new command opions have both short and long format, for example ``-d`` or ``--device``. In the table below, only the longer option format is used.

You may refer the documentation of the new tools in the following links :doc:`xbutil` and :doc:`xbmgmt` . 

xbutil
~~~~~~

+------------+-------------+-------------------+----------------------+
|Subcommand  | Subcommand  |Option             |Option                |
|Legacy      | New         |Legacy             |New                   |
+============+=============+===================+======================+
|            |             |                   |                      |
|``help``    |``--help``   |                   |                      |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``version`` |``--version``|                   |                      | 
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``reset``   |``reset``	   |``-d <bdf>``       |``--device <bdf>``    |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``validate``|``validate`` |``-d <bdf>``       |``--device <bdf>``    |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |``-q``             |``--run quick``       |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |      	       |``--run <testname>``  |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |      	       |``--format <type>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |     	       |``--output <file>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``scan``    |``examine``  |                   |                      |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``query``   |``examine``  |``-d <bdf>``       |``--device <bdf>``    |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |                   |``--report <list>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |                   |``--output <file>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``dump``    |``examine``  |                   |``--format <type>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``program`` |``program``  |``-d <bdf>``       |``--device <bdf>``    |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``dmatest`` |``validate`` |``-d <bdf>``       |``--device <bdf>``    |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |                   |    ``--run DMA``     |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |``-b <blksize kb>``|    n/a               |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |``-p <xclbin>``    |``--user <xclbin>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``host_mem``|``configure``| ``-d <bdf>``      | ``--device <bdf>``   |
|            |             |                   |            	      |
+------------+-------------+-------------------+----------------------+
|            |             |``--enable``       |``--host-mem enable`` |
|            |             |``--size <sz>``    |``--size <sz>``       |
|            |             |                   |            	      |
+------------+-------------+-------------------+----------------------+
|            |             |``--disable``      |``--host-mem disable``|
|            |             |                   |                      |
|            |             |                   |             	      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|``p2p``     |``configure``| ``-d <bdf>``      | ``--device <bdf>``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             | ``--enable``      | ``--p2p enable``     |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             | ``--disable``     | ``--p2p disable``    |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
|            |             |                   | ``--p2p validate``   |
|            |             |                   |                      |
+------------+-------------+-------------------+----------------------+
|            |             |                   |                      |
| ``mem``    |             | ``--read``        |   Not Supported      |
|            |             | ``--write``       |                      |  
+------------+-------------+-------------------+----------------------+
|            |                                                        |
| ``top``    |          separate ``xbtop`` command                    |
|            |                                                        |  
+------------+-------------+-------------------+----------------------+



xbmgmt
~~~~~~

+-------------+-------------+---------------------------+----------------------------------+
|Subcommand   | Subcommand  |Option                     |Option                            |
|Legacy       | New         |Legacy                     |New                               |
+=============+=============+===========================+==================================+
|             |             |                           |                                  |
|``help``     |``--help``   |                           |                                  |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |
|``version``  |``--version``|                           |                                  |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |
|``flash``    |``program``  |``-d <bdf>``               |``--device <bdf>``                |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |
|             |             |``--update``	        |``--base``	                   |    	
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |
|             |             |``--update --shell <name>``|``--base --image <name w/ path>`` |
|             |             |                           |	   	                   |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  | 
|             |             |``--factory_reset``        |``--revert-to-golden``            |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  | 
|``partition``|``program``  |``--card <bdf>``           |``--device <bdf>``                |	
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  | 
|             |             |``--program``              |``--shell <shell-file w/ path>``  |	
|             |             |``--name <shell name>``    |                                  |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  | 
|             |             |``--path <xclbin w/ path>``|``--user <xclbin w/ path>``       |	
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |    
|``scan``     |``examine``  |                           |``--report host``                 |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |    
|	      |``examine``  |                           |``--device <bdf>``                |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  | 
|             |             |     	                |``--report <list>``               |	
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  | 
|             |             |      	                |``--format <type>``               |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |
|             |             |     	                |``--output <file>``               |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |    
|``reset``    |``reset``    |``-d <bdf>``               |``--device <bdf>``                |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+
|             |             |                           |                                  |    
|``config``   |             |``--enable_retention``     | To be implemented in next release|
|             |             |                           |                                  |
|             |             |``--disable_retention``    |                                  |
|             |             |                           |                                  |
+-------------+-------------+---------------------------+----------------------------------+



Few examples of legacy vs new commands 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here are few example use-cases of legacy vs new commands

**1. Getting all the information obtained from the userspace kernel driver**

**Legacy command**

There were two variants of legacy commands

.. code-block:: shell

       xbutil query  
       
or 

.. code-block:: shell

       xbutil dump


**New command**

.. code-block:: shell

       xbutil examine --device 0000:b3:00.1 --report all

**2. Validate the card**

**Legacy command**

.. code-block:: shell

        xbutil validate 
        
**New command**

.. code-block:: shell
 
        xbutil validate --device 0000:b3:00.1
        
**3. Obtaining platform information such as SC version, flashed partition(s) running on the card, etc.** 

**Legacy command**

.. code-block:: shell

   xbmgmt  flash --scan
   
**New command**

.. code-block:: shell

   xbmgmt examine --device 0000:b3:00.0 --report platform 
   
   
You need to use a combination of ``xball xbmgmt`` command if you prefer to see information from all the cards attached to the host server. Please see ``xball`` page for more details. 
   
**4. Programming the base partition**

**Legacy command**

.. code-block:: shell

   xbmgmt --update --shell <partition name>
   
**New command**

.. code-block:: shell

   xbmgmt program --base --device 0000:d8:00.0 --base 
   
or when a specific partition to choose

.. code-block:: shell

   xbmgmt program --base --device 0000:d8:00.0 --base --image <partition name> 

**5. Resetting the device**

**Legacy command**

.. code-block:: shell

   xbutil --reset 
   
**New command**

.. code-block:: shell

   xbutil --reset --device 0000:d8:00.1  


