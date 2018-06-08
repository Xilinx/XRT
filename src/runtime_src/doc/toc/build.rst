Building and Installing Software Stack
--------------------------------------

OCL Runtime
~~~~~~~~~~~

::

   git clone https://github.com/Xilinx/SDAccel.git
   cd SDAccel
   mkdir build
   cd build
   cmake ../
   make


XRT
~~~

The key XRT components making up the software stack are provided as zip files. These files
can be found in SDx installation directory under ``data/sdaccel/pcie/src`` directory.

xocl
~~~~
::

   unzip xocl.zip
   cd driver/xclng/drm/xocl
   make
   sudo make install

xclmgmt
~~~~~~~
::

   unzip xclmgmt.zip
   cd driver/xclng/mgmt
   make
   sudo make install

HAL
~~~
::

   unzip xclgemhal.zip
   cd driver/xclng/user_gem
   make

xbsak
~~~~~
::

   unzip xclgemhal.zip
   cd driver/xclng/tools/xbsak_gem
   make
