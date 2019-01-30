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
#include "lib/xmahw_hal.h"
#include "lib/xmares.h"
#include "xmaplugin.h"

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

int32_t
xma_enc_plugins_load(XmaSystemCfg      *systemcfg,
                     XmaEncoderPlugin  *encoders)
{
    // Get the plugin path
    char *pluginpath = systemcfg->pluginpath;
    char *error;
    int32_t k = 0;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);
    // Load the xmaplugin library as it is a dependency for all plugins
    void *xmahandle = dlopen("libxmaplugin.so",
                             RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Failed to open plugin xmaplugin.so. Error msg: %s\n",
                   dlerror());
        return XMA_ERROR;
    }

    // For each plugin imagecfg/kernelcfg,
    for (int32_t i = 0; i < systemcfg->num_images; i++)
    {
        for (int32_t j = 0;
             j < systemcfg->imagecfg[i].num_kernelcfg_entries; j++)
        {
            char *func = systemcfg->imagecfg[i].kernelcfg[j].function;
            if (strcmp(func, XMA_CFG_FUNC_NM_ENC) != 0)
                continue;
            char *plugin = systemcfg->imagecfg[i].kernelcfg[j].plugin;
            char pluginfullname[PATH_MAX + NAME_MAX];
            sprintf(pluginfullname, "%s/%s", pluginpath, plugin);
            void *handle = dlopen(pluginfullname, RTLD_NOW);
            if (!handle)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }

            XmaEncoderPlugin *plg =
                (XmaEncoderPlugin*)dlsym(handle, "encoder_plugin");
            if ((error = dlerror()) != NULL)
            {
                xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                    "Failed to open plugin %s\n Error msg: %s\n",
                    pluginfullname, dlerror());
                return XMA_ERROR;
            }
            memcpy(&encoders[k++], plg, sizeof(XmaEncoderPlugin));
        }
    }
    return XMA_SUCCESS;
}

XmaEncoderSession*
xma_enc_session_create(XmaEncoderProperties *enc_props)
{
    XmaEncoderSession *enc_session = malloc(sizeof(XmaEncoderSession));
    XmaResources xma_shm_cfg = g_xma_singleton->shm_res_cfg;
    XmaKernelRes kern_res;
    int rc, dev_handle, kern_handle, enc_handle;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);
	if (!xma_shm_cfg) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "No reference to xma res database\n");
        free(enc_session);
		return NULL;
    }

    memset(enc_session, 0, sizeof(XmaEncoderSession));
    // init session data
    enc_session->encoder_props = *enc_props;
    enc_session->base.chan_id = -1;
    enc_session->base.session_type = XMA_ENCODER;

    // Just assume this is a VP9 encoder for now and that the FPGA
    // has been downloaded.  This is accomplished by getting the
    // first device (dev_handle, base_addr, ddr_bank) and making a
    // XmaHwSession out of it.  Later this needs to be done by searching
    // for an available resource.
    /* JPM TODO default to exclusive device access.  Ensure multiple threads
       can access this device if in-use pid = requesting thread pid */
    rc = xma_res_alloc_enc_kernel(xma_shm_cfg, enc_props->hwencoder_type,
                                  enc_props->hwvendor_string,
                                  &enc_session->base, false);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Failed to allocate free encoder kernel. Return code %d\n", rc);
        return NULL;
    }

    kern_res = enc_session->base.kern_res;

    dev_handle = xma_res_dev_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD,"dev_handle = %d\n", dev_handle);
    if (dev_handle < 0)
        return NULL;

    kern_handle = xma_res_kern_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD,"kern_handle = %d\n", kern_handle);
    if (kern_handle < 0)
        return NULL;

    enc_handle = xma_res_plugin_handle_get(kern_res);
    xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD,"enc_handle = %d\n", enc_handle);
    if (enc_handle < 0)
        return NULL;

    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_handle].handle;

    enc_session->base.hw_session.dev_handle = hal->dev_handle;
    enc_session->base.hw_session.base_address =
        hwcfg->devices[dev_handle].kernels[kern_handle].base_address;
    enc_session->base.hw_session.ddr_bank =
        hwcfg->devices[dev_handle].kernels[kern_handle].ddr_bank;

    //For execbo:
    enc_session->base.hw_session.kernel_info = hwcfg->devices[dev_handle].kernels[kern_handle];
    enc_session->base.hw_session.dev_index = hal->dev_index;

    enc_session->encoder_plugin = &g_xma_singleton->encodercfg[enc_handle];

    // Allocate the private data
    enc_session->base.plugin_data =
        calloc(g_xma_singleton->encodercfg[enc_handle].plugin_data_size, sizeof(uint8_t));

    // For the encoder, only a receiver connection make sense
    // because no HW component consumes an encoded frame at
    // this point in a pipeline
    XmaEndpoint *end_pt = malloc(sizeof(XmaEndpoint));
    end_pt->session = &enc_session->base;
    end_pt->dev_id = dev_handle;
    end_pt->format = enc_props->format;
    end_pt->bits_per_pixel = enc_props->bits_per_pixel;
    end_pt->width = enc_props->width;
    end_pt->height = enc_props->height;
    enc_session->conn_recv_handle =
        xma_connect_alloc(end_pt, XMA_CONNECT_RECEIVER);

    // Call the plugins initialization function with this session data
    rc = enc_session->encoder_plugin->init(enc_session);
    if (rc) {
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Initalization of encoder plugin failed. Return code %d\n",
                   rc);
        return NULL;
    }

    // Create encoder file if it does not exist and initialize all fields 
    xma_enc_session_statsfile_init(enc_session);

    return enc_session;
}

int32_t
xma_enc_session_destroy(XmaEncoderSession *session)
{
    int32_t rc;

    xma_logmsg(XMA_DEBUG_LOG, XMA_ENCODER_MOD, "%s()\n", __func__);

    // Clean up the stats file, but don't delete it 
    xma_enc_session_statsfile_close(session);

    rc  = session->encoder_plugin->close(session);
    if (rc != 0)
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Error closing encoder plugin. Return code %d\n", rc);

    // Clean up the private data
    free(session->base.plugin_data);

    // Free the receiver connection
    xma_connect_free(session->conn_recv_handle,
                        XMA_CONNECT_RECEIVER);

    /* free kernel/kernel-session */
    rc = xma_res_free_kernel(g_xma_singleton->shm_res_cfg,
                             session->base.kern_res);
    if (rc)
        xma_logmsg(XMA_ERROR_LOG, XMA_ENCODER_MOD,
                   "Error freeing kernel session. Return code %d\n", rc);

    // Free the session
    // TODO: (should also free the Hw sessions)
    free(session);

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
    char            *path = "/var/tmp/xilinx";
    char            *enc_type_str;
    char            *vendor;
    int32_t          dev_id = 0;
    int32_t          kern_inst = 0;
    int32_t          chan_id = 0;
    char             fname[100];
    XmaEncoderStats *stats;

    stats = malloc(sizeof(XmaEncoderStats));

    // Convert encoder type to string
    switch(session->encoder_props.hwencoder_type)
    {
        case XMA_H264_ENCODER_TYPE:
            enc_type_str = "H264";
        break;
        case XMA_HEVC_ENCODER_TYPE:
            enc_type_str = "HEVC";
        break;
        case XMA_VP9_ENCODER_TYPE:
            enc_type_str = "VP9";
        break;
        case XMA_AV1_ENCODER_TYPE:
            enc_type_str = "AV1";
        break;
        case XMA_COPY_ENCODER_TYPE:
            enc_type_str = "COPY";
        break;
        default:
            enc_type_str = "UNKNOWN";
        break;
    }

    // Build up file name based on session parameters
    vendor = session->encoder_props.hwvendor_string;
    dev_id = xma_res_dev_handle_get(session->base.kern_res);
    kern_inst = xma_res_kern_handle_get(session->base.kern_res);
    chan_id = session->base.chan_id;
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

    // Always re-write the entire file
    lseek(stats->fd, 0, SEEK_SET);
    rc = write(stats->fd, stat_buf, strlen(stat_buf)); 
    if (rc < 0)
        xma_logmsg(XMA_INFO_LOG, XMA_ENCODER_MOD, 
                   "Write to statsfile failed\n");
}

void 
xma_enc_session_statsfile_close(XmaEncoderSession *session)
{
    XmaEncoderStats *stats;

    stats = (XmaEncoderStats*)session->base.stats;
     
    // Close and free the stats memory
    close(stats->fd);
    free(stats);
}
