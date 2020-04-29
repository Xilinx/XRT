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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "lib/xmaapi.h"
#include "app/xma_utils.hpp"
#include "lib/xma_utils.hpp"
#include "xmaplugin.h"
#include <bitset>

char    g_stat_fmt[] = "last_pid_in_use          :%d\n"
                       "last_received_input_ts   :%lu\n"
                       "last_received_output_ts  :%lu\n"
                       "received_frame_count     :%lu\n"
                       "received_pixel_count     :%lu\n"
                       "received_bit_count       :%lu\n"
                       "encoded_frame_count      :%lu\n"
                       "encoded_bit_count        :%lu\n";

typedef struct XmaEncoderStats
{
    int32_t     fd;
    uint64_t    last_pid_in_use;
    uint64_t    last_received_input_ts;
    uint64_t    last_received_output_ts;
    uint64_t    received_frame_count;
    uint64_t    received_pixel_count;
    uint64_t    received_bit_count;
    uint64_t    encoded_frame_count;
    uint64_t    encoded_bit_count;
} XmaEncoderStats;

// Private functions for managing statistics
void xma_enc_session_statsfile_init(XmaEncoderSession *session);

void xma_enc_session_statsfile_send_frame(XmaEncoderSession *session, 
                                          uint64_t           timestamp,
                                          uint32_t           frame_size);

void xma_enc_session_statsfile_recv_data(XmaEncoderSession *session, 
                                         uint64_t           timestamp,
                                         uint32_t           data_size);

void xma_enc_session_statsfile_write(XmaEncoderStats *stats);

void xma_enc_session_statsfile_close(XmaEncoderSession *session);

#define XMA_ENCODER_MOD "xmaencoder"

extern XmaSingleton *g_xma_singleton;

XmaEncoderSession*
xma_enc_session_create(XmaEncoderProperties *enc_props)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);
    if (!g_xma_singleton->xma_initialized) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "XMA session creation must be after initialization\n");
        return nullptr;
    }
    if (enc_props->plugin_lib == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "EncoderProperties must set plugin_lib\n");
        return nullptr;
    }

    XmaEncoderPlugin *plg = nullptr;
    void *handle = dlopen(enc_props->plugin_lib, RTLD_NOW);
    if (!handle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
            "Failed to open plugin %s\n Error msg: %s\n",
            enc_props->plugin_lib, dlerror());
        return nullptr;
    }

    plg = (XmaEncoderPlugin*)dlsym(handle, "encoder_plugin");
    char *error;
    if ((error = dlerror()) != NULL)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
            "Failed to get struct encoder_plugin from %s\n Error msg: %s\n",
            enc_props->plugin_lib, dlerror());
        return nullptr;
    }

    if (plg->xma_version == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "EncoderPlugin library must have xma_version function\n");
        return nullptr;
    }

    XmaEncoderSession *enc_session = (XmaEncoderSession*) malloc(sizeof(XmaEncoderSession));
    if (enc_session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
            "Failed to allocate memory for encoderSession\n");
        return nullptr;
    }
    memset(enc_session, 0, sizeof(XmaEncoderSession));
    // init session data
    enc_session->encoder_props = *enc_props;
    enc_session->base.channel_id = enc_props->channel_id;
    enc_session->base.session_type = XMA_ENCODER;
    enc_session->base.stats = NULL;
    enc_session->private_session_data = NULL;//Managed by host video application
    enc_session->private_session_data_size = -1;//Managed by host video application

    enc_session->encoder_plugin = plg;

    int32_t rc, dev_index, cu_index;
    dev_index = enc_props->dev_index;
    cu_index = enc_props->cu_index;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "XMA session creation failed. dev_index not found\n");
        free(enc_session);
        return nullptr;
    }

    uint32_t hwcfg_dev_index = 0;
    bool found = false;
    for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
        if (hw_device.dev_index == (uint32_t)dev_index) {
            found = true;
            break;
        }
        hwcfg_dev_index++;
    }
    if (!found) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "XMA session creation failed. dev_index not loaded with xclbin\n");
        free(enc_session);
        return nullptr;
    }
    if ((cu_index > 0 && (uint32_t)cu_index >= hwcfg->devices[hwcfg_dev_index].number_of_cus) || (cu_index < 0 && enc_props->cu_name == NULL)) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "XMA session creation failed. Invalid cu_index = %d\n", cu_index);
        free(enc_session);
        return nullptr;
    }
    if (cu_index < 0) {
        std::string cu_name = std::string(enc_props->cu_name);
        found = false;
        for (XmaHwKernel& kernel: g_xma_singleton->hwcfg.devices[hwcfg_dev_index].kernels) {
            if (std::string((char*)kernel.name) == cu_name) {
                found = true;
                cu_index = kernel.cu_index;
                break;
            }
        }
        if (!found) {
            xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                    "XMA session creation failed. cu %s not found\n", cu_name.c_str());
            free(enc_session);
            return nullptr;
        }
    }

    void* dev_handle = hwcfg->devices[hwcfg_dev_index].handle;
    XmaHwKernel* kernel_info = &hwcfg->devices[hwcfg_dev_index].kernels[cu_index];
    enc_session->base.hw_session.dev_index = hwcfg->devices[hwcfg_dev_index].dev_index;

    //Allow user selected default ddr bank per XMA session
    if (xma_core::finalize_ddr_index(kernel_info, enc_props->ddr_bank_index, 
        enc_session->base.hw_session.bank_index, XMA_ENCODER_MOD) != XMA_SUCCESS) {
        free(enc_session);
        return nullptr;
    }

    if (kernel_info->kernel_channels) {
        if (enc_session->base.channel_id > (int32_t)kernel_info->max_channel_id) {
            xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                "Selected dataflow CU with channels has ini setting with max channel_id of %d. Cannot create session with higher channel_id of %d\n", kernel_info->max_channel_id, enc_session->base.channel_id);
            
            free(enc_session);
            return nullptr;
        }
    }

    // Call the plugins initialization function with this session data
    int32_t xma_main_ver = -1;
    int32_t xma_sub_ver = -1;
    rc = enc_session->encoder_plugin->xma_version(&xma_main_ver, &xma_sub_ver);
    int32_t tmp_check = xma_core::check_plugin_version(xma_main_ver, xma_sub_ver);

    if (rc < 0 || tmp_check == -1) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Initalization of plugin failed. Plugin is incompatible with this XMA version\n");
        free(enc_session);
        return nullptr;
    }
    if (tmp_check <= -2) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Initalization of plugin failed. Newer plugin is not allowed with old XMA library\n");
        free(enc_session);
        return nullptr;
    }

    XmaHwDevice& dev_tmp1 = hwcfg->devices[hwcfg_dev_index];
    // Allocate the private data
    enc_session->base.plugin_data =
        calloc(enc_session->encoder_plugin->plugin_data_size, sizeof(uint8_t));

    XmaHwSessionPrivate *priv1 = new XmaHwSessionPrivate();
    priv1->dev_handle = dev_handle;
    priv1->kernel_info = kernel_info;
    priv1->kernel_complete_count = 0;
    priv1->device = &hwcfg->devices[hwcfg_dev_index];
    enc_session->base.hw_session.private_do_not_use = (void*) priv1;
    enc_session->base.session_signature = (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved));

    int32_t num_execbo = g_xma_singleton->num_execbos;
    priv1->kernel_execbos.reserve(num_execbo);
    priv1->num_execbo_allocated = num_execbo;
    if (xma_core::create_session_execbo(priv1, num_execbo, XMA_ENCODER_MOD) != XMA_SUCCESS) {
        free(enc_session->base.plugin_data);
        free(enc_session);
        delete priv1;
        return nullptr;
    }

    // Create encoder file if it does not exist and initialize all fields 
    xma_enc_session_statsfile_init(enc_session);

    //Obtain lock only for a) singleton changes & b) kernel_info changes
    std::unique_lock<std::mutex> guard1(g_xma_singleton->m_mutex);
    //Singleton lock acquired

    if (!kernel_info->soft_kernel && !kernel_info->in_use && !kernel_info->context_opened) {
        if (xclOpenContext(dev_handle, dev_tmp1.uuid, kernel_info->cu_index_ert, true) != 0) {
            xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD, "Failed to open context to CU %s for this session\n", kernel_info->name);
            free(enc_session->base.plugin_data);
            free(enc_session);
            delete priv1;
            return nullptr;
        }
    }

    enc_session->base.session_id = g_xma_singleton->num_of_sessions + 1;
    xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD,
                "XMA session channel_id: %d; session_id: %d\n", enc_session->base.channel_id, enc_session->base.session_id);

    if (kernel_info->in_use) {
        kernel_info->is_shared = true;
        xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD,
                   "XMA session sharing CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    } else {
        kernel_info->in_use = true;
        xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD,
                   "XMA session with CU: %s\n", hwcfg->devices[hwcfg_dev_index].kernels[cu_index].name);
    }
    kernel_info->num_sessions++;
    g_xma_singleton->num_encoders++;
    g_xma_singleton->num_of_sessions = enc_session->base.session_id;

    g_xma_singleton->all_sessions_vec.push_back(enc_session->base);

    //Release singleton lock
    guard1.unlock();

    //init can execute cu cmds as well so must be fater adding to singleton above
    rc = enc_session->encoder_plugin->init(enc_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Initalization of encoder plugin failed. Return code %d\n",
                   rc);
        free(enc_session->base.plugin_data);
        //free(enc_session); Added to singleton above; Keep it as checked for cu cmds
        //delete priv1;
        return nullptr;
    }

    return enc_session;
}

int32_t
xma_enc_session_destroy(XmaEncoderSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);

    std::lock_guard<std::mutex> guard1(g_xma_singleton->m_mutex);
    //Singleton lock acquired

    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Session is already released\n");

        return XMA_ERROR;
    }
    if (session->base.hw_session.private_do_not_use == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Session is corrupted\n");

        return XMA_ERROR;
    }
    if (session->encoder_plugin == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Session is corrupted\n");

        return XMA_ERROR;
    }
    // Clean up the stats file, but don't delete it 
    xma_enc_session_statsfile_close(session);

    rc  = session->encoder_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Error closing encoder plugin. Return code %d\n", rc);

    // Clean up the private data
    free(session->base.plugin_data);

    // Free the session
    //Let's not chnage in_use and num of encoders
    //It is better to have different session_id for debugging
    /*
    delete (XmaHwSessionPrivate*)session->base.hw_session.private_do_not_use;
    */
    session->base.hw_session.private_do_not_use = nullptr;
    session->base.plugin_data = nullptr;
    session->base.stats = NULL;
    session->encoder_plugin = NULL;
    //do not change kernel in_use as it maybe in use by another plugin
    session->base.hw_session.dev_index = -1;
    session->base.session_signature = NULL;
    free(session);
    session = nullptr;

    return XMA_SUCCESS;
}

int32_t
xma_enc_session_send_frame(XmaEncoderSession *session,
                           XmaFrame          *frame)
{
    int32_t  rc;
    struct   timespec ts;
    uint64_t timestamp;
    uint32_t frame_size;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);

    if (frame == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "xma_enc_session_send_frame failed. Frame is NULL\n");
        return XMA_ERROR;
    }
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "xma_enc_session_send_frame failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD, "xma_enc_session_send_frame failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
  
    clock_gettime(CLOCK_MONOTONIC, &ts);  
    timestamp = (ts.tv_sec * 1000000000) + ts.tv_nsec;
    rc = session->encoder_plugin->send_frame(session, frame);
    if (frame->do_not_encode == false)
    {
        frame_size = frame->frame_props.width * frame->frame_props.height; 
        xma_enc_session_statsfile_send_frame(session, 
                                             timestamp,
                                             frame_size);
    }
    return rc;

}

int32_t
xma_enc_session_recv_data(XmaEncoderSession *session,
                          XmaDataBuffer     *data,
                          int32_t           *data_size)
{
    int32_t  rc;
    struct   timespec ts;
    uint64_t timestamp;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);
    if (session == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "xma_enc_session_recv_data failed. Session is already released\n");
        return XMA_ERROR;
    }
    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) session->base.hw_session.private_do_not_use;
    if (priv1 == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD, "xma_enc_session_recv_data failed. XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    if (session->base.session_signature != (void*)(((uint64_t)priv1) | ((uint64_t)priv1->reserved))) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD, "XMASession is corrupted.\n");
        return XMA_ERROR;
    }
    rc = session->encoder_plugin->recv_data(session, data, data_size);
    if (*data_size)
    {
        clock_gettime(CLOCK_MONOTONIC, &ts);  
        timestamp = (ts.tv_sec * 1000000000) + ts.tv_nsec;
        xma_enc_session_statsfile_recv_data(session, 
                                            timestamp,
                                            *data_size);
    }

    return rc;
}

void 
xma_enc_session_statsfile_init(XmaEncoderSession *session)
{
    char            *path = (char*) "/var/tmp/xilinx";
    char            *enc_type_str;
    char            *vendor;
    int32_t          dev_id = 0;
    int32_t          kern_inst = 0;
    int32_t          chan_id = 0;
    char             fname[512];
    XmaEncoderStats *stats;

    stats = (XmaEncoderStats*) malloc(sizeof(XmaEncoderStats));
	if (stats == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Unable to initialize encoder stats file\n");
		return;
    }

    // Convert encoder type to string
    switch(session->encoder_props.hwencoder_type)
    {
        case XMA_H264_ENCODER_TYPE:
            enc_type_str = (char*) "H264";
        break;
        case XMA_HEVC_ENCODER_TYPE:
            enc_type_str = (char*) "HEVC";
        break;
        case XMA_VP9_ENCODER_TYPE:
            enc_type_str = (char*) "VP9";
        break;
        case XMA_AV1_ENCODER_TYPE:
            enc_type_str = (char*) "AV1";
        break;
        case XMA_COPY_ENCODER_TYPE:
            enc_type_str = (char*) "COPY";
        break;
        default:
            enc_type_str = (char*) "UNKNOWN";
        break;
    }

    // Build up file name based on session parameters
    vendor = session->encoder_props.hwvendor_string;
    /*Sarab: TODO. Fix dev id, cu id, etc
    dev_id = xma_res_dev_handle_get(session->base.kern_res);
    kern_inst = xma_res_kern_handle_get(session->base.kern_res);
    */
    chan_id = session->base.channel_id;
    sprintf(fname, "%s/ENC-%s-%s-%d-%d-%d",
            path, enc_type_str, vendor, dev_id, kern_inst, chan_id);     
    
    // Open the file - create if it doesn't exixt, truncate if it does
    umask(0);
    stats->fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0666);    

    // Initialize our in-memory copy of the stats
    stats->last_pid_in_use = getpid();
    stats->last_received_input_ts = 0ULL;
    stats->last_received_output_ts = 0ULL;
    stats->received_frame_count = 0ULL;
    stats->received_pixel_count = 0ULL;
    stats->received_bit_count = 0ULL;
    stats->encoded_frame_count = 0ULL;
    stats->encoded_bit_count = 0ULL;

    xma_enc_session_statsfile_write(stats);

    session->base.stats = stats;
}

void xma_enc_session_statsfile_send_frame(XmaEncoderSession *session, 
                                          uint64_t           timestamp,
                                          uint32_t           frame_size)
{
    XmaEncoderStats *stats;

    stats = (XmaEncoderStats*)session->base.stats;
     
    // Update send_frame stats
    stats->last_received_input_ts = timestamp;
    stats->received_frame_count++;
    stats->received_pixel_count += frame_size;
    stats->received_bit_count += (frame_size * 12);

    xma_enc_session_statsfile_write(stats);
}

void xma_enc_session_statsfile_recv_data(XmaEncoderSession *session, 
                                         uint64_t           timestamp,
                                         uint32_t           data_size)
{
    XmaEncoderStats *stats;

    stats = (XmaEncoderStats*)session->base.stats;
     
    // Update recv_data stats
    stats->last_received_output_ts = timestamp;
    stats->encoded_frame_count++;
    stats->encoded_bit_count += data_size * 8;

    xma_enc_session_statsfile_write(stats);
}

void xma_enc_session_statsfile_write(XmaEncoderStats *stats)
{
    int32_t          rc;
    char             stat_buf[1024];

    sprintf(stat_buf, g_stat_fmt, 
            stats->last_pid_in_use,
            stats->last_received_input_ts,
            stats->last_received_output_ts,
            stats->received_frame_count,
            stats->received_pixel_count,
            stats->received_bit_count,
            stats->encoded_frame_count,
            stats->encoded_bit_count);

    if (stats->fd <= 0) {
        xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD, 
                   "statsfile failed to open\n");
    } else {
        // Always re-write the entire file
        lseek(stats->fd, 0, SEEK_SET);
        rc = write(stats->fd, stat_buf, strlen(stat_buf)); 
        if (rc < 0)
            xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD, 
                    "Write to statsfile failed\n");
    }
}

void 
xma_enc_session_statsfile_close(XmaEncoderSession *session)
{
    XmaEncoderStats *stats;

    stats = (XmaEncoderStats*)session->base.stats;
     
    // Close and free the stats memory
    if (stats->fd > 0)
        close(stats->fd);
    free(stats);
}
