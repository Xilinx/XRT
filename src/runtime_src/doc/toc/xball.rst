.. _xball.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.

xball
=====

To facilitate running the same ``xrt-smi`` or ``xbmgmt`` command on a multi-card environment XRT provides a utility script named ``xball``. The script ``xball`` can be used as below:  

.. code-block:: shell

  #xball <xrt-smi | xbmgmt> <utility arguments>
  xball xrt-smi examine

The ``xball`` script will detect all the cards attached to the server, and execute the ``xrt-smi`` or ``xbmgmt`` command on all of them.

Additionally, the ``xball`` script provides a filtering option to execute the command on some specific cards, such as

.. code-block:: shell

   #Run `xrt-smi examine` command only on U30 cards
   xball --device-filter 'u30'  xrt-smi examine

   #Run `xrt-smi examine` command only on U250 cards
   xball --device-filter '^xilinx_u250' xrt-smi examine
