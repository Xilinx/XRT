===========================================
Application Development Guide
===========================================

The XMA application interface is used to provide an API that can
be used to control video accelerators.  The XMA API operations
fall into four categories:

1. Initialization
2. Create session
3. Runtime frame/data processing
4. Cleanup

Initialization
~~~~~~~~~~~~~~~~~~~~~~
The first act an application must perform is that of initialization of the
system environment.  This is accomplished by calling xma_initialize() and
passing in device and xclbin info.

Create Session
~~~~~~~~~~~~~~~~~~~~~~
Each kernel class (i.e. encoder, filter, decoder, scaler, filter, kernel)
requires different properties to be specified before a session can be created.

1. xmadec
2. xmaenc
3. xmafilter
4. xmascaler
5. xmakernel

The general initialization sequence that is common to all kernel classes is as follows:

1. define key type-specific properties of the kernel to be initialized
2. call the_session_create() routine corresponding to the kernel (e.g. xma_enc_session_create())


Runtime Frame and Data Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Most kernel types include routines to consume data and then produce data from
host memory buffers.  Depending on the nature of the kernel, you may be
required to send a frame and then receive data or vice versa.
XMA defines buffer data structures that correspond to frames (XmaFrame)
or data (XmaFrameData). These buffer structures are used to communicate
with the kernel application APIs and include addresses to host memory.  The XMA Application Interface includes
functions to allocate data from host or device memory and create these containers for
you.  See xmabuffers.h for additional information.

Some routines, such as that of the encoder, may require multiple frames of
data before recv_data() can be called.  You must consult the API to ensure
you check for the correct return code to know how to proceed.  In the case of
the encoder, calling xma_enc_session_send_frame() may return XMA_SEND_MORE_DATA
which is an indication that calling recv_data() will not yield any data as
more frames must be sent before any output data can be received.

Of special note is the XmaKernel plugin type.  This kernel type is a generic
type and not necessarily video-specific. It is used to represent kernels that
perform control functions and/or other functions not easily represented by
any of the other kernel classes.

Cleanup
~~~~~~~
When runtime video processing has concluded, the application should destroy
each session.  Doing so will free the session to be used by another thread or
process and ensure that the kernel plugin has the opportunity to perform
proper cleanup/closing procedures.

1. xma_enc_session_destroy()
2. xma_dec_session_destroy()
3. xma_scaler_session_destroy()
4. xma_filter_session_destroy()
5. xma_kernel_session_destroy()

Standalone Example
~~~~~~~~~~~~~~~~~~

See XMA copy_encoder_ example for an standalone working example of encoder kernel type.

.. _copy_encoder: https://github.com/Xilinx/xma-samples
