/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#ifndef _XMA_H_
#define _XMA_H_

#include "app/xmabuffers.h"
#include "app/xmalogger.h"
#include "app/xmadecoder.h"
#include "app/xmaencoder.h"
#include "app/xmaerror.h"
#include "app/xmascaler.h"
#include "app/xmafilter.h"
#include "app/xmakernel.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @defgroup xma_app_intf XMA Application Interface
 * The interface used by stand-alone XMA applications or plugins
*/

/**
 * @ingroup xma_app_intf
 * @file xma.h
 * The primary header file that defines and provides access to the XMA
*/

/**
 * @ingroup xma_app_intf
 * @addtogroup xma xma.h
 * @{
*/

/**
 *  @brief Initialize the system according to the layout specified in the
 *      YAML configuration file.
 *
 *  This is the entry point routine for utilzing the XMA library and must be
 *  the first call within any application before calling any other XMA APIs.
 *  The YAML file is parsed and then verified for compatibility with the system
 *  hardware.  If deemed compatible, each device specified in the YAML file
 *  will be programmed with the xclbin(s) specified in the YAML.  A shared
 *  memory file will be created in /tmp  which will store the contents of
 *  the YAML file *  and serve as a resource database tracking allocation of
 *  kernels thus permitting multiple processes to share device resources.  If
 *  the system has already been configured by a prior process, then a successful
 *  return from this routine will map the *existing* resource database file to
 *  the calling processes; XMA will NOT attempt to reprogram any of the system
 *  devices if any device is in-use based on the prior configuration.
 *  In effect, programming and and configuration of the system will only occur
 *  when this routine is first invoked.  From the first invocation, so long as
 *  any running process is attached to and utilizing resources for an existing
 *  configuration, all subsequent invocations of this routine by any other
 *  process will be forced to use the existing configuration of the system;
 *  their configuration file argument will be ignored.
 *  When all currently running processes attached to a given resource file
 *  database have run to completion normally, the resource file will be deleted
 *  and a subsequent process invoking this routine will restart the parsing and
 *  programming of the system as would be true during initial invocation.
 *
 *  @param [in] cfgfile a filepath to the YAML configuration file describing
 *      the layout of the xclbin(s) and the devices to which the xclbin(s) are
 *      to be deployed. If a NULL value is passed, the XMA will use a default
 *      name and location: /etc/xma/xma_def_sys_cfg.yaml.  In all cases, a
 *      properly defined yaml configuration file must exist.
 *
 *  @return XMA_SUCCESS after successfully initializing the system and/or (if
 *      not the first process to invoke) mapping in the currently active system
 *      configuration.
 *  @return XMA_ERROR_INVALID if the YAML file is incompatible with the system
 *      hardware.
 *  @return XMA_ERROR for all other errors.
*/
int32_t xma_initialize(char *cfgfile);

/* @} */
#ifdef __cplusplus
}
#endif
#endif

/**
 * @mainpage Overview
 * @tableofcontents
 * @section intro_sec Introduction
 *
 * The Xilinx Media Accelerator (XMA) library (libxmaapi) is a host interface
 * meant to simplify the development of applications managing and controlling
 * video accelerators such as decoders, scalers, filters, and encoders. The
 * libxmaapi is comprised of two API interfaces: an application interface and a
 * plugin interface. The application API is a higher-level, generalized
 * interface intended for application developers responsible for integrating
 * control of Xilinx accelerators into software frameworks such as FFmpeg,
 * GStreamer, or proprietary frameworks. The plugin API is a lower level
 * interface intended for developers responsible for implementing hardware
 * control of specific Xilinx acceleration kernels. In general, plugins are
 * developed by kernel providers as these plugins are specialized user space
 * drivers that are aware of the low-level hardware interface.
 *
 * From a high-level perspective, the XMA sits between a media framework (i.e.
 * FFmpeg)  and the Xilinx runtime (XRT). In addition, the XMA acts as a peer
 * to the host side implementation of OpenCL. The diagram below illustrates the
 * entire stack including an example of common accelerator kernels that are
 * possible in a specific design:
 *
 * @image html XMA-Stack.png
 *
 * The remaining sections will describe the key architectural aspects of the
 * libxmaapi and describe the high-level API along with the low-level plugin
 * API.
 *
 * @section xma_app XMA Application Interface Overview
 *
 * The API for the libxmaapi can be categorized into three areas:
 *
 *  @li Initialization
 *  @li Video frame processing
 *  @li Termination
 *
 * From an interface perspective, the high-level or upper edge interface and the
 * low-level or plugin interface are organized as follows:
 *
 * @image html XMA-Internal-Stack.png
 *
 * The diagram above illustrates a number of distinct API layers.  The XMA upper
 * edge initialization API provides two types of initialization: global and
 * session level initialization.  The XMA upper edge API also provides functions
 * for sending and receiving frames as well as a method for gracefully terminating
 * a video stream when the end of the stream is found.  Also depicted in the
 * diagram is the XMA Framework and resource manager.  The XMA Framework and
 * resource manager are responsible for managing the state of the system,
 * delegating requests to the appropriate plugin, and selecting available
 * resources based on session creation requests.
 *
 * See the @ref app_dev_guide for more information about utilizing the XMA
 * application interface to development your own stand alone or integrated
 * applications.
 *
 * @section xma_plg XMA Plugin Interface Overview
 *
 * The XMA lower edge API parallels the upper edge API; however, the lower edge
 * API is comprised of function callbacks similar to those used in a driver or as
 * defined in the FFmpeg plugin interface.
 *
 * There are five classes of XMA plugin interfaces: decoders, encoders,
 * filters, scalers, and a generic 'kernel' class.
 * Since each of these classes are unique in terms of the processing performed,
 * the APIs are slightly different, however, there is a common pattern associated
 * with these classes. Specifically, a plugin must provide registration
 * information and must implement all required callback functions. In general, an
 * XMA plugin implements at least four required callback functions: initialize,
 * send frame or send data, receive frame or receive data, and close. In addition
 * to these required functions, the encoder plugin API offers two optional
 * callbacks for channel allocation and retrieving the physical address of an
 * available device input buffer. The channel allocation callback is needed for
 * encoders that support multiple channels within a single kernel by time-division
 * multiplexing of the underlying accelerator resources. The retrieval of a
 * physical address of a device input buffer is needed when an encoder offers
 * zero-copy support as this buffer address is required by the kernel preceding
 * the encoder as the output buffer.
 *
 * By way of example, the following represents the interface of the XMA Encoder
 * class:
 *
 * @code
 * typedef struct XmaEncoderPlugin
 * {
 *  XmaEncoderType  hwencoder_type;
 *  const char     *hwvendor_string;
 *  XmaFormatType   format;
 *  int32_t         bits_per_pixel;
 *  size_t          kernel_data_size;
 *  size_t          plugin_data_size;
 *  int32_t         (*init)(XmaEncoderSession *enc_session);
 *  int32_t         (*send_frame)(XmaEncoderSession *enc_session,
 *                                XmaFrame          *frame);
 *  int32_t         (*recv_data)(XmaEncoderSession  *enc_session,
 *                               XmaDataBuffer      *data,
 *                               int32_t            *data_size);
 *  int32_t         (*close)(XmaEncoderSession *session);
 *  int32_t         (*alloc_chan)(XmaSession *pending_sess,
 *                                XmaSession **curr_sess,
 *                                uint32_t sess_cnt);
 *  uint64_t        (*get_dev_input_paddr)(XmaEncoderSession *enc_session);
 * } XmaEncoderPlugin;
 * @endcode
 *
 * Finally, the XMA offers a set of buffer management utilities that includes
 * the creation of frame buffers and encoded data buffers along with a set of
 * miscellaneous utility functions. By providing XMA buffer management
 * functions, it is possible for an XMA plugin to easily integrate with
 * virtually any higher-level media framework without requiring any
 * changes. Instead, it is up to the upper level media framework functions to
 * convert buffers into the appropriate XMA buffer.
 * The sections that follow will describe the layers of the API in more detail and
 * provide examples of how these functions are called from both the perspective of
 * an application and from the perspective of an XMA plugin. For the low-level
 * details of the APIs, please consult the doxygen documentation.
 *
 * See the @ref plg_dev_guide for more information about developing XMA kernel plugins
 *
 * @section xma_app_sub Sequence of Operations
 *
 * In order to better understand how XMA integrates with a standard multi-media
 * framework such as FFmpeg, the sequence diagram that follows identifies the
 * critical operations and functions called as part of a hypothetical encoder. The
 * diagram only calls out the initialization and processing stages:
 *
 * @image html XMA-Sequence-Diagram.png
 *
 * As shown in the diagram above, the system is comprised of five blocks:
 * <ol>
 *     <li> The FFmpeg Command Line application that is used to create a processing
 *          graph</li>
 *     <li> The FFmpeg encoder plugin that interfaces with the XMA Upper Edge
 *          Interface to manage a video session</li>
 *     <li> The XMA Upper Edge library interface responsible for initialization,
 *          resource allocation, and dispatching of the XMA plugin</li>
 *     <li> The XMA Lower Edge plugin responsible for interfacing with the SDAccel
 *          Video Kernel</li>
 *     <li> The XMA Video Kernel responsible for accelerating the encoding function</li>
 *  </ol>
 *
 * While this sequence diagram only shows five components, more complex systems
 * can be developed that include multiple accelerators with the associated XMA
 * plugin and FFmpeg plugin. In fact, adding new processing blocks is controlled
 * entirely by the FFmpeg command line and the presence of the requested
 * accelerator kernels. No additional development is required if all of the
 * SDAccel kernels are available along with the associated plugins.  In this
 * example, an FFmpeg command is invoked that ingests an MP4 file encoded as H.264
 * and re-encodes the file as H.264 at a lower bit rate. As a result, the main()
 * function of the FFmpeg command is invoked and this calls the xma_initialize()
 * function. The xma_initialize() function is called prior to executing any other
 * XMA functions and performs a number of initialization steps that are detailed
 * in a subsequent section.
 * Once the xma_initialize() successfully completes, the FFmpeg main() function
 * performs initialization of all requested processing plugins. In this case, the
 * hypothetical encoder plugin has been registered with FFmpeg and the
 * initialization callback of the plugin is invoked. The FFmpeg encoder plugin
 * begins by creating an XMA session using the xma_enc_session_create() function.
 * The xma_enc_session_create() function finds an available resource based on the
 * properties supplied and, assuming resources are available, invokes the XMA
 * plugin initialization function. The XMA plugin initialization function
 * allocates any required input and output buffers on the device and performs
 * initialization of the SDAccel kernel if needed.
 *
 * After initialization has completed, the FFmpeg main() function reads encoded
 * data from the specified file, decodes the data in software, and sends the raw
 * video frame to the FFmpeg plugin for encoding by calling the encode2() plugin
 * callback. The encode2() callback function converts the AVFrame into an XmaFrame
 * and forwards the request to the XMA Upper Edge interface via the
 * xma_enc_session_send_frame() function. The xma_enc_session_send_frame()
 * function locates the corresponding XMA plugin and invokes the send frame
 * callback function of the plugin. The XMA send frame callback function writes
 * the frame buffer data to a pre-allocated DDR buffer on the device and launches
 * the kernel. After the FFmpeg plugin encode2() function has sent the frame for
 * encoding, the next step is to determine if encoded data can be received or if
 * another raw frame should be sent. In most cases, an encoder will want several
 * raw frames before providing encoded data. Supplying multiple frames before
 * generated encoded data improves video quality through a look ahead and improves
 * performance by allowing new frame data to be written to the device DDR in
 * parallel with processing previously supplied frames.  Assuming a frame is ready
 * to be received, the xma_enc_session_recv_data() function is called by the
 * FFmpeg plugin and in turn results in the receive data function of the XMA
 * plugin being invoked. The XMA plugin communicates with the kernel to ensure
 * that data is ready to be received, determines the length of the encoded data,
 * and reads the encoded data from DDR device memory to host memory.
 * The description above is meant as a high-level introduction to FFmpeg and XMA.
 * The remainder of this document covers these topics in more depth and provides
 * code examples to help illustrate usage of the XMA.
 *
 *
 * @page app_dev_guide Application Development Guide
 * @tableofcontents
 * The XMA application interface is used to provide an API that can
 * be used to control video accelerators.  The XMA API operations
 * fall into three categories:
 * <ol>
 * <li> Initialization</li>
 * <li> Runtime frame/data processing</li>
 * <li> Cleanup</li>
 * </ol>
 *
 * @section xma_app_init Initialization
 *
 * The first act an application must perform is that of initialization of the
 * system environment.  This is accomplished by calling xma_initialize() and
 * passing in a string that represents the filepath to your system configuration
 * file.  This system configuration file, described in more detail below, serves
 * as both information about the images you will be deploying as well as
 * instructions to XMA with regard to which devices will be programmed with
 * a given image.
 *
 * Once the system has been configured according to the instructions in your
 * system configuration file, the next step is allocate and initialize the video
 * kernels that will be required for your video processing pipeline.  Each class
 * of video kernel supported by XMA has its own initialization routine and
 * a set of properties that must be populated and passed to this routine to
 * allocate and initialize a video kernel.  Both system wide initialization and
 * kernel initialization are detailed in the next two sections.
 *
 * @subsection xma_app_init_yaml XMA System Configuration File
 *
 * System configuration is described by a file conforming to YAML syntax
 * (http://www.yaml.org).  This file contains instructions for the XMA system
 * initialization as well as description(s) of the kernel contents of the xclbin
 * image file(s).  The configuration file consists of two logial parts:
 * \li System paths to required libraries and binary files (e.g. pluginpath)
 * \li One or more image deployment plans and descriptions (i.e. ImageCfg)
 *
 * Below is a sample configuration file describing a simple system
 * configuration for a single device utilizing an image file containing a
 * single HEVC encoder kernel:
 *
 * @code
 * SystemCfg:
 *     - dsa:        xilinx_1525_dynamic_5_1
 *     - pluginpath: /tmp/libxmaapi/installdir/share/libxmaapi
 *     - xclbinpath: /tmp/xclbins
 *     - ImageCfg:
 *         xclbin:   hevc_encoder.xclbin
 *         zerocopy: disable
 *         device_id_map: [0]
 *         KernelCfg: [[ instances: 1,
 *                       function: encoder,
 *                       plugin: libhevc.so,
 *                       vendor: ACME,
 *                       name: hevc_encoder_1,
 *                       ddr_map: [0]]]
 * @endcode
 *
 * Because this file is parsed using YAML syntax, the indentation present in
 * this example is mandatory for showing the relationships between the data.
 *
 * The system information comes first and includes the path to the directory
 * of the XMA plugin libraries as well as a directory to the xclbin files (aka
 * images).  After this system information will be one or more image
 * descriptions. Each image description, denoted by the 'ImageCfg' key,
 * instructs XMA as to which devices should be programmed with the given image
 * file. In the example above, we are deploying only to device '0' (devices are
 * enumerated as positive integers starting from 0).  In addition, a
 * description of the kernels that are included in the image is also a part of
 * the image description and will be used by XMA for tracking kernel resources.
 *
 * The configuration file is hierarchial and must conform to YAML
 * syntax as well as include the requisite keys else an error will be thrown
 * indicating what is missing/mistaken.
 *
 * In Backus-Naur Form, the grammar of the YAML file could be described as
 * follows:
 *
 * <pre>
 * [SystemCfg]    ::= SystemCfg:CRLF HTAB[dsa]CRLF HTAB[pluginpath]CRLF
 *                    HTAB[xclbinpath]CRLF (HTAB[ImageCfg])+
 * [dsa]          ::= dsa:[name_string]
 * [pluginpath]   ::= pluginpath:[filepath]
 * [xclbinpath]   ::= xclbinpath:[filepath]
 * [ImageCfg]     ::= ImageCfg:CRLF HTAB*2[zerocopy]CRLF HTAB*2[device_id_map]CRLF
 *                    HTAB*2[KernelCfg]CRLF
 * [zerocopy]     ::= zerocopy:(enable | disable)
 * [device_id_map]::= device_id_map:[number_list] CRLF
 * [KernelCfg]    ::= KernelCfg:%5B (%5B HTAB*3[instances]CRLF HTAB*3[function]CRLF
 *                    HTAB*3[plugin]CRLF  HTAB*3[vendor]CRLF HTAB*3[name]CRLF
 *                    HTAB*3[ddr_map]CRLF %5D)+ %5D
 * [instances]    ::= instances:digit+
 * [function]     ::= encoder | scaler | decoder | filter | kernel
 * [plugin]       ::= plugin:[name_string]
 * [vendor]       ::= vendor:[name_string]
 * [name]         ::= name:[name_string]
 * [ddr_map]      ::= ddr_map:[number_list]
 * [filepath]     ::= (%2F(vchar)*)+
 * [name_string]  ::= (vchar)+
 * [number_list]  ::= %5B digit+[,(digit)+]*%5D
 * </pre>
 *
 * A description of each YAML key:
 *
 * @param SystemCfg  Mandatory header property.  Takes no arguments.
 * @param dsa        Property of SystemCfg; The name of the "Dynamic System Archive"
 *     used for all images.
 * @param pluginpath Property of SystemCfg; The path to directory containing all
 *     plugin libraries (typically \<libxmaapi install dir\>/share/libxmaapi)
 * @param xclbinpath Property of SystemCfg; The path to the directory containing
 *     the hardware binary file(s) that will be used to program the devices on the
 *     system.
 * @param ImageCfg   Property of SystemCfg; Mandatory sub-header property
 *      describing an xclbin image as well as specifying to which device(s) is shall
 *      be deployed.
 * @param xclbin     Property of ImageCfg; The xclbin filename that comprises this
 *     image to be deployed to the specified devices in device_id_map.
 * @param zerocopy   Property of ImageCfg; Either the bare word 'enable' or 'disable'.
 *     If set to 'enable', indicates that zerocopy between kernels will be attempted
 *     if possible (requires both kernels to be connected to the same device
 *     memory -- see @ref xma_plg_guide_zerocopy).
 * @param device_id_map Property of ImageCfg; An array of numeric device ids
 *     (0-indexed) indicating which fpga devices will be programmed with the xclbin.
 *     Note: if a device id specified is > than the number of actual devices on the
 *     system, initalization will fail and an error message will be logged.
 * @param KernelCfg  Property denoting the start of array of kernel entries
 *     contained in the xclbin.
 * @param instances  Propery of KernelCfg; identifies the number of kernels of a
 *     a specific type included in this xclbin.  IMPORTANT: The order of the
 *     kernel entries MUST MATCH the order of base addresses in which the kernels
 *     are assigned in a given xclbin.  Lowest base address must be described first.
 * @param function   Either 'encoder','scaler','decoder','filter' or 'kernel' as
 *     appropriate for this kernel entry.
 * @param plugin     Then name of the XMA plugin library that will be mapped to
 *     this kernel entry; used by XMA to route high level application calls to the
 *     appropriate XMA plugin driver.
 * @param vendor     Name of the vendor that authored this kernel.  Important for
 *     session creation as the vendor string is used by application code to, in
 *     part, identify which kernel entry is being requested for a given session.
 * @param name       The name, as it appears in the xclbin, of this kernel entry.
 *     Not used as this time.
 * @param ddr_map    An array of integer values indicating a mapping of
 *     kernel instances to DDR banks.  This MUST MATCH the number of kernel
 *     instances indicated for this entry.
 *
 * Below is a sample of a more complex, multi-image YAML configuration file:
 * @code
 * SystemCfg:
 *     - dsa:        xilinx_xil-accel-rd-vu9p_4ddr-xpr_4_2
 *     - pluginpath: /plugin/path
 *     - xclbinpath: /xcl/path
 *     - ImageCfg:
 *         xclbin: filename1.xclbin
 *         zerocopy: enable
 *         device_id_map: [0,1]
 *         KernelCfg: [[ instances: 2,
 *                       function: HEVC,
 *                       plugin:  libhevc.so,
 *                       vendor: ACME,
 *                       name:   hevc_kernel,
 *                       ddr_map: [0,0]],
 *                     [ instances: 1,
 *                       function: Scaler,
 *                       plugin: libxscaler.so,
 *                       vendor: Xilinx,
 *                       name: xlnx_scaler_kernel,
 *                       ddr_map: [0]]]
 *     - ImageCfg:
 *         xclbin: filename2.xclbin
 *         zerocopy: disable
 *         device_id_map: [2]
 *         KernelCfg: [[ instances: 1,
 *                       function: H264,
 *                       plugin:  libxlnxh264.so,
 *                       vendor: Xilinx,
 *                       name: H264_E_KERNEL,
 *                       ddr_map: [0]]]
 *
 * @endcode
 *
 * In the above example, two images are described.  XMA will deploy the
 * filename1.xclbin to devices 0 and 1. The first image consists of three kernels:
 * two hevc kernels mapped to DDR banks 0 and 0.  The third kernel is the video
 * scaler.  The second image file is instructed to be deployed to device 2 and
 * consists of a single h264 kernel mapped to ddr bank 0.
 *
 * This YAML file will be consumed by the application code as the first step in
 * the initalization process.
 *
 * @subsection xma_app_init_sys XMA Initalization
 *
 * The prior section described the components of a proper configuration file
 * necessary for describing the planned initialization of the system.  Herein,
 * we describe the proper XMA API calls to both initialize the system
 * with your properly prepared YAML system configuration file as well as the
 * to allocate and initialize one or more video kernels.
 *
 * Initialization has two parts and must be performed in the following order:
 * <ol>
 * <li> system initialization wherein all devices are programmed with images as
 *     described by the XMA system configuration file</li>
 * <li> kernel initialization wherein a specific kernel resource is
 *     initialized for video processing</li>
 * </ol>
 *
 * All application code must include the following header file to access the
 * XMA application interface:
 * @code
 * #include <xma.h>
 * @endcode
 *
 * This header will pull in all files located in [include_dir]/app/ which,
 * collectively, defines the complete application interface and datastructures
 * required for XMA development.
 *
 * The first step for any XMA application is to initalize the system with the
 * system configuraton file:
 *
 * @code
 * //prior includes
 * ...
 * #include <xma.h> // XMA application interface
 *
 * int main(void) {
 *
 *     int rc;
 *     char *my_yaml_path = "/tmp/xma_sys_cfg.yaml";
 *
 *     rc = xma_initalize(my_yaml_path);
 * ...
 *
 * }
 * @endcode
 *
 * The above code will program all devices on the system as defined in the
 * xma_sys_cfg.yaml.  The name of the configuration file is arbitrary and you
 * may have multiple configuration files.  However, only the first invocation
 * of xma_initialize will result in programming of the system.  Any subsequent
 * invocation is idempotent.  If another process attempts to initalize the
 * system (or the same program is invoked a 2nd time) while the original
 * process that initialized the system is still active, the existing system
 * configuration will be utilized by the 2nd process; device programming will
 * only ever occur once.  When all processes connected to the original system
 * configuration have terminated, the process of initialization with a new YAML
 * file can begin anew when a later process calls xma_initalize() with a new
 * system configuration file.
 *
 * Once the system has been initialized, then kernel sessions can be allocated.
 *
 * Each kernel class (i.e. encoder, filter, decoder, scaler, filter, kernel)
 * requires different properties to be specified before a session can be created.
 *
 * See the document for the corresponding module for more details for a given
 * kernel type:
 * @li @ref xmadec
 * @li @ref xmaenc
 * @li @ref xmafilter
 * @li @ref xmascaler
 * @li @ref xmakernel
 *
 * The general initialzation sequence that is common to all kernel classes
 * is as follows:
 * \li define key type-specific properties of the kernel to be initialized
 * \li call the *_session_create() routine corresponding to the
 *     kernel (e.g. xma_enc_session_create())
 *
 * Using the decoder kernel as an example, the following code defines
 * a request for an H264 decoder kernel made by Xilinx:
 *
 * @code
 * #include <xma.h>
 * ...
 * // init system via yaml file
 * ...
 * // Setup decoder properties
 * XmaDecoderProperties dec_props;
 *
 * dec_props.hwdecoder_type = XMA_H264_DECODER_TYPE;
 * strcpy(dec_props.hwvendor_string, "Xilinx");
 *
 * // Create a decoder session based on the requested properties
 * XmaDecoderSession *dec_session;
 * dec_session = xma_dec_session_create(&dec_props);
 * if (!dec_session)
 * {
 *     // Log message indicating session could not be created
 *     // return from function
 * }
 * ...
 * @endcode
 *
 * What is returned is a reference to a session object (XmaDecoderSession in
 * the case of the above example).  This will serve as an opqaue object handle
 * that you will pass to all other API routines interacting with this kernel.
 * A session represents control a single kernel.  Note that some kernels
 * may support 'channels' which are portions of a kernel resource that behave
 * like full kernels (i.e. in essence, a 'virtual' kernel).  The distinction,
 * is unimportant to the application developer; a session is a kernel resource
 * and functions as a dedicated kernel resource to the requesting process or
 * thread.  Note: channels of a given kernel may only be assigned to threads
 * from within a given process context. Multiple processes may not share
 * a kernel; channels from a single kernel may not be assigned to multiple
 * processes.
 *
 * @section xma_app_rt Runtime Frame and Data Processing
 *
 * Once system and kernel initalization (i.e. session creation) are complete,
 * video processing may commence.
 *
 * Most kernel types include routines to consume data and then produce data from
 * host memory buffers.  Depending on the nature of the kernel, you may be
 * required to send a frame and then receive data or vice versa.
 * XMA defines buffer data structures that correspond to frames (@ref XmaFrame)
 * or data (@ref XmaFrameData). These buffer structures are used to communicate
 * with the kernel application APIs and include addresses to host memory which
 * you will be required to allocate.  The XMA Application Interface includes
 * functions to allocate data from host memory and create these containers for
 * you.  See xmabuffers.h for additional information.
 *
 * Continuing with our decoder example, the two runtime routines for data
 * processing are:
 * \li xma_dec_session_send_data()
 * \li xma_dec_session_recv_frame()
 *
 * Calling the send_data() routine and following with recv_frame() will form
 * the body of your runtime processing code.
 *
 * If, by contrast, we examine the XMA Encoder library, we see the following
 * two routines:
 * \li xma_enc_session_send_frame()
 * \li xma_enc_session_recv_data()
 *
 * The idea is the same as that of the decoder: send data to be processed, then
 * receive the data.
 *
 * @code
 * int ret, data_size = 0;
 * ...
 * // XMA init code and enc_session
 * ...
 * // Create an input frame
 * XmaFrameProperties fprops;
 * fprops.format = XMA_YUV420_FMT_TYPE;
 * fprops.width = 1920;
 * fprops.height = 1080;
 * fprops.bits_per_pixel = 8;
 * XmaFrame *scl_frame = xma_frame_alloc(&fprops);
 *
 * // Create data buffer for encoder
 * XmaDataBuffer *buffer;
 * buffer = xma_data_buffer_alloc(1920 * 1080);
 * ...
 * ret = XMA_SEND_MORE_DATA;
 * //send encoder frame
 * if (ret == XMA_SEND_MORE_DATA) {
 *     ret = xma_enc_session_send_frame(enc_session, scl_frame);
 *     continue; // read next frame into scl_frame buffer
 * } else if (ret == XMA_SUCCESS) {
 *     do {
 *         xma_enc_session_recv_data(enc_session, buffer, &data_size);
 *     }while(data_size == 0);
 * }
 * @endcode
 *
 * Some routines, such as that of the encoder, may require multiple frames of
 * data before recv_data() can be called.  You must consult the API to ensure
 * you check for the correct return code to know how to proceed.  In the case of
 * the encoder, calling xma_enc_session_send_frame() may return XMA_SEND_MORE_DATA
 * which is an indication that calling recv_data() will not yield any data as
 * more frames must be sent before any output data can be received.
 *
 * Of special note is the XmaKernel plugin type.  This kernel type is a generic
 * type and not necessarily video-specific. It is used to represent kernels that
 * perform control functions and/or other functions not easily represented by
 * any of the other kernel classes.
 *
 * As such, the application API is more flexible:
 * \li xma_kernel_session_write
 * \li xma_kernel_session_read
 *
 * These routines take a list of XmaParameter objects which are type-length-value
 * objects.  A kernel implementing this interface must make known what parameters
 * are legal to the application developer via a document so that that right types
 * of parameters may be instantiated and passed to the write/read routines.
 * If using a kernel of this type, consult the kernel developer's documentation
 * to learn what XmaParameter types are expected to be passed in for write() and
 * what will be returned upon calling read().
 *
 * @section xma_app_cleanup Cleanup
 *
 * When runtime video processing has concluded, the application should destroy
 * each session.  Doing so will free the session to be used by another thread or
 * process and ensure that the kernel plugin has the opportunity to perform
 * proper cleanup/closing procedures.
 *
 * Each kernel type offers a *_session_destroy() function:
 *
 * \li xma_enc_session_destroy()
 * \li xma_dec_session_destroy()
 * \li xma_scaler_session_destroy()
 * \li xma_filter_session_destroy()
 * \li xma_kernel_session_destroy()
 *
*/
