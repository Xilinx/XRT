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
#ifndef _XMA_PLUGIN_H_
#define _XMA_PLUGIN_H_

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include <stddef.h>
#include "xma.h"
#include "lib/xmahw.h"
#include "lib/xmahw_hal.h"
#include "plg/xmasess.h"
#include "plg/xmadecoder.h"
#include "plg/xmaencoder.h"
#include "plg/xmascaler.h"
#include "plg/xmafilter.h"
#include "plg/xmakernel.h"

/**
 * @defgroup xma_plg_intf XMA Plugin Interface
 * The interface used by XMA kernel plugin developers
*/

/**
 * @ingroup xma_plg_intf xmaplugin
 * @file xmaplugin.h
 * Primary header file providing access to complete XMA plugin interface
 */

/**
 * @ingroup xma_plg_intf
 * @addtogroup xmaplugin xmaplugin.h
 * @{
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t  XmaBufferHandle;

/**
 *  @brief Allocate device memory
 *
 *  This function allocates memory on the FPGA DDR and
 *  provides a handle to the memory that can be used for
 *  copying data from the host to device memory or from
 *  the device to the host.  In addition, the handle
 *  can be passed to the function @ref xma_plg_get_paddr()
 *  in order to obtain the physical address of the buffer.
 *  Obtaining the physical address is necessary for setting
 *  the AXI register map with physical pointers so that the
 *  kernel knows where key input and output buffers are located.
 *
 *  This function knows which DDR bank is associated with this
 *  session and therefore automatically selects the correct
 *  DDR bank.
 *
 *  @param  s_handle The session handle associated with this plugin instance.
 *  @param  size     Size in bytes of the device buffer to be allocated.
 *
 *  @return          Non-zero buffer handle on success
 *
 */
XmaBufferHandle xma_plg_buffer_alloc(XmaHwSession s_handle, size_t size);

/**
 *  @brief Free a device buffer
 *
 *  This function frees a previous allocated buffer that was obtained
 *  using the @ref xma_plg_buffer_alloc() function.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param b_handle  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *
 */
void xma_plg_buffer_free(XmaHwSession s_handle, XmaBufferHandle b_handle);

/**
 *  @brief Get a physical address for a buffer handle
 *
 *  This function returns the physical address of DDR memory on the FPGA
 *  used by a specific session
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param b_handle  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *
 *  @return          Physical address of DDR on the FPGA
 *
 */
uint64_t xma_plg_get_paddr(XmaHwSession s_handle, XmaBufferHandle b_handle);

/**
 *  @brief Write data from host to device buffer
 *
 *  This function copies data from host to memory to device memory.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param b_handle  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *  @param src       Source data pointer
 *  @param size      Size of data to copy
 *  @param offset    Offset from the beginning of the allocated device memory
 *
 *  @return         XMA_SUCCESS on success
 *  @return         XMA_ERROR on failure
 *
 */
int32_t xma_plg_buffer_write(XmaHwSession     s_handle,
                             XmaBufferHandle  b_handle,
                             const void      *src,
                             size_t           size,
                             size_t           offset);

/**
 *  @brief Read data from device memory and copy to host memory
 *
 *  This function copies data from device memory and stores the result in
 *  the requested host memory
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param b_handle  The buffer handle returned from
 *                   @ref xma_plg_buffer_alloc()
 *  @param dst       Destination data pointer
 *  @param size      Size of data to copy
 *  @param offset    Offset from the beginning of the allocated device memory
 *
 *  @return         XMA_SUCCESS on success
 *  @return         XMA_ERROR on failure
 *
 */
int32_t xma_plg_buffer_read(XmaHwSession     s_handle,
                            XmaBufferHandle  b_handle,
                            void            *dst,
                            size_t           size,
                            size_t           offset);

/**
 * NOTE: Do not use this API. Instead use below mentioned xma_plg_ebo_kernel* APIs
 * 
 *  @brief Write kernel register(s)
 *
 *  This function writes the data provided and sets the specified AXI_Lite
 *  register(s) exposed by a kernel. The base offset of 0 is the beginning
 *  of the kernels AXI_Lite memory map as this function adds the required
 *  offsets internally for the kernel and PCIe.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param dst       Destination data pointer
 *  @param size      Size of data to copy
 *  @param offset    Offset from the beginning of the kernel AXI_Lite register
 *                   register map
 *
 *  @return          >=0 number of bytes written
 *  @return          <0 on failure
 *
 */
int32_t xma_plg_register_write(XmaHwSession     s_handle,
                               void            *dst,
                               size_t           size,
                               size_t           offset) __attribute__ ((deprecated));




/**
 *  @brief execBO based kernel APIs
 *  xma_plg_register_write should NOT be used.
 *  Please use below APIs instead
 *  xma_plg_ebo_kernel_start
 *  xma_plg_ebo_kernel_done: Wait for all pending kernel commands to finish
 *  
 */
int32_t xma_plg_ebo_kernel_start(XmaHwSession  s_handle, uint32_t* args, uint32_t args_size);
int32_t xma_plg_ebo_kernel_done(XmaHwSession  s_handle);//Wait for all pending kernel commands to finish



/**
 *  @brief Read kernel registers
 *
 *  This function reads the register(s) exposed by the kernel. The base offset
 *  of 0 is the beginning of the kernels AXI_Lite memory map as this function
 *  adds the required offsets internally for the kernel and PCIe.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param dst       Destination data pointer
 *  @param size      Size of data to copy
 *  @param offset    Offset from the beginning of the kernel's AXI_Lite memory
 *                   map
 *
 *  @return          >=0 number of bytes read
 *  @return          <0 on failure
 *
 */
int32_t xma_plg_register_read(XmaHwSession     s_handle,
                              void            *dst,
                              size_t           size,
                              size_t           offset) __attribute__ ((deprecated));

/**
 *  @brief Dump kernel registers
 *
 *  This function dumps the registers for a kernel up to the number of words
 *  specified and prints them with the offset and value.
 *
 *  @param s_handle  The session handle associated with this plugin instance
 *  @param num_words Number of 32-bit words to dump
 *
 */
void xma_plg_register_dump(XmaHwSession     s_handle,
                           int32_t          num_words);
/**
 *  @}
 */

/**
 * @page plg_dev_guide Plugin Development Guide
 * @tableofcontents
 *
 * @section xma_plg_guide_ov Overview
 * The XMA Plugin Interface is used to write software capable of managing a
 * specific video kernel hardware resource.  The plugin interface consists of a
 * library for moving data between device memory and host memory and accessing
 * hardware registers.  Additionally, standard interfaces are defined to
 * represent various video kernel archtypes such as encoders, decoders, and
 * filters.
 *
 * The plugin developer, by implementing a given plugin interface, permits XMA
 * to translate requests from XMA applications into hardware-specific actions
 * (i.e. register programming, buffer processing). The XMA plugin is akin to a
 * software 'driver' in this regard.
 *
 * The first step in developing an XMA plugin requires you to decide which XMA
 * kernel interface accurately represents the type of hardware kernel for which
 * you seek to provide support:
 *
 * <table border="1">
 * <tr style="color:#fff; background: blue;"><th>Kernel Type</th><th>XMA Plugin Interface</th></tr>
 * <tr><td>Encoders (VP9, H.264, H.265)</td><td>@ref xmaplgenc</td></tr>
 * <tr><td>Decoders (VP9, H.264, H.265)</td><td>@ref xmaplgdec</td></tr>
 * <tr><td>Filters (colorspace converter, scalers)</td><td>@ref xmaplgfilter or @ref xmaplgscaler</td></tr>
 * <tr><td>Scalers</td><td>@ref xmaplgscaler</td></tr>
 * <tr><td>Other (embedded cpu)</td><td>@ref xmaplgkernel</td></tr>
 * </table>
 *
 * Once selected, the job of the plugin author is to implement the interface
 * for the given kernel thus providing a mapping between the @ref xma_app_intf and
 * the kernel.  Most callbacks specified are implicitly mandatory with some exceptions
 * which will be noted below.
 *
 * Your plugin will be compiled into a shared object library and linked to the
 * kernel via the XMA configuration file 'pluginpath' property:
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
 * In the above example, the libhevc.so is an XMA plugin that is linked to the
 * encoder instance produced by the "ACME" company.  When an application requests
 * a resource through the XMA Application API, it will specify a specific type,
 * from the list of @ref XmaEncoderType as well as a vendor name string.  Your
 * plugin will be linked to the vendor string as part of the YAML configuration
 * file (as indicated in the example above) and will specify the precise type (i.e.
 * XmaEncoderType) it is designed to control in its XMA kernel-specific plugin data
 * structure (e.g. see @ref XmaEncoderPlugin::hwencoder_type).  If there is a
 * match, then your plugin will be called into service to implement control of
 * the kernel in response to the application interface.
 *
 * See @ref xma_app_init_yaml for more details about the system configuration file.
 *
 * @section xma_plg_guide_code_layout XMA Plugin Code Layout
 *
 * Each XMA kernel type specifies a slightly different interface so these
 * guidelines are intended to cover what is generally common.
 *
 * All plugin code must include xmaplugin.h
 * @code
 * #include <xmaplugin.h>
 * @endcode
 *
 * This will provide the plugin code access to all data structures necessary
 * to author XMA plugin code.  This includes access to the structures used
 * by the @ref xma_app_intf as xmaplugin.h includes xma.h.
 *
 * What follows is a general description of what is expected of a plugin in
 * response to the @ref xma_app_intf.
 *
 * From the application perspective, the following operations
 * will be peformed:
 *
 * 1. Create session
 * 2. Send data/frame or write**
 * 3. Receive data/frame or read**
 * 4. Destroy
 *
 * \** in the case of a non-video kernel
 *
 * Steps 2 and 3 will form the runtime processing of frames/data and likely
 * repeated for as long as there is data to be processed.
 *
 * A general mapping between the application interface and plugin interface:
 *
 * <table border="1">
 * <tr style="color:#fff; background: blue;"><th>Application Call</th><th>Plugin Callbacks Invoked</th></tr>
 * <tr><td>session_create()</td><td>alloc_chan()**<br>init()</td></tr>
 * <tr><td>send_(data|frame)()</td><td>get_dev_input_paddr()**<br>send_(data|frame)()</td></tr>
 * <tr><td>recv_(data|frame)()</td><td>recv_(data|frame)()</td></tr>
 * <tr><td>destroy()</td><td>close()</td></tr>
 * </table>
 *
 * \** optional callback if specified in kernel interface
 *
 * Using the XMA encoder plugin kernel type as an example (specified by
 * XmaEncoderPlugin) the following is a rough sketch of a simple plugin
 * implementation with most implementation details omitted for brevity:
 *
 * @code
 * #include <stdio.h>
 * #include <xmaplugin.h>
 *
 *
 * static int32_t xlnx_encoder_init(XmaEncoderSession *enc_session)
 * {
 *     //Gather plugin-specific data and properties
 *     EncoderContext *ctx = enc_session->base.plugin_data;
 *     XmaEncoderProperties *enc_props = &enc_session->encoder_props;
 *     HostKernelCtx *pKernelCtx = ((XmaSession*)enc_session)->kernel_data;
 *     ...
 *     //allocate device buffers for incoming and outgoing encoded data
 *     ctx->encoder.input_y_buffer[i].b_handle = xma_plg_buffer_alloc(hw_handle,
 *                                                   ctx->encoder.input_y_buffer[i].b_size);
 *
 *     ctx->encoder.input_u_buffer[i].b_handle = xma_plg_buffer_alloc(hw_handle,
 *                                                   ctx->encoder.input_u_buffer[i].b_size);
 *
 *     ctx->encoder.input_v_buffer[i].b_handle = xma_plg_buffer_alloc(hw_handle,
 *                                                   ctx->encoder.input_v_buffer[i].b_size);
 *     //alloc add'l buffers for outgoing data
 *     ...
 *     //initalize state of encoder based on enc_props via register_write
 *     ...
 *     //update private context data structures *ctx and *pKernelCtx
 *     ...
 *     return 0;
 * }
 *
 * static int32_t xlnx_encoder_alloc_chan(XmaSession *pending, XmaSession **sessions, uint32_t sess_cnt)
 * {
 *     // evaluate pending session loado on kernel vs existing sessions and reject/approve
 *     ...
 *     //approve new channel request and assign channel id
 *     pending->chan_id = sess_cnt;
 *     return 0;
 * }
 *
 * static int32_t xlnx_encoder_send_frame(XmaEncoderSession *enc_session, XmaFrame *frame)
 * {
 *     EncoderContext *ctx = enc_session->base.plugin_data;
 *     XmaHwSession hw_handle = enc_session->base.hw_session;
 *     HostKernelCtx *pKernelCtx = ((XmaSession*)enc_session)->kernel_data;
 *     uint32_t nb = 0;
 *     nb = ctx->n_frame % NUM_BUFFERS;
 *
 *     //write frame properties to registers
 *     xma_plg_register_write(hw_handle, &(ctx->width), sizeof(uint32_t), ADDR_FRAME_WIDTH_DATA);
 *     xma_plg_register_write(hw_handle, &(ctx->height), sizeof(uint32_t), ADDR_FRAME_HEIGHT_DATA);
 *     xma_plg_register_write(hw_handle, &(ctx->fixed_qp), sizeof(uint32_t), ADDR_QP_DATA);
 *     xma_plg_register_write(hw_handle, &(ctx->bitrate), sizeof(uint32_t), ADDR_BITRATE_DATA);
 *     ...
 *     //additional register writes for frame processing...
 *     ...
 *     //copy host frame data to device memory for YUV buffer
 *     xma_plg_buffer_write(hw_handle,
 *             ctx->encoder.input_y_buffer[nb].b_handle,
 *             frame->data[0].buffer,
 *             ctx->encoder.input_y_buffer[nb].b_size, 0);
 *
 *     xma_plg_buffer_write(hw_handle,
 *             ctx->encoder.input_u_buffer[nb].b_handle,
 *             frame->data[1].buffer,
 *             ctx->encoder.input_u_buffer[nb].b_size, 0);
 *
 *     xma_plg_buffer_write(hw_handle,
 *             ctx->encoder.input_v_buffer[nb].b_handle,
 *             frame->data[2].buffer,
 *             ctx->encoder.input_v_buffer[nb].b_size, 0);
 *     //additonal register read to ensure data is processed
 *     ...
 *     return 0;
 * }
 *
 * static int32_t xlnx_encoder_recv_data(XmaEncoderSession *enc_session, XmaDataBuffer *data, int32_t *data_size)
 * {
 *     EncoderContext *ctx = enc_session->base.plugin_data;
 *     XmaHwSession hw_handle = enc_session->base.hw_session;
 *     HostKernelCtx *pKernelCtx = ((XmaSession*)enc_session)->kernel_data;
 *     int64_t out_size = 0;
 *     uint64_t d_cnt = 0;
 *     uint32_t nb = (ctx->n_frame) % NUM_BUFFERS;
 *
 *     // Read the length of output data into out_size
 *     ...
 *     // Copy data to host buffer data->data.buffer
 *     xma_plg_buffer_read(hw_handle,
 *                         ctx->encoder.output_buffer[nb].b_handle,
 *                         data->data.buffer, out_size, 0);
 *     ...
 *     return 0;
 * }
 *
 * static int32_t xlnx_encoder_close(XmaEncoderSession *enc_session)
 * {
 *     EncoderContext *ctx = enc_session->base.plugin_data;
 *     XmaHwSession hw_handle = enc_session->base.hw_session;
 *
 *     for (int i = 0; i < NUM_BUFFERS; i++)
 *     {
 *         xma_plg_buffer_free(hw_handle, ctx->encoder.input_y_buffer[i].b_handle);
 *         xma_plg_buffer_free(hw_handle, ctx->encoder.input_u_buffer[i].b_handle);
 *         xma_plg_buffer_free(hw_handle, ctx->encoder.input_v_buffer[i].b_handle);
 *         xma_plg_buffer_free(hw_handle, ctx->encoder.output_buffer[i].b_handle);
 *     }
 *     return 0;
 * }
 *
 * XmaEncoderPlugin encoder_plugin = {
 *     .hwencoder_type = XMA_H264_ENCODER_TYPE,
 *     .hwvendor_string = "Xilinx",
 *     .format = XMA_YUV420_FMT_TYPE,
 *     .bits_per_pixel = 8,
 *     .plugin_data_size = sizeof(EncoderContext),
 *     .kernel_data_size = sizeof(HostKernelCtx),
 *     .init = xlnx_encoder_init,
 *     .send_frame = xlnx_encoder_send_frame,
 *     .recv_data = xlnx_encoder_recv_data,
 *     .close = xlnx_encoder_close,
 *     .alloc_chan = xlnx_encoder_alloc_chan,
 *     .get_dev_input_paddr = NULL
 * };
 * @endcode
 *
 *
 * Note that each plugin implementation must statically allocate a data structure
 * with a specific name (as present on line 425 in the above example):
 *
 *
 * <table border="1">
 * <tr style="color:#fff; background: blue;"><th>Plugin Type</th><th>Required Global Variable Name</th></tr>
 * <tr><td>XmaDecoderPlugin</td><td>decoder_plugin</td></tr>
 * <tr><td>XmaEncoderPlugin</td><td>encoder_plugin</td></tr>
 * <tr><td>XmaFilterPlugin</td><td>filter_plugin</td></tr>
 * <tr><td>XmaScalerPlugin</td><td>scaler_plugin</td></tr>
 * <tr><td>XmaKernelPlugin</td><td>Kernel_plugin</td></tr>
 * </table>
 *
 * @section Initalization
 *
 * Initialization is the time for a plugin to perform one or more of the
 * following:
 * * evaluate an application request for a kernel channel (optional)
 * * allocate device buffers to handle input data as well as output data
 * * initalize the state of the kernel
 *
 * When an application creates a session (e.g. xma_enc_session_create()), the
 * plugin code will have the following callbacks invoked:
 * 1. alloc_chan (optional)
 * 2. init
 *
 * What is returned to the application code is a session object corresponding
 * to the type of session requested (e.g. XmaEncoderSession).  All
 * session objects derive from a base class: XmaSession.  These session
 * data structures contain all of the instance data pertaining to a kernel
 * and are used by the XMA library as well as plugin for storage and retrieval
 * of state information.
 *
 * From the perspective of the application, an session object represents
 * control of a kernel instance.  This may, in fact, be an entire video kernel
 * or, in the case of a kernel that supports channels, a 'virtual'
 * kernel that is shared amongst more than one thread of execution.
 * If your kernel supports channels (i.e. a type of 'virtual' kernel),
 * then the alloc_chan() callback must be implemented.  The signature
 * for alloc_chan includes an array of existing XmaSession objects that
 * have been previously allocated to this kernel as well as the
 * currently pending request.  It is your responsibility, as the plugin
 * developer, to decide if the pending request can be approved or rejected.
 * Approval should include updating the XmaSession::chan_id member with
 * a non-negative channel id and an XMA_SUCCESS return code (see
 * @ref xmaerr).
 *
 * Your init function will then be called after alloc_chan (assuming it was
 * implemented).  Within your init() implementation, you will be expected
 * to intialize any private session-specific data structures,
 * kernel-specific data structures, allocate device memory for holding
 * incoming data as well as for holding outgoing data and program
 * the registers of the kernel to place it into an initial state ready
 * for processing data.
 *
 * When your plugin is first loaded, XMA will allocate memory
 * for kernel-wide data (@ref XmaSession::kernel_data) based on the size you
 * specify in your plugin. This data is considered global for all
 * sessions sharing a given kernel (if the kernel supports this
 * via channels) and should be protected from simultaneous access.
 *
 * When a session has been created in response to an application request,
 * XMA will allocate plugin data (@ref XmaSession::plugin_data) that
 * is session-specific.
 *
 * These XmaSession::kernel_data and XmaSession::plugin_data members are
 * available to you to store the necessary kernel-wide and session-specific
 * state as necessary. There is no need to free these data structures during
 * termination; XMA frees this data for you.
 *
 * The XMA Plugin Library provides a set of functions to allocating
 * device memory and performing register reads and writes.
 * To allocate buffers necessary to handle both incoming and outgoing
 * data, please see xma_plg_buffer_alloc().
 *
 * See @ref xmaplugin.h for more details.
 *
 * @section xma_plg_guide_input Handling Incoming Application Data
 *
 * For each kernel type, there is an application interface to send data to be
 * proceessed (i.e. encoded, decoded, or otherwised transformed).
 * Data being sent by an application to the kernel will result in the invocation
 * of your send()/write() callback.
 *
 * The most common operation within the plugin is to copy data from host
 * memory to device memory so that it may be operated on by the kernel.
 * Subsequently, the kernel must be programmed to know which device buffer
 * contains the data to be processed and programmed appropriately.
 *
 * The XMA Plugin library call xma_plg_buffer_write() can be used to copy
 * host data to device data.
 *
 * xma_plg_register_write() and xma_plg_register_read() can be used to program
 * the kernel registers and start kernel processing.
 *
 * @section xma_plg_guide_output Sending Output to the Application
 *
 * For each kernel type, there is an application interface to request processed
 * data (i.e. encoded, decoded, otherwise transformed) by the kernel.  Data
 * being requested by an application from the kernel will invoke your
 * recv()/read() callback implementation.
 *
 * The most common operation within the plugin is to copy data from device
 * memory back to host memory so that it may be processed by the application.
 * Subsequently, the kernel may be prepared for new data to arrive for processing.
 *
 * The XMA Plugin library call xma_plg_buffer_read() can be used to copy
 * host data to device data.
 *
 * xma_plg_register_write() and xma_plg_register_read() can be used to program
 * the kernel registers and start kernel processing.
 *
 * @section xma_plg_guide_term Termination
 *
 * When an XMA application has concluded data processing, it will destroy its
 * kernel session.  Your close() callback will be invoked to perform the necessary
 * cleanup.  Your close() implementation should free any buffers that were
 * allocated in device memory during your init() via xma_plg_buffer_free().
 * Freeing XmaSession::kernel_data and XmaSession::plugin_data is not necessary
 * as this will be done by the XMA library.
 *
 * @section xma_plg_guide_zerocopy Zerocopy Special Case
 *
 * Encoders are capable of receiving data directly from upstream video processing
 * kernels such as filters or scalers.  In such a case, it may improve the
 * the performance of a video processing pipeline that includes both a filter and
 * an encoder to exchange data directly within device memory rather than have
 * the filter copy data back to a host buffer only to be re-copied from the host
 * to the device buffer of the downstream encoder.  This double-copy can be
 * avoided if the two kernels can share a buffer within the device memory; a
 * buffer that serves as an 'output' buffer for the filter but an 'input'
 * buffer for the encoder. This optimization is known as 'zerocopy'. The
 * encoder must implement the XmaEncoderPlugin::get_dev_input_paddr() callback.
 * The XMA library can detect whether the two kernel sessions are capable of
 * sharing buffers.  The following conditions will be checked:
 * 1. Both kernel sessions are connected to the same device DDR bank
 * 2. The get_dev_input_paddr() callback is implemented by the encoder session
 * 3. The encoder has been configured to expect frame data that is same format and
 *    size as the upstream filter kernel is producing as output.
 * 4. The system configuration file has specified that zerocopy is 'enabled'
 *
 * If all of the above conditions are true, zero-copy between the kernels will
 * be supported.  The XMA library will obtain the destination buffer address
 * for the filter from the encoder session.  This will then be provided as the
 * destination address to the filter's XmaFrame argument as part of its
 * recv_frame() callback.
 *
*/
#ifdef __cplusplus
}
#endif

#endif
