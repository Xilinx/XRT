===========================================
Plugin Development Guide
===========================================

The XMA Plugin Interface is used to write software capable of managing a
specific video kernel hardware resource.  The plugin interface consists of a
library for moving data between device memory and host memory and accessing
hardware registers.  Additionally, standard interfaces are defined to
represent various video kernel archtypes such as encoders, decoders, and
filters.

The plugin developer, by implementing a given plugin interface, permits XMA
to translate requests from XMA applications into hardware-specific actions
(i.e. register programming, buffer processing). The XMA plugin is akin to a
software 'driver' in this regard.

The first step in developing an XMA plugin requires you to decide which XMA
kernel interface accurately represents the type of hardware kernel for which
you seek to provide support:

======================================== =========================================
                Kernel Type                           XMA Plugin Interface
======================================== =========================================
Encoders (VP9, H.264, H.265)                   xmaplgenc
Decoders (VP9, H.264, H.265)                    xmaplgdec
Filters (colorspace converter, scalers)   xmaplgfilter or xmaplgscaler
Scalers                                                 xmaplgscaler
Other (embedded cpu)                                   xmaplgkernel
======================================== =========================================

Once selected, the job of the plugin author is to implement the interface
for the given kernel thus providing a mapping between the xma_app_intf and
the kernel.  Most callbacks specified are implicitly mandatory with some exceptions
which will be noted below.

Your plugin will be compiled into a shared object library and linked to the
kernel via create_session properties:



All plugin code must include xmaplugin.h

::

    #include <xmaplugin.h>

This will provide the plugin code access to all data structures necessary
to author XMA plugin code.  This includes access to the structures used
by the xma_app_intf as xmaplugin.h includes xma.h.

What follows is a general description of what is expected of a plugin in
response to the xma_app_intf.

From the application perspective, the following operations
will be peformed:

1. Create session
2. Send data/frame or write**
3. Receive data/frame or read**
4. Destroy

\** in the case of a non-video kernel

Steps 2 and 3 will form the runtime processing of frames/data and likely
repeated for as long as there is data to be processed.

A general mapping between the application interface and plugin interface:

+---------------------+-------------------------------+
| Application Call    |  Plugin Callbacks Invoked     |
+=====================+===============================+
| session_create()    |    init()                     |
+---------------------+-------------------------------+
| send_(data|frame)() |   send_(data|frame)()         |
+---------------------+-------------------------------+
| recv_(data|frame)() |    recv_(data|frame)()        |
+---------------------+-------------------------------+
|  destroy()          |           close()             |
+---------------------+-------------------------------+




Initalization
~~~~~~~~~~~~~~~~~~~~

Initialization is the time for a plugin to perform one or more of the
following:

1. allocate device buffers to handle input data as well as output data
2. initalize the state of the kernel


When a session has been created in response to an application request,
XMA will allocate plugin data that
is session-specific.

XmaSession->plugin_data member is
available to plugin to store the necessary session-specific
state as necessary. There is no need to free these data structures during
termination; XMA frees this data for you.

To allocate buffers necessary to handle both incoming and outgoing
data, please see

1. xma_plg_buffer_alloc(): Allocate device buffer on default session ddr_bank
2. xma_plg_buffer_alloc_arg_num(): Allocate device buffer on ddr_bank connected to a kernel argument


Handling Incoming Application Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each kernel type, there is an application interface to send data to be
proceessed (i.e. encoded, decoded, or otherwised transformed).
Data being sent by an application to the kernel will result in the invocation
of your send()/write() callback.

The most common operation within the plugin is to copy data from host
memory to device memory so that it may be operated on by the kernel.
Subsequently, the kernel must be programmed to know which device buffer
contains the data to be processed and programmed appropriately.

The XMA Plugin library call xma_plg_buffer_write() can be used to copy
host data to device data.

xma_plg_schedule_cu_cmd() or xma_plg_schedule_work_item() can be used to program
the kernel registers and start kernel processing.

Sending Output to the Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each kernel type, there is an application interface to request processed
data (i.e. encoded, decoded, otherwise transformed) by the kernel.  Data
being requested by an application from the kernel will invoke your
recv()/read() callback implementation.

The most common operation within the plugin is to copy data from device
memory back to host memory so that it may be processed by the application.
Subsequently, the kernel may be prepared for new data to arrive for processing.

The XMA Plugin library call xma_plg_buffer_read() can be used to copy
host data to device data.


Termination
~~~~~~~~~~~~~~

When an XMA application has concluded data processing, it will destroy its
kernel session.  Your close() callback will be invoked to perform the necessary
cleanup.  Your close() implementation should free any buffers that were
allocated in device memory during your init() via xma_plg_buffer_free().
Freeing XmaSession->plugin_data is not necessary
as this will be done by the XMA library.

Zerocopy Special Case
~~~~~~~~~~~~~~~~~~~~~~

Encoders are capable of receiving data directly from upstream video processing
kernels such as filters or scalers.  In such a case, it may improve the
the performance of a video processing pipeline that includes both a filter and
an encoder to exchange data directly within device memory rather than have
the filter copy data back to a host buffer only to be re-copied from the host
to the device buffer of the downstream encoder.  This double-copy can be
avoided if the two kernels can share a buffer within the device memory; a
buffer that serves as an 'output' buffer for the filter but an 'input'
buffer for the encoder. This optimization is known as 'zerocopy'.

Use XRM for system resource reservation such that zero-copy is possible
XmaFrame with device only buffer can be output of plugins supporting zero-copy and feeding zero-copy enabled plugin/s
Plugins may use dev_index, bank_index & device_only info from BufferObject to enable or disable zero-copy

Standalone Example
~~~~~~~~~~~~~~~~~~

See XMA copy_encoder_ example for an standalone working example of encoder kernel type.

.. _copy_encoder: https://github.com/Xilinx/xma-samples
