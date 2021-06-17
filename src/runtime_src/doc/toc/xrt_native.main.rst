.. _xrt_native.main.rst:

XRT Native Library C++ API
**************************

Configuration APIs
~~~~~~~~~~~~~~~~~~

.. doxygennamespace:: xrt::ini
   :project: XRT
   :members:


Device APIs
~~~~~~~~~~~

.. doxygenclass:: xrt::device
   :project: XRT
   :members:

XCLBIN APIs
~~~~~~~~~~~

.. doxygenclass:: xrt::xclbin
   :project: XRT
   :members:


Buffer APIs
~~~~~~~~~~~

.. doxygenclass:: xrt::bo
   :project: XRT
   :members:


Kernel APIs
~~~~~~~~~~~

.. doxygenclass:: xrt::kernel
   :project: XRT
   :members:

.. doxygenclass:: xrt::run
   :project: XRT
   :members:

Custom IP APIs
~~~~~~~~~~~~~~

.. doxygenclass:: xrt::ip
   :project: XRT
   :members:

Info APIs
~~~~~~~~~

.. doxygennamespace:: xrt::info
   :project: XRT
   :members:
   
UUID APIs
~~~~~~~~~

.. doxygenclass:: xrt::uuid
   :project: XRT
   :members:


XRT Native Library C API
************************

Configuration APIs
~~~~~~~~~~~~~~~~~~

.. include:: ../core/xrt_ini.rst

Device and XCLBIN APIs
~~~~~~~~~~~~~~~~~~~~~~

.. include:: ../core/xrt_device.rst

.. include:: ../core/xrt_xclbin.rst

Buffer APIs
~~~~~~~~~~~

.. include:: ../core/xrt_bo.rst

Kernel APIs
~~~~~~~~~~~
.. include:: ../core/xrt_kernel.rst
