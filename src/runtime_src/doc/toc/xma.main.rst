===========================================
Xilinx Media Accelerator (XMA) Core Library
===========================================

Introduction
---------------

The Xilinx Media Accelerator (XMA) library (libxmaapi) is a host interface
meant to simplify the development of applications managing and controlling
video accelerators such as decoders, scalers, filters, and encoders. The
libxmaapi is comprised of two API interfaces: an application interface and a
plugin interface. The application API is a higher-level, generalized
interface intended for application developers responsible for integrating
control of Xilinx accelerators into software frameworks such as FFmpeg,
GStreamer, or proprietary frameworks. The plugin API is a lower level
interface intended for developers responsible for implementing hardware
control of specific Xilinx acceleration kernels. In general, plugins are
developed by kernel providers as these plugins are specialized user space
drivers that are aware of the low-level hardware interface.

From a high-level perspective, the XMA sits between a media framework (i.e.
FFmpeg)  and the Xilinx runtime (XRT). In addition, the XMA acts as a peer
to the host side implementation of OpenCL. The diagram below illustrates the
entire stack including an example of common accelerator kernels that are
possible in a specific design:


.. image:: XMA-Stack.png
   :align: center

The remaining sections will describe the key architectural aspects of the
libxmaapi and describe the high-level API along with the low-level plugin
API.

XMA Application Interface Overview
----------------------------------------

The API for the libxmaapi can be categorized into three areas:

1. Initialization
2. Video frame processing
3. Termination

From an interface perspective, the high-level or upper edge interface and the
low-level or plugin interface are organized as follows:

.. image:: XMA-Internal-Stack.png
   :align: center

The diagram above illustrates a number of distinct API layers.  The XMA upper
edge initialization API provides two types of initialization: global and
session level initialization.  The XMA upper edge API also provides functions
for sending and receiving frames as well as a method for gracefully terminating
a video stream when the end of the stream is found.  Also depicted in the
diagram is the XMA Framework and resource manager.  The XMA Framework and
resource manager are responsible for managing the state of the system,
delegating requests to the appropriate plugin, and selecting available
resources based on session creation requests.

See the `Application Development Guide`_ for more information about utilizing the XMA
application interface to development your own stand alone or integrated
applications.

XMA Plugin Interface Overview
----------------------------------

The XMA lower edge API parallels the upper edge API; however, the lower edge
API is comprised of function callbacks similar to those used in a driver or as
defined in the FFmpeg plugin interface.

There are five classes of XMA plugin interfaces: decoders, encoders,
filters, scalers, and a generic 'kernel' class.
Since each of these classes are unique in terms of the processing performed,
the APIs are slightly different, however, there is a common pattern associated
with these classes. Specifically, a plugin must provide registration
information and must implement all required callback functions. In general, an
XMA plugin implements at least four required callback functions: initialize,
send frame or send data, receive frame or receive data, and close. In addition
to these required functions, the encoder plugin API offers two optional
callbacks for channel allocation and retrieving the physical address of an
available device input buffer. The channel allocation callback is needed for
encoders that support multiple channels within a single kernel by time-division
multiplexing of the underlying accelerator resources. The retrieval of a
physical address of a device input buffer is needed when an encoder offers
zero-copy support as this buffer address is required by the kernel preceding
the encoder as the output buffer.

By way of example, the following represents the interface of the XMA Encoder
class:


::

    typedef struct XmaEncoderPlugin
    {
     XmaEncoderType  hwencoder_type;
     const char    hwvendor_string;
     XmaFormatType   format;
     int32_t         bits_per_pixel;
     size_t          kernel_data_size;
     size_t          plugin_data_size;
     int32_t         (*init)(XmaEncoderSessionenc_session);
     int32_t         (*send_frame)(XmaEncoderSessionenc_session,
                                   XmaFrame         frame);
     int32_t         (*recv_data)(XmaEncoderSession enc_session,
                                  XmaDataBuffer     data,
                                  int32_t           data_size);
     int32_t         (*close)(XmaEncoderSessionsession);
     int32_t         (*alloc_chan)(XmaSessionpending_sess,
                                   XmaSession*curr_sess,
                                   uint32_t sess_cnt);
     uint64_t        (*get_dev_input_paddr)(XmaEncoderSessionenc_session);
    } XmaEncoderPlugin;


Finally, the XMA offers a set of buffer management utilities that includes
the creation of frame buffers and encoded data buffers along with a set of
miscellaneous utility functions. By providing XMA buffer management
functions, it is possible for an XMA plugin to easily integrate with
virtually any higher-level media framework without requiring any
changes. Instead, it is up to the upper level media framework functions to
convert buffers into the appropriate XMA buffer.
The sections that follow will describe the layers of the API in more detail and
provide examples of how these functions are called from both the perspective of
an application and from the perspective of an XMA plugin. For the low-level
details of the APIs, please consult the doxygen documentation.


Sequence of Operations
--------------------------

In order to better understand how XMA integrates with a standard multi-media
framework such as FFmpeg, the sequence diagram that follows identifies the
critical operations and functions called as part of a hypothetical encoder. The
diagram only calls out the initialization and processing stages:

.. image:: XMA-Sequence-Diagram.png
   :align: center

As shown in the diagram above, the system is comprised of five blocks:

- The FFmpeg Command Line application that is used to create a processing graph
- The FFmpeg encoder plugin that interfaces with the XMA Upper Edge Interface to manage a video session
- The XMA Upper Edge library interface responsible for initialization, resource allocation, and dispatching of the XMA plugin
- The XMA Lower Edge plugin responsible for interfacing with the SDAccel Video Kernel
- The XMA Video Kernel responsible for accelerating the encoding function

While this sequence diagram only shows five components, more complex systems
can be developed that include multiple accelerators with the associated XMA
plugin and FFmpeg plugin. In fact, adding new processing blocks is controlled
entirely by the FFmpeg command line and the presence of the requested
accelerator kernels. No additional development is required if all of the
SDAccel kernels are available along with the associated plugins.  In this
example, an FFmpeg command is invoked that ingests an MP4 file encoded as H.264
and re-encodes the file as H.264 at a lower bit rate. As a result, the main()
function of the FFmpeg command is invoked and this calls the xma_initialize()
function. The xma_initialize() function is called prior to executing any other
XMA functions and performs a number of initialization steps that are detailed
in a subsequent section.
Once the xma_initialize() successfully completes, the FFmpeg main() function
performs initialization of all requested processing plugins. In this case, the
hypothetical encoder plugin has been registered with FFmpeg and the
initialization callback of the plugin is invoked. The FFmpeg encoder plugin
begins by creating an XMA session using the xma_enc_session_create() function.
The xma_enc_session_create() function finds an available resource based on the
properties supplied and, assuming resources are available, invokes the XMA
plugin initialization function. The XMA plugin initialization function
allocates any required input and output buffers on the device and performs
initialization of the SDAccel kernel if needed.

After initialization has completed, the FFmpeg main() function reads encoded
data from the specified file, decodes the data in software, and sends the raw
video frame to the FFmpeg plugin for encoding by calling the encode2() plugin
callback. The encode2() callback function converts the AVFrame into an XmaFrame
and forwards the request to the XMA Upper Edge interface via the
xma_enc_session_send_frame() function. The xma_enc_session_send_frame()
function locates the corresponding XMA plugin and invokes the send frame
callback function of the plugin. The XMA send frame callback function writes
the frame buffer data to a pre-allocated DDR buffer on the device and launches
the kernel. After the FFmpeg plugin encode2() function has sent the frame for
encoding, the next step is to determine if encoded data can be received or if
another raw frame should be sent. In most cases, an encoder will want several
raw frames before providing encoded data. Supplying multiple frames before
generated encoded data improves video quality through a look ahead and improves
performance by allowing new frame data to be written to the device DDR in
parallel with processing previously supplied frames.  Assuming a frame is ready
to be received, the xma_enc_session_recv_data() function is called by the
FFmpeg plugin and in turn results in the receive data function of the XMA
plugin being invoked. The XMA plugin communicates with the kernel to ensure
that data is ready to be received, determines the length of the encoded data,
and reads the encoded data from DDR device memory to host memory.
The description above is meant as a high-level introduction to FFmpeg and XMA.
The remainder of this document covers these topics in more depth and provides
code examples to help illustrate usage of the XMA.

Application Development Guide
----------------------------------

The XMA application interface is used to provide an API that can
be used to control video accelerators.  The XMA API operations
fall into three categories:

- Initialization
- Runtime frame/data processing
- Cleanup

Initialization
~~~~~~~~~~~~~~~~~~~~~~
The first act an application must perform is that of initialization of the
system environment.  This is accomplished by calling xma_initialize() and
passing in a string that represents the filepath to your system configuration
file.  This system configuration file, described in more detail below, serves
as both information about the images you will be deploying as well as
instructions to XMA with regard to which devices will be programmed with
a given image.

Once the system has been configured according to the instructions in your
system configuration file, the next step is allocate and initialize the video
kernels that will be required for your video processing pipeline.  Each class
of video kernel supported by XMA has its own initialization routine and
a set of properties that must be populated and passed to this routine to
allocate and initialize a video kernel.  Both system wide initialization and
kernel initialization are detailed in the next two sections.

XMA System Configuration File
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

System configuration is described by a file conforming to YAMLsyntax_.
This file contains instructions for the XMA system
initialization as well as description(s) of the kernel contents of the xclbin
image file(s).  The configuration file consists of two logial parts:
- System paths to required libraries and binary files (e.g. pluginpath)
- One or more image deployment plans and descriptions (i.e. ImageCfg)

.. _YAMLsyntax: http://www.yaml.org

Below is a sample configuration file describing a simple system
configuration for a single device utilizing an image file containing a
single HEVC encoder kernel:

::

    SystemCfg:
        - logfile:    ./output.log
        - loglevel:   2
        - dsa:        xilinx_1525_dynamic_5_1
        - pluginpath: /tmp/libxmaapi/installdir/share/libxmaapi
        - xclbinpath: /tmp/xclbins
        - ImageCfg:
            xclbin:   hevc_encoder.xclbin
            zerocopy: disable
            device_id_map: [0]
            KernelCfg: [[ instances: 1,
                          function: encoder,
                          plugin: libhevc.so,
                          vendor: ACME,
                          name: hevc_encoder_1,
                          ddr_map: [0]]]

Because this file is parsed using YAML syntax, the indentation present in
this example is mandatory for showing the relationships between the data.

The system information comes first and includes the path to the directory
of the XMA plugin libraries as well as a directory to the xclbin files (aka
images).  After this system information will be one or more image
descriptions. Each image description, denoted by the 'ImageCfg' key,
instructs XMA as to which devices should be programmed with the given image
file. In the example above, we are deploying only to device '0' (devices are
enumerated as positive integers starting from 0).  In addition, a
description of the kernels that are included in the image is also a part of
the image description and will be used by XMA for tracking kernel resources.

The configuration file is hierarchial and must conform to YAML
syntax as well as include the requisite keys else an error will be thrown
indicating what is missing/mistaken.

In Backus-Naur Form, the grammar of the YAML file could be described as
follows:

::

    @precondition
    [SystemCfg]    ::= SystemCfg:CRLF
                       (HTAB[logifile]CRLF)*
                       (HTAB[loglevel]CRLF)*
                       HTAB[dsa]CRLF
                          HTAB[pluginpath]CRLF
                       HTAB[xclbinpath]CRLF
                       (HTAB[ImageCfg])+
    [logfile]      ::= logfile:[filepath]
    [loglevel]     ::= loglevel:[0 | 1 | 2| 3]
    [dsa]          ::= dsa:[name_string]
    [pluginpath]   ::= pluginpath:[filepath]
    [xclbinpath]   ::= xclbinpath:[filepath]
    [ImageCfg]     ::= ImageCfg:CRLF HTAB*2[zerocopy]CRLF
                       HTAB*2[device_id_map]CRLF
                       HTAB*2[KernelCfg]CRLF
    [zerocopy]     ::= zerocopy:(enable | disable)
    [device_id_map]::= device_id_map:[number_list] CRLF
    [KernelCfg]    ::= KernelCfg:%5B (%5B HTAB[instances]CRLF
                       HTAB*3[function]CRLF
                       HTAB*3[plugin]CRLF
                       HTAB*3[vendor]CRLF HTAB*3[name]CRLF
                       HTAB*3[ddr_map]CRLF %5D)+ %5D
    [instances]    ::= instances:digit+
    [function]     ::= encoder | scaler | decoder | filter | kernel
    [plugin]       ::= plugin:[name_string]
    [vendor]       ::= vendor:[name_string]
    [name]         ::= name:[name_string]
    [ddr_map]      ::= ddr_map:[number_list]
    [filepath]     ::= (%2F(vchar)*)+
    [name_string]  ::= (vchar)+
    [number_list]  ::= %5B digit+[,(digit)+]*%5D

A description of each YAML key:

**Parameters**

``SystemCfg``
  Mandatory header property. Takes no arguments.

``logifile``
 Optional property of SystemCfg; specifies filename to write
 log output.  If logfile and loglevel parameters are not specified, the
 log level will default to INFO and the output file will be stdout.

``loglevel``
  Optional property of SystemCfg; specifies the level of logging
  of which there are four: CRITICAL, ERROR, INFO, DEBUG.  Logs of a the level
  specified or lower will be output to the specified logfile.  The level mapping
  is as follows: 0 = CRITICAL, 1 = ERROR, 2 = INFO, 3 = DEBUG.
  For more information regarding the logging capability see xmalog.

``dsa``
 Property of SystemCfg; The name of the "Dynamic System Archive"
 used for all images.

``pluginpath``
 Property of SystemCfg; The path to directory containing all
 plugin libraries (typically \<libxmaapi install dir\>/share/libxmaapi)

``xclbinpath``
 Property of SystemCfg; The path to the directory containing
 the hardware binary file(s) that will be used to program the devices on the
 system.

``ImageCfg``
 Property of SystemCfg; Mandatory sub-header property
 describing an xclbin image as well as specifying to which device(s) is shall
 be deployed.

``xclbin``
 Property of ImageCfg; The xclbin filename that comprises this
 image to be deployed to the specified devices in device_id_map.

``zerocopy``
 Property of ImageCfg; Either the bare word 'enable' or 'disable'.
 If set to 'enable', indicates that zerocopy between kernels will be attempted
 if possible (requires both kernels to be connected to the same device
 memory).

``device_id_map``
 Property of ImageCfg; An array of numeric device ids
 (0-indexed) indicating which fpga devices will be programmed with the xclbin.
 Note: if a device id specified is > than the number of actual devices on the
 system, initalization will fail and an error message will be logged.

``KernelCfg``
 Property denoting the start of array of kernel entries contained in the xclbin.

``instances``
 Propery of KernelCfg; identifies the number of kernels of a
 a specific type included in this xclbin.  IMPORTANT: The order of the
 kernel entries MUST MATCH the order of base addresses in which the kernels
 are assigned in a given xclbin.  Lowest base address must be described first.

``function``
 Either 'encoder','scaler','decoder','filter' or 'kernel' as
 appropriate for this kernel entry.

``plugin``
 Then name of the XMA plugin library that will be mapped to
 this kernel entry; used by XMA to route high level application calls to the
 appropriate XMA plugin driver.

``vendor``
 Name of the vendor that authored this kernel.  Important for
 session creation as the vendor string is used by application code to, in
 part, identify which kernel entry is being requested for a given session.

``name``
 The name, as it appears in the xclbin, of this kernel entry. Not used as this time.

``ddr_map``
 An array of integer values indicating a mapping of
 kernel instances to DDR banks.  This MUST MATCH the number of kernel
 instances indicated for this entry.


Below is a sample of a more complex, multi-image YAML configuration file:

::

    SystemCfg:
        - logfile:    ./output.log
        - loglevel:   2
        - dsa:        xilinx_xil-accel-rd-vu9p_4ddr-xpr_4_2
        - pluginpath: /plugin/path
        - xclbinpath: /xcl/path
        - ImageCfg:
            xclbin: filename1.xclbin
            zerocopy: enable
            device_id_map: [0,1]
            KernelCfg: [[ instances: 2,
                          function: HEVC,
                          plugin:  libhevc.so,
                          vendor: ACME,
                          name:   hevc_kernel,
                          ddr_map: [0,0]],
                        [ instances: 1,
                          function: Scaler,
                          plugin: libxscaler.so,
                          vendor: Xilinx,
                          name: xlnx_scaler_kernel,
                          ddr_map: [0]]]
        - ImageCfg:
            xclbin: filename2.xclbin
            zerocopy: disable
            device_id_map: [2]
            KernelCfg: [[ instances: 1,
                          function: H264,
                          plugin:  libxlnxh264.so,
                          vendor: Xilinx,
                          name: H264_E_KERNEL,
                          ddr_map: [0]]]


In the above example, two images are described.  XMA will deploy the
filename1.xclbin to devices 0 and 1. The first image consists of three kernels:
two hevc kernels mapped to DDR banks 0 and 0.  The third kernel is the video
scaler.  The second image file is instructed to be deployed to device 2 and
consists of a single h264 kernel mapped to ddr bank 0.
Logging is set to a local file called output.log and at the INFO level (i.e.
all logging of type CRITICAL, ERROR and INFO will be output to the log).


This YAML file will be consumed by the application code as the first step in
the initalization process.

XMA Initalization
~~~~~~~~~~~~~~~~~~~~~~

The prior section described the components of a proper configuration file
necessary for describing the planned initialization of the system.  Herein,
we describe the proper XMA API calls to both initialize the system
with your properly prepared YAML system configuration file as well as the
to allocate and initialize one or more video kernels.

Initialization has two parts and must be performed in the following order:


- system initialization wherein all devices are programmed with images as described by the XMA system configuration file
- kernel initialization wherein a specific kernel resource is initialized for video processing


All application code must include the following header file to access the
XMA application interface:

::

    #include <xma.h>


This header will pull in all files located in [include_dir]/app/ which,
collectively, defines the complete application interface and datastructures
required for XMA development.

The first step for any XMA application is to initalize the system with the
system configuraton file:

::

    //prior includes
    ...
    #include <xma.h>
    // XMA application interface
    int main(void) {
        int rc;charmy_yaml_path = "/tmp/xma_sys_cfg.yaml";
        rc = xma_initalize(my_yaml_path);...
    }


The above code will program all devices on the system as defined in the
xma_sys_cfg.yaml.  The name of the configuration file is arbitrary and you
may have multiple configuration files.  However, only the first invocation
of xma_initialize will result in programming of the system.  Any subsequent
invocation is idempotent.  If another process attempts to initalize the
system (or the same program is invoked a 2nd time) while the original
process that initialized the system is still active, the existing system
configuration will be utilized by the 2nd process; device programming will
only ever occur once.  When all processes connected to the original system
configuration have terminated, the process of initialization with a new YAML
file can begin anew when a later process calls xma_initalize() with a new
system configuration file.

Once the system has been initialized, then kernel sessions can be allocated.

Each kernel class (i.e. encoder, filter, decoder, scaler, filter, kernel)
requires different properties to be specified before a session can be created.

See the document for the corresponding module for more details for a given
kernel type:
- xmadec
- xmaenc
- xmafilter
- xmascaler
- xmakernel

The general initialization sequence that is common to all kernel classes is as follows:

- define key type-specific properties of the kernel to be initialized
- call the_session_create() routine corresponding to the kernel (e.g. xma_enc_session_create())
  Using the decoder kernel as an example, the following code defines request for an H264 decoder kernel made by Xilinx:


::

    #include <xma.h>
    ...
    // init system via yaml file
    ...
    // Setup decoder properties
    XmaDecoderProperties dec_props;
    dec_props.hwdecoder_type = XMA_H264_DECODER_TYPE;
    strcpy(dec_props.hwvendor_string, "Xilinx");
    // Create a decoder session based on the requested properties
    XmaDecoderSessiondec_session;
    dec_session = xma_dec_session_create(&dec_props);
    if (!dec_session){
        // Log message indicating session could not be created
        // return from function
    }
        ...

What is returned is a reference to a session object (XmaDecoderSession in the case of the above example).  This will serve as an opqaue object handlthat you will pass to all other API routines interacting with this kernelA session represents control a single kernel.  Note that some kernelmay support 'channels' which are portions of a kernel resource that behavlike full kernels (i.e. in essence, a 'virtual' kernel).  The distinctionis unimportant to the application developer; a session is a kernel resourcand functions as a dedicated kernel resource to the requesting process othread.  Note: channels of a given kernel may only be assigned to threadfrom within a given process context. Multiple processes may not shara kernel; channels from a single kernel may not be assigned to multiplprocesses.


Runtime Frame and Data Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Once system and kernel initalization (i.e. session creation) are complete,
video processing may commence.

Most kernel types include routines to consume data and then produce data from
host memory buffers.  Depending on the nature of the kernel, you may be
required to send a frame and then receive data or vice versa.
XMA defines buffer data structures that correspond to frames (XmaFrame)
or data (XmaFrameData). These buffer structures are used to communicate
with the kernel application APIs and include addresses to host memory which
you will be required to allocate.  The XMA Application Interface includes
functions to allocate data from host memory and create these containers for
you.  See xmabuffers.h for additional information.

Continuing with our decoder example, the two runtime routines for data
processing are:

- xma_dec_session_send_data()
- xma_dec_session_recv_frame()

Calling the send_data() routine and following with recv_frame() will form
the body of your runtime processing code.

If, by contrast, we examine the XMA Encoder library, we see the following
two routines:

- xma_enc_session_send_frame()
- xma_enc_session_recv_data()

The idea is the same as that of the decoder: send data to be processed, thereceive the data.


::

    int ret, data_size = 0;...// XMA init code and enc_session
    ...
    // Create an input frame
    XmaFrameProperties fprops;
    fprops.format = XMA_YUV420_FMT_TYPE;fprops.width = 1920;
    fprops.height = 1080;
    fprops.bits_per_pixel = 8;
    XmaFramescl_frame = xma_frame_alloc(&fprops);

    // Create data buffer for encoderXmaDataBuffer
    buffer;
    buffer = xma_data_buffer_alloc(191080);
    ...
    ret = XMA_SEND_MORE_DATA;
    //send encoder frame
    if (ret == XMA_SEND_MORE_DATA) {
        ret = xma_enc_session_send_frame(enc_session, scl_frame);
        continue; // read next frame into scl_frame buffer}
    else if (ret == XMA_SUCCESS) {
        do {
            xma_enc_session_recv_data(enc_session, buffer, &data_size);
        }while(data_size == 0);
    }


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

As such, the application API is more flexible:

- xma_kernel_session_write
- xma_kernel_session_read

These routines take a list of XmaParameter objects which are type-length-value
objects.  A kernel implementing this interface must make known what parameters
are legal to the application developer via a document so that that right types
of parameters may be instantiated and passed to the write/read routines.
If using a kernel of this type, consult the kernel developer's documentation
to learn what XmaParameter types are expected to be passed in for write() and
what will be returned upon calling read().

Cleanup
~~~~~~~~~~~~
When runtime video processing has concluded, the application should destroy
each session.  Doing so will free the session to be used by another thread or
process and ensure that the kernel plugin has the opportunity to perform
proper cleanup/closing procedures.

- xma_enc_session_destroy()
- xma_dec_session_destroy()
- xma_scaler_session_destroy()
- xma_filter_session_destroy()
- xma_kernel_session_destroy()


Plugin Development Guide
-----------------------------

Overview
~~~~~~~~~~~~~~~~~~~~~~~~
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
kernel via the XMA configuration file 'pluginpath' property:

::

    SystemCfg:
        - dsa:        xilinx_1525_dynamic_5_1
        - pluginpath: /tmp/libxmaapi/installdir/share/libxmaapi
        - xclbinpath: /tmp/xclbins
        - ImageCfg:
            xclbin:   hevc_encoder.xclbin
            zerocopy: disable
            device_id_map: [0]
            KernelCfg: [[ instances: 1,
                          function: encoder,
                          plugin: libhevc.so,
                          vendor: ACME,
                          name: hevc_encoder_1,
                          ddr_map: [0]]]


In the above example, the libhevc.so is an XMA plugin that is linked to the
encoder instance produced by the "ACME" company.  When an application requests
a resource through the XMA Application API, it will specify a specific type,
from the list of XmaEncoderType as well as a vendor name string.  Your
plugin will be linked to the vendor string as part of the YAML configuration
file (as indicated in the example above) and will specify the precise type (i.e.
XmaEncoderType) it is designed to control in its XMA kernel-specific plugin data
structure (e.g. see XmaEncoderPlugin::hwencoder_type).  If there is a
match, then your plugin will be called into service to implement control of
the kernel in response to the application interface.

See *xma_app_init_yaml* for more details about the system configuration file.

XMA Plugin Code Layout
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each XMA kernel type specifies a slightly different interface so these
guidelines are intended to cover what is generally common.

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
| session_create()    |  | alloc_chan()**             |
|                     |  | init()                     |
+---------------------+-------------------------------+
| send_(data|frame)() | | get_dev_input_paddr()**     |
|                     | | send_(data|frame)()         |
+---------------------+-------------------------------+
| recv_(data|frame)() |    recv_(data|frame)()        |
+---------------------+-------------------------------+
|  destroy()          |           close()             |
+---------------------+-------------------------------+

\** optional callback if specified in kernel interface

Using the XMA encoder plugin kernel type as an example (specified by
XmaEncoderPlugin) the following is a rough sketch of a simple plugin
implementation with most implementation details omitted for brevity:

::

    #include <stdio.h>
    #include <xmaplugin.h>


    static int32_t xlnx_encoder_init(XmaEncoderSessionenc_session)
    {
        //Gather plugin-specific data and properties
        EncoderContextctx = enc_session->base.plugin_data;
        XmaEncoderPropertiesenc_props = &enc_session->encoder_props;
        HostKernelCtxpKernelCtx = ((XmaSession*)enc_session)->kernel_data;
        ...
        //allocate device buffers for incoming and outgoing encoded data
        ctx->encoder.input_y_buffer[i].b_handle = xma_plg_buffer_alloc(hw_handle,
                                                      ctx->encoder.input_y_buffer[i].b_size);

        ctx->encoder.input_u_buffer[i].b_handle = xma_plg_buffer_alloc(hw_handle,
                                                      ctx->encoder.input_u_buffer[i].b_size);

        ctx->encoder.input_v_buffer[i].b_handle = xma_plg_buffer_alloc(hw_handle,
                                                      ctx->encoder.input_v_buffer[i].b_size);
        //alloc add'l buffers for outgoing data
        ...
        //initalize state of encoder based on enc_props via register_write
        ...
        //update private context data structuresctx andpKernelCtx
        ...
        return 0;
    }

    static int32_t xlnx_encoder_alloc_chan(XmaSessionpending, XmaSession*sessions, uint32_t sess_cnt)
    {
        // evaluate pending session loado on kernel vs existing sessions and reject/approve
        ...
        //approve new channel request and assign channel id
        pending->chan_id = sess_cnt;
        return 0;
    }

    static int32_t xlnx_encoder_send_frame(XmaEncoderSessionenc_session, XmaFrameframe)
    {
        EncoderContextctx = enc_session->base.plugin_data;
        XmaHwSession hw_handle = enc_session->base.hw_session;
        HostKernelCtxpKernelCtx = ((XmaSession*)enc_session)->kernel_data;
        uint32_t nb = 0;
        nb = ctx->n_frame % NUM_BUFFERS;

        //write frame properties to registers
        xma_plg_register_write(hw_handle, &(ctx->width), sizeof(uint32_t), ADDR_FRAME_WIDTH_DATA);
        xma_plg_register_write(hw_handle, &(ctx->height), sizeof(uint32_t), ADDR_FRAME_HEIGHT_DATA);
        xma_plg_register_write(hw_handle, &(ctx->fixed_qp), sizeof(uint32_t), ADDR_QP_DATA);
        xma_plg_register_write(hw_handle, &(ctx->bitrate), sizeof(uint32_t), ADDR_BITRATE_DATA);
        ...
        //additional register writes for frame processing...
        ...
        //copy host frame data to device memory for YUV buffer
        xma_plg_buffer_write(hw_handle,
                ctx->encoder.input_y_buffer[nb].b_handle,
                frame->data[0].buffer,
                ctx->encoder.input_y_buffer[nb].b_size, 0);

        xma_plg_buffer_write(hw_handle,
                ctx->encoder.input_u_buffer[nb].b_handle,
                frame->data[1].buffer,
                ctx->encoder.input_u_buffer[nb].b_size, 0);

        xma_plg_buffer_write(hw_handle,
                ctx->encoder.input_v_buffer[nb].b_handle,
                frame->data[2].buffer,
                ctx->encoder.input_v_buffer[nb].b_size, 0);
        //additonal register read to ensure data is processed
        ...
        return 0;
    }

    static int32_t xlnx_encoder_recv_data(XmaEncoderSessionenc_session, XmaDataBufferdata, int32_tdata_size)
    {
        EncoderContextctx = enc_session->base.plugin_data;
        XmaHwSession hw_handle = enc_session->base.hw_session;
        HostKernelCtxpKernelCtx = ((XmaSession*)enc_session)->kernel_data;
        int64_t out_size = 0;
        uint64_t d_cnt = 0;
        uint32_t nb = (ctx->n_frame) % NUM_BUFFERS;

        // Read the length of output data into out_size
        ...
        // Copy data to host buffer data->data.buffer
        xma_plg_buffer_read(hw_handle,
                            ctx->encoder.output_buffer[nb].b_handle,
                            data->data.buffer, out_size, 0);
        ...
        return 0;
    }

    static int32_t xlnx_encoder_close(XmaEncoderSessionenc_session)
    {
        EncoderContextctx = enc_session->base.plugin_data;
        XmaHwSession hw_handle = enc_session->base.hw_session;

        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            xma_plg_buffer_free(hw_handle, ctx->encoder.input_y_buffer[i].b_handle);
            xma_plg_buffer_free(hw_handle, ctx->encoder.input_u_buffer[i].b_handle);
            xma_plg_buffer_free(hw_handle, ctx->encoder.input_v_buffer[i].b_handle);
            xma_plg_buffer_free(hw_handle, ctx->encoder.output_buffer[i].b_handle);
        }
        return 0;
    }

    XmaEncoderPlugin encoder_plugin = {
        .hwencoder_type = XMA_H264_ENCODER_TYPE,
        .hwvendor_string = "Xilinx",
        .format = XMA_YUV420_FMT_TYPE,
        .bits_per_pixel = 8,
        .plugin_data_size = sizeof(EncoderContext),
        .kernel_data_size = sizeof(HostKernelCtx),
        .init = xlnx_encoder_init,
        .send_frame = xlnx_encoder_send_frame,
        .recv_data = xlnx_encoder_recv_data,
        .close = xlnx_encoder_close,
        .alloc_chan = xlnx_encoder_alloc_chan,
        .get_dev_input_paddr = NULL
    };


Note that each plugin implementation must statically allocate a data structure
with a specific name (as present on line 425 in the above example):

======================================== =========================================
              Plugin Type                      Required Global Variable Name
======================================== =========================================
           XmaDecoderPlugin                           decoder_plugin
           XmaEncoderPlugin                           encoder_plugin
           XmaFilterPlugin                            filter_plugin
           XmaScalerPlugin                            scaler_plugin
           XmaKernelPlugin                            Kernel_plugin
======================================== =========================================


Initalization
~~~~~~~~~~~~~~~~~~~~

Initialization is the time for a plugin to perform one or more of the
following:
* evaluate an application request for a kernel channel (optional)
* allocate device buffers to handle input data as well as output data
* initalize the state of the kernel

When an application creates a session (e.g. xma_enc_session_create()), the
plugin code will have the following callbacks invoked:

1. alloc_chan (optional)
2. init

What is returned to the application code is a session object corresponding
to the type of session requested (e.g. XmaEncoderSession).  All
session objects derive from a base class: XmaSession.  These session
data structures contain all of the instance data pertaining to a kernel
and are used by the XMA library as well as plugin for storage and retrieval
of state information.

From the perspective of the application, an session object represents
control of a kernel instance.  This may, in fact, be an entire video kernel
or, in the case of a kernel that supports channels, a 'virtual'
kernel that is shared amongst more than one thread of execution.
If your kernel supports channels (i.e. a type of 'virtual' kernel),
then the alloc_chan() callback must be implemented.  The signature
for alloc_chan includes an array of existing XmaSession objects that
have been previously allocated to this kernel as well as the
currently pending request.  It is your responsibility, as the plugin
developer, to decide if the pending request can be approved or rejected.
Approval should include updating the XmaSession::chan_id member with
a non-negative channel id and an XMA_SUCCESS return code.

Your init function will then be called after alloc_chan (assuming it was
implemented).  Within your init() implementation, you will be expected
to intialize any private session-specific data structures,
kernel-specific data structures, allocate device memory for holding
incoming data as well as for holding outgoing data and program
the registers of the kernel to place it into an initial state ready
for processing data.

When your plugin is first loaded, XMA will allocate memory
for kernel-wide data based on the size you
specify in your plugin. This data is considered global for all
sessions sharing a given kernel (if the kernel supports this
via channels) and should be protected from simultaneous access.

When a session has been created in response to an application request,
XMA will allocate plugin data that
is session-specific.

These XmaSession::kernel_data and XmaSession::plugin_data members are
available to you to store the necessary kernel-wide and session-specific
state as necessary. There is no need to free these data structures during
termination; XMA frees this data for you.

The XMA Plugin Library provides a set of functions to allocating
device memory and performing register reads and writes.
To allocate buffers necessary to handle both incoming and outgoing
data, please see xma_plg_buffer_alloc().

See `xmaplugin`_ for more details.

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

xma_plg_register_write() and xma_plg_register_read() can be used to program
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

xma_plg_register_write() and xma_plg_register_read() can be used to program
the kernel registers and start kernel processing.

Termination
~~~~~~~~~~~~~~

When an XMA application has concluded data processing, it will destroy its
kernel session.  Your close() callback will be invoked to perform the necessary
cleanup.  Your close() implementation should free any buffers that were
allocated in device memory during your init() via xma_plg_buffer_free().
Freeing XmaSession::kernel_data and XmaSession::plugin_data is not necessary
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
buffer for the encoder. This optimization is known as 'zerocopy'. The
encoder must implement the XmaEncoderPlugin::get_dev_input_paddr() callback.
The XMA library can detect whether the two kernel sessions are capable of
sharing buffers.  The following conditions will be checked:

1. Both kernel sessions are connected to the same device DDR bank
2. The get_dev_input_paddr() callback is implemented by the encoder session
3. The encoder has been configured to expect frame data that is same format and size as the upstream filter kernel is producing as output.
4. The system configuration file has specified that zerocopy is 'enabled'

If all of the above conditions are true, zero-copy between the kernels will
be supported.  The XMA library will obtain the destination buffer address
for the filter from the encoder session.  This will then be provided as the
destination address to the filter's XmaFrame argument as part of its
recv_frame() callback.


xma
-------------
.. include:: ../core/xma.rst


xmaplugin
------------------
1. Intro from xmaplugin.h
2. xmaplugin.h autogenerated

.. include:: ../core/xmaplugin.rst
