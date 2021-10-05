.. _xball.rst:

..
   comment:: SPDX-License-Identifier: Apache-2.0
   comment:: Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.

xball
=====

To facilitate running the same ``xbutil`` or ``xbmgmt`` command on a multi-card environment XRT provides a utility script named ``xball``. The script ``xball`` can be used as below:  

.. code-block:: shell

  #xball <xbutil | xbmgmt> <utility arguments>
  xball xbutil examine

The ``xball`` script will detect all the cards attached to the server, and execute the xbutil (or xbmgmt) command on all of them.

Additionally, the ``xball`` script provides a filtering option to execute the command on some specific cards, such as

.. code-block:: shell

   #Run `xbutil examine` command only on U30 cards
   xball --device-filter 'u30'  xbutil examine

   #Run `xbutil examine` command only on U250 cards
   xball --device-filter '^xilinx_u250' xbutil examine
