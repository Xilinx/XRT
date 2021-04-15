/*
 * Copyright 2020 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * TODO:
 * - Dynamic resolution change not supported
 * - do_not_encode support not added. need to check how gst-omx is doing
 * - pts is overriden by videoencoder base class, need to check how to send proper pts
 */

#include <fstream>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <time.h>

#include "plugin_enc.h"
#include "xrt_utils.h"

/*Defaults*/
#define GST_IVAS_VIDEO_ENC_ASPECT_RATIO_DEFAULT               ASPECT_RATIO_AUTO
#define GST_IVAS_VIDEO_ENC_CONTROL_RATE_DEFAULT                          RC_CBR
#define GST_IVAS_VIDEO_ENC_TARGET_BITRATE_DEFAULT                            64
#define GST_IVAS_VIDEO_ENC_QP_MODE_DEFAULT                              AUTO_QP
#define GST_IVAS_VIDEO_ENC_MIN_QP_DEFAULT                                     0
#define GST_IVAS_VIDEO_ENC_MAX_QP_DEFAULT                                    51
#define GST_IVAS_VIDEO_ENC_GOP_MODE_DEFAULT                         DEFAULT_GOP
#define GST_IVAS_VIDEO_ENC_GDR_MODE_DEFAULT                         GDR_DISABLE
#define GST_IVAS_VIDEO_ENC_INITIAL_DELAY_DEFAULT                           1000
#define GST_IVAS_VIDEO_ENC_CPB_SIZE_DEFAULT                                2000
#define GST_IVAS_VIDEO_ENC_SCALING_LIST_DEFAULT            SCALING_LIST_DEFAULT
#define GST_IVAS_VIDEO_ENC_MAX_BITRATE_DEFAULT                             5000
#define GST_IVAS_VIDEO_ENC_MAX_QUALITY_DEFAULT                               14
#define GST_IVAS_VIDEO_ENC_FILLER_DATA_DEFAULT                             TRUE
#define GST_IVAS_VIDEO_ENC_NUM_SLICES_DEFAULT                                 1
#define GST_IVAS_VIDEO_ENC_SLICE_QP_DEFAULT                                  -1
#define GST_IVAS_VIDEO_ENC_SLICE_SIZE_DEFAULT                                 0
#define GST_IVAS_VIDEO_ENC_PREFETCH_BUFFER_DEFAULT                         TRUE
#define GST_IVAS_VIDEO_ENC_LONGTERM_REF_DEFAULT                           FALSE
#define GST_IVAS_VIDEO_ENC_LONGTERM_FREQUENCY_DEFAULT                         0
#define GST_IVAS_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT          X_MAXUINT
#define GST_IVAS_VIDEO_ENC_B_FRAMES_DEFAULT                                   0
#define GST_IVAS_VIDEO_ENC_GOP_LENGTH_DEFAULT                                30
#define GST_IVAS_VIDEO_ENC_ENTROPY_MODE_DEFAULT                      MODE_CABAC
#define GST_IVAS_VIDEO_ENC_CONSTRAINED_INTRA_PREDICTION_DEFAULT           FALSE
#define GST_IVAS_VIDEO_ENC_LOOP_FILTER_MODE_DEFAULT          LOOP_FILTER_ENABLE
#define GST_IVAS_VIDEO_ENC_LOW_BANDWIDTH_DEFAULT                          FALSE
#define GST_IVAS_VCU_ENC_SK_DEFAULT_NAME                   "kernel_vcu_encoder"
#define GST_IVAS_VIDEO_ENC_RC_MODE_DEFAULT                                FALSE
#define GST_IVAS_VIDEO_ENC_KERNEL_NAME_DEFAULT              "encoder:encoder_1"

static void gst_ivas_xvcuenc_map_params (XrtIvas_XVCUEnc * enc); 

static int
ivas_xvcuenc_check_softkernel_response (XrtIvas_XVCUEnc * enc,
    sk_payload_data * payload_buf)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  int iret;

  memset (payload_buf, 0, priv->sk_payload_buf->size);
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    return FALSE;
  }

  /* check response from softkernel */
  if (!payload_buf->cmd_rsp)
    return FALSE;

  return TRUE;
}

static int
ivas_xvcuenc_allocate_internal_buffers (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  int iret = 0;

  priv->ert_cmd_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->ert_cmd_buf == NULL) {
    ERROR_PRINT ("failed to allocate ert cmd memory");
    goto error;
  }

  priv->sk_payload_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->sk_payload_buf == NULL) {
    ERROR_PRINT ("failed to allocate sk payload memory");
    goto error;
  }

  priv->dyn_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dyn_cfg_buf == NULL) {
    ERROR_PRINT("failed to allocate encoder dyncamic config memory handle");
    goto error;
  }

  /* allocate ert command buffer */
  iret =
      alloc_xrt_buffer (priv->xcl_handle, ERT_CMD_SIZE, XCL_BO_SHARED_VIRTUAL,
      1 << 31, priv->ert_cmd_buf);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate ert command buffer..");
    goto error;
  }

  /* allocate softkernel payload buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (sk_payload_data),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->sk_payload_buf);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate softkernel payload buffer..");
    goto error;
  }

  /* allocate encoder config buffer */
  iret =
      alloc_xrt_buffer (priv->xcl_handle, sizeof (enc_dynamic_params_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->dyn_cfg_buf);
  if (iret < 0) {
    ERROR_PRINT("failed to allocate encoder dynamic config buffer..");
    goto error;
  }

  return TRUE;

error:
  return FALSE;
}

static void
ivas_xvcuenc_free_internal_buffers (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;

  if (priv->dyn_cfg_buf) {
    free_xrt_buffer (priv->xcl_handle, priv->dyn_cfg_buf);
    free (priv->dyn_cfg_buf);
    priv->dyn_cfg_buf = NULL;
  }

  if (priv->static_cfg_buf) {
    free_xrt_buffer (priv->xcl_handle, priv->static_cfg_buf);
    free (priv->static_cfg_buf);
    priv->static_cfg_buf = NULL;
  }

  if (priv->sk_payload_buf) {
    free_xrt_buffer (priv->xcl_handle, priv->sk_payload_buf);
    free (priv->sk_payload_buf);
    priv->sk_payload_buf = NULL;
  }

  if (priv->ert_cmd_buf) {
    free_xrt_buffer (priv->xcl_handle, priv->ert_cmd_buf);
    free (priv->ert_cmd_buf);
    priv->ert_cmd_buf = NULL;
  }
}

static int
ivas_xvcuenc_allocate_output_buffers (XrtIvas_XVCUEnc * enc, int num_out_bufs,
    int out_buf_size)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  uint64_t *out_bufs_addr;
  int iret = 0, i;

  priv->num_out_bufs = num_out_bufs;

  INFO_PRINT ("output buffer allocation: nbuffers = %u and output buffer size = %u",
      priv->num_out_bufs, out_buf_size);

  priv->out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->out_bufs_handle == NULL) {
    ERROR_PRINT ("failed to allocate encoder output buffers structure");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle, priv->num_out_bufs * sizeof (uint64_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->out_bufs_handle);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate encoder out buffers handle..");
    goto error;
  }

  out_bufs_addr = (uint64_t *) (priv->out_bufs_handle->user_ptr);

  if (priv->out_xrt_bufs)
    free (priv->out_xrt_bufs);

  priv->out_xrt_bufs =
      (xrt_buffer **) calloc (priv->num_out_bufs, sizeof (xrt_buffer *));
  if (!priv->out_xrt_bufs) {
    ERROR_PRINT ("failed to allocate memory");
    goto error;
  }

  for (i = 0; i < priv->num_out_bufs; i++) {
    xrt_buffer *out_xrt_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
    if (out_xrt_buf == NULL) {
      ERROR_PRINT ("failed to allocate encoder output buffer");
      goto error;
    }

    iret =
        alloc_xrt_buffer (priv->xcl_handle, out_buf_size, XCL_BO_DEVICE_RAM,
        MEM_BANK, out_xrt_buf);
    if (iret < 0) {
      ERROR_PRINT ("failed to allocate encoder output buffer..");
      goto error;
    }
    /* store each out physical address in priv strucuture */
    out_bufs_addr[i] = out_xrt_buf->phy_addr;
    priv->out_xrt_bufs[i] = out_xrt_buf;
  }

  iret = xclSyncBO (priv->xcl_handle, priv->out_bufs_handle->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->out_bufs_handle->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  return TRUE;
error:
  return FALSE;
}


static void
ivas_xvcuenc_free_output_buffers (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  xrt_buffer *out_xrt_buf = NULL;
  int num_out_bufs;
  int i;

  num_out_bufs = priv->num_out_bufs;
  for (i = (num_out_bufs - 1); i >= 0; i--) {
    out_xrt_buf = priv->out_xrt_bufs[i];
    free_xrt_buffer (priv->xcl_handle, out_xrt_buf);
    free (out_xrt_buf);
  }

  if (priv->out_bufs_handle) {
    free_xrt_buffer (priv->xcl_handle, priv->out_bufs_handle);
    free (priv->out_bufs_handle);
  }
}

static int
gst_ivas_xvcuenc_open (XrtIvas_XVCUEnc *enc, const char *xclbin_path, int sk_idx, int dev_idx)
{
  XrtIvas_XVCUEncPrivate *priv = NULL;
  int bret = FALSE;
  int cu_index = 0;
  
  DEBUG_PRINT ("opening");

  if (!enc)
    return bret;

  enc->xclbin_path = (char *)xclbin_path;
  enc->sk_start_idx = enc->sk_cur_idx = sk_idx;
  enc->dev_index = dev_idx;           /* default dev index is 0 */

  priv = (XrtIvas_XVCUEncPrivate *) calloc (1, sizeof (XrtIvas_XVCUEncPrivate));
  if (!priv) {
    ERROR_PRINT ("failed to allocate private memory structure");
    return bret;
  }

  enc->priv = priv;

  if (bret = download_xclbin (enc->xclbin_path, enc->dev_index, &cu_index, &(priv->xcl_handle),
          &(priv->xclbinId))) {
    if (bret < 0) 
      ERROR_PRINT ("failed to download xclbin %s]", enc->xclbin_path);

    return NOTSUPP;
  }
 
  priv->cu_idx = cu_index;
 
  if (xclOpenContext (priv->xcl_handle, priv->xclbinId, priv->cu_idx, true)) {
    ERROR_PRINT("failed to open context CU index %d.", priv->cu_idx);
    return FALSE;
  }

  INFO_PRINT("Initialization of XRT is successfully. xrt handle = %p",
      priv->xcl_handle);

  bret = ivas_xvcuenc_allocate_internal_buffers (enc);
  if (bret == FALSE) {
    ERROR_PRINT("failed to allocate internal buffers");
    return FALSE;
  }

  return TRUE;
}

static int
gst_ivas_xvcuenc_close (XrtIvas_XVCUEnc *enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  int iret;

  /* free all output buffers allocated */
  ivas_xvcuenc_free_output_buffers (enc);

  /* free all internal buffers */
  ivas_xvcuenc_free_internal_buffers (enc);

  DEBUG_PRINT("closing");

  iret = xclCloseContext (priv->xcl_handle, priv->xclbinId, priv->cu_idx);
  if (iret)
    ERROR_PRINT ("failed to close context of CU index %d. "
        "reason : %s", priv->cu_idx, strerror (errno));

  xclClose (priv->xcl_handle);
  return TRUE;
}

static int
ivas_xvcuenc_preinit (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  //GstVideoInfo in_vinfo;
  int bret = FALSE;
  enc_dynamic_params_t *dyn_cfg_params;
  int iret = -1;
  struct timespec init_time;

  dyn_cfg_params = (enc_dynamic_params_t *) (priv->dyn_cfg_buf->user_ptr);
  memset (dyn_cfg_params, 0, priv->dyn_cfg_buf->size);

  dyn_cfg_params->width = 1920;
  dyn_cfg_params->height = 1080;
  dyn_cfg_params->framerate = 30;
  dyn_cfg_params->rc_mode = 0;

  iret = xclSyncBO (priv->xcl_handle, priv->dyn_cfg_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dyn_cfg_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  gst_ivas_xvcuenc_map_params(enc);

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PREINIT;

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, 1024 * sizeof (int));
  clock_gettime (CLOCK_MONOTONIC, &init_time);
  priv->timestamp = ((init_time.tv_sec * 1e6) + (init_time.tv_nsec/1e3));

  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = VCU_PREINIT;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);
  payload_data[num_idx++] = priv->static_cfg_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->static_cfg_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->static_cfg_buf->size;
  payload_data[num_idx++] = priv->dyn_cfg_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->dyn_cfg_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->dyn_cfg_buf->size;
  payload_data[num_idx++] = 0;  // TODO: Lambda not supported yet
  payload_data[num_idx++] = 0;  // TODO: Lambda not supported yet
  payload_data[num_idx++] = 0;  // TODO: Lambda not supported yet

  INFO_PRINT ("sending pre-init command to softkernel");

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_PREINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel pre-initialization failed");
      goto error;
    }
  }

  INFO_PRINT ("minimum output buffers required by encoder %u and output buffer size %u",
      payload_buf->obuf_count, payload_buf->obuf_size);

  if (!payload_buf->obuf_count || !payload_buf->obuf_size) {
    ERROR_PRINT ("invalid params received from softkernel : outbuf count %u, outbuf size %u",
        payload_buf->obuf_count, payload_buf->obuf_size);
    goto error;
  }

  enc->priv->min_num_inbufs = payload_buf->ibuf_count;
  enc->priv->in_buf_size = payload_buf->ibuf_size;

  /* allocate number of output buffers based on softkernel requirement */
  bret = ivas_xvcuenc_allocate_output_buffers (enc, payload_buf->obuf_count,
      payload_buf->obuf_size);
  if (bret != TRUE)
    goto error;

  INFO_PRINT ("Successfully pre-initialized softkernel");
  return TRUE;

error:
  return FALSE;
}

static int
ivas_xvcuenc_init (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0;
  int bret = FALSE;

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_INIT;
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, 1024 * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = VCU_INIT;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = priv->out_bufs_handle->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->out_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->out_bufs_handle->size;

  if (priv->qp_bufs_handle) {
    payload_data[num_idx++] = priv->qp_bufs_handle->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
        ((uint64_t) (priv->qp_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = priv->qp_bufs_handle->size;
  } else {
    payload_data[num_idx++] = 0;
    payload_data[num_idx++] = 0;
    payload_data[num_idx++] = 0;
  }

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_INIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel initialization failed");
      goto error;
    }
  }

  INFO_PRINT ("Successfully initialized softkernel");
  return TRUE;

error:
  return FALSE;
}

static int
ivas_xvcuenc_send_frame (XrtIvas_XVCUEnc *enc, uint8_t *indata, size_t insize)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0, i;
  bool bret = FALSE;
  uint32_t cur_in_idx = 0xBAD;
  xrt_buffer *xrt_in_buff = NULL;
  int xrt_in_size = insize;
  struct timespec tms;

  /* POSIX.1-2008 way */
  if (clock_gettime(CLOCK_REALTIME, &tms)) {
    return -1;
  }
  /* Read from input file for encoder input frame */
  if (!indata)
  {
    ERROR_PRINT ("Invalid input data");
    goto error;
  }

  xrt_in_buff = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (xrt_in_buff == NULL) {
    ERROR_PRINT ("failed to allocate sk payload memory");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle, xrt_in_size,
      XCL_BO_DEVICE_RAM, MEM_BANK, xrt_in_buff);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate softkernel payload buffer..");
    goto error;
  }

  /* copy input frame buffer to xrt memory */
  iret = xclWriteBO (priv->xcl_handle,
      xrt_in_buff->bo, indata, xrt_in_size, 0);
  if (iret != 0) {
    ERROR_PRINT ("failed to write input frame to xrt memory. reason : %s\n", strerror (errno));
    goto error;
  }

  /* transfer input frame contents to device */
  iret = xclSyncBO (priv->xcl_handle,
      xrt_in_buff->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, xrt_in_size, 0);
  if (iret != 0) {
    ERROR_PRINT ("failed to sync input frame. reason : %s\n",
        strerror (errno));
    goto error;
  }

  cur_in_idx = priv->num_in_idx++;

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PUSH;
  payload_buf->ibuf_index = cur_in_idx;

  payload_buf->ibuf_size = xrt_in_size;
  payload_buf->ibuf_paddr = xrt_in_buff->phy_addr;
  payload_buf->ibuf_meta.pts = tms.tv_sec * 1000000;
  payload_buf->obuf_indexes_to_release_valid_cnt = 0;


  /* transfer payload settings to device */
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, 1024 * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = VCU_PUSH;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  DEBUG_PRINT ("sending VCU_PUSH command to softkernel");

  /* send command to softkernel */
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_PUSH command to softkernel - %d, reason : %s", iret, 
        strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel send frame failed");
      goto error;
    }
  }

  if (payload_buf->freed_ibuf_index != 0xBAD) {
    DEBUG_PRINT ("successfully completed VCU_PUSH command : input buffer index freed %d\n",
      payload_buf->freed_ibuf_index);
  }

  return TRUE;

error:
  return FALSE;

}

static int
ivas_xvcuenc_read_output_frame (XrtIvas_XVCUEnc * enc, uint8_t * outbuf,
    int oidx, int outsize)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  xrt_buffer *out_xrt_buf = NULL;
  int iret = 0;

  out_xrt_buf = priv->out_xrt_bufs[oidx];

  if (outsize > out_xrt_buf->size) {
    ERROR_PRINT ("received out frame size %d greater than allocated xrt buffer size %d",
        outsize, out_xrt_buf->size);
    goto error;
  }

  iret = xclSyncBO (priv->xcl_handle, out_xrt_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, outsize, 0);
  if (iret != 0) {
    ERROR_PRINT ("xclSyncBO failed for output buffer. error = %d",
        iret);
    goto error;
  }

  //TODO: xclReadBO can be avoided if we allocate buffers using GStreamer pool
  iret =
      xclReadBO (priv->xcl_handle, out_xrt_buf->bo, outbuf, outsize, 0);
  if (iret != 0) {
    ERROR_PRINT ("failed to read output buffer. reason : %s",
        strerror (errno));
    goto error;
  }


  return TRUE;

error:
  return FALSE;
}

static XrtFlowReturn
ivas_xvcuenc_receive_out_frame (XrtIvas_XVCUEnc * enc, uint8_t *out_buffer, uint32_t *out_size)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0;
  int bret = FALSE;
  int oidx, outsize;
  XrtFlowReturn fret = XRT_FLOW_ERROR;

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_RECEIVE;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, 1024 * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = VCU_RECEIVE;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  DEBUG_PRINT ("sending VCU_RECEIVE command to softkernel");

  /* send command to softkernel */
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_RECEIVE command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel receive frame failed");
      goto error;
    }
  }
  DEBUG_PRINT ("successfully completed VCU_RECEIVE command");

  DEBUG_PRINT ("freed index count received from softkernel = %d",
      payload_buf->freed_index_cnt);
  if (payload_buf->freed_index_cnt == 0) {
    if (payload_buf->end_encoding) {
      INFO_PRINT ("received EOS from softkernel");
      return XRT_FLOW_EOS;
    }
    DEBUG_PRINT("no encoded buffers to consume");
    return XRT_FLOW_OK;
  }

  memcpy (&priv->last_rcvd_payload, payload_buf, sizeof (sk_payload_data));

  priv->last_rcvd_oidx = 0;

  oidx =
      priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].obuff_index;
  if (oidx == 0xBAD) {
    ERROR_PRINT ("received bad index from softkernel");
    goto error;
  }

  outsize =
      priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].recv_size;

  DEBUG_PRINT ("reading encoded output at index %d with size %d",
      oidx, outsize);

  bret = ivas_xvcuenc_read_output_frame (enc, out_buffer, oidx,
      outsize);
  if (bret != TRUE)
    return XRT_FLOW_ERROR;

  /* TODO : Need to calculate the timestamp which we send VCP_PUSH Case */
  priv->last_rcvd_payload.freed_index_cnt--;
  priv->last_rcvd_oidx++;
  *out_size = outsize;

  return XRT_FLOW_OK;

error:
  return fret;
}

static int
ivas_xvcuenc_send_flush (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  int bret = FALSE;
  int iret = 0;
  unsigned int num_idx = 0;

  if (priv->flush_done) {
    INFO_PRINT ("flush already issued to softkernel, hence returning");
    return TRUE;
  }
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_FLUSH;
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }
  memset (payload_data, 0, 1024 * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = VCU_FLUSH;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_FLUSH command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel flush failed");
      goto error;
    }
  }

  DEBUG_PRINT ("successfully sent flush command");
  priv->flush_done = true;

  return TRUE;

error:
  return FALSE;
}

static int
ivas_xvcuenc_deinit (XrtIvas_XVCUEnc * enc)
{
  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0, i;

  if (priv->deinit_done) {
    DEBUG_PRINT("deinit already issued to softkernel, hence returning");
    return TRUE;
  }

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_DEINIT;
  payload_buf->obuf_indexes_to_release_valid_cnt = 0;

  INFO_PRINT ("released buffers sending to deinit %d",
      payload_buf->obuf_indexes_to_release_valid_cnt);

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }
  memset (payload_data, 0, 1024 * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = VCU_DEINIT;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  priv->deinit_done = TRUE;     // irrespective of error

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_DEINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  }
  INFO_PRINT ("completed de-initialization");
  return TRUE;

error:
  return FALSE;
}

static int
gst_ivas_xvcuenc_stop (XrtIvas_XVCUEnc *enc)
{
  int bret = FALSE;

  DEBUG_PRINT ("stop");

  if (enc->priv->init_done) {
    bret = ivas_xvcuenc_send_flush (enc);
    if (bret != TRUE)
      return bret;

    bret = ivas_xvcuenc_deinit (enc);
    if (bret != TRUE)
      return bret;

    enc->priv->init_done = FALSE;
  }

  return bret;
}

static int
gst_ivas_xvcuenc_set_format (XrtIvas_XVCUEnc *enc)
{
  int bret = TRUE;
  if (!enc->priv->init_done) {
    bret = ivas_xvcuenc_preinit (enc);
    if (bret)
      return FALSE;

    bret = ivas_xvcuenc_init (enc);
    if (bret)
      return FALSE;

    enc->priv->init_done = true;
    memset (&enc->priv->last_rcvd_payload, 0x00, sizeof (sk_payload_data));
    enc->priv->last_rcvd_oidx = 0;
  }

  return TRUE;
}

static XrtFlowReturn
gst_ivas_xvcuenc_handle_frame (XrtIvas_XVCUEnc *enc, uint8_t *indata, size_t insize)
{
  int bret = FALSE;

  bret = ivas_xvcuenc_send_frame (enc, indata, insize);
  if (bret != TRUE)
    goto error;

  /* TODO 
  return ivas_xvcuenc_receive_out_frame (enc);
  */

  return XRT_FLOW_OK;

error:
  return XRT_FLOW_ERROR;
}

static XrtFlowReturn
gst_ivas_xvcuenc_finish (XrtIvas_XVCUEnc *enc, uint8_t *out_buffer, uint32_t *out_size)
{
  XrtFlowReturn fret = XRT_FLOW_OK;
  int bret = FALSE;
  uint32_t retry = RETRY_COUNT;

  DEBUG_PRINT ("finish");

  // TODO: add support when encoder not negotiated
  bret = ivas_xvcuenc_send_flush (enc);
  if (bret != TRUE)
    return XRT_FLOW_ERROR;

  do {
    fret = ivas_xvcuenc_receive_out_frame (enc, out_buffer, out_size);
  } while (fret == XRT_FLOW_OK && --retry);

    if (!retry) {
    ERROR_PRINT ("Not received the output.. Retry Done!!\n");
    return XRT_FLOW_ERROR;
  }

  return XRT_FLOW_OK;
}

static void
gst_ivas_xvcuenc_map_params (XrtIvas_XVCUEnc * enc)
{

  XrtIvas_XVCUEncPrivate *priv = enc->priv;
  char params[2048];
  /* Hard coded value for gstreamer related fileds */
  int width = 1920, height = 1080;
  const char *RateCtrlMode = "CBR";
  const char *PrefetchBuffer = "ENABLE";
  const char *format = "NV12";
  const int frame_n = 30, frame_d = 1;
  const char *GopCtrlMode = "DEFAULT_GOP";
  const char *EntropyMode = "MODE_CABAC";
  const char *QPCtrlMode = "AUTO_QP";
  const char *ScalingList = "DEFAULT";
  const char *LoopFilter = "ENABLE";
  const char *AspectRatio = "ASPECT_RATIO_AUTO";
  const char *EnableFillerData = "ENABLE";
  const char *GDRMode = "DISABLE";
  const char *ConstIntraPred = "DISABLE";
  const char *Profile = "AVC_HIGH";
  char SliceQP[10];
  //GstVideoInfo in_vinfo;
  int bret = FALSE;
  int iret;
  unsigned int fsize;

  enc->codec_type = XLNX_CODEC_INVALID;  // TODO SAIF Need to fix
  enc->kernel_name = (char *)GST_IVAS_VIDEO_ENC_KERNEL_NAME_DEFAULT;

  enc->target_bitrate = GST_IVAS_VIDEO_ENC_TARGET_BITRATE_DEFAULT;
  enc->max_bitrate = GST_IVAS_VIDEO_ENC_MAX_BITRATE_DEFAULT;
  enc->min_qp = GST_IVAS_VIDEO_ENC_MIN_QP_DEFAULT;
  enc->max_qp = GST_IVAS_VIDEO_ENC_MAX_QP_DEFAULT;
  enc->cpb_size = GST_IVAS_VIDEO_ENC_CPB_SIZE_DEFAULT;
  enc->initial_delay = GST_IVAS_VIDEO_ENC_INITIAL_DELAY_DEFAULT;
  enc->gop_length = GST_IVAS_VIDEO_ENC_GOP_LENGTH_DEFAULT;
  enc->b_frames = GST_IVAS_VIDEO_ENC_B_FRAMES_DEFAULT;
  enc->periodicity_idr = GST_IVAS_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT;
  enc->level = "5";
  enc->num_slices = GST_IVAS_VIDEO_ENC_NUM_SLICES_DEFAULT;
  enc->slice_size = GST_IVAS_VIDEO_ENC_SLICE_SIZE_DEFAULT;
  enc->num_cores = 0;
  enc->tier = "MAIN_TIER";
  strcpy (SliceQP, "AUTO");
  sprintf (params, "[INPUT]\n"
      "Width = %d\n"
      "Height = %d\n"
      "Format = %s\n"
      "[RATE_CONTROL]\n"
      "RateCtrlMode = %s\n"
      "FrameRate = %d/%d\n"
      "BitRate = %d\n"
      "MaxBitRate = %d\n"
      "SliceQP = %s\n"
      "MaxQP = %u\n"
      "MinQP = %u\n"
      "CPBSize = %f\n"
      "InitialDelay = %f\n"
      "[GOP]\n"
      "GopCtrlMode = %s\n"
      "Gop.GdrMode = %s\n"
      "Gop.Length = %u\n"
      "Gop.NumB = %u\n"
      "Gop.FreqIDR = %d\n"
      "[SETTINGS]\n"
      "Profile = %s\n"
      "Level = %s\n"
      "ChromaMode = CHROMA_4_2_0\n"
      "BitDepth = 8\n"
      "NumSlices = %d\n"
      "QPCtrlMode = %s\n"
      "SliceSize = %d\n"
      "EnableFillerData = %s\n"
      "AspectRatio = %s\n"
      "ColourDescription = COLOUR_DESC_UNSPECIFIED\n"
      "ScalingList = %s\n"
      "EntropyMode = %s\n"
      "LoopFilter = %s\n"
      "ConstrainedIntraPred = %s\n"
      "LambdaCtrlMode = DEFAULT_LDA\n"
      "CacheLevel2 = %s\n"
      "NumCore = %d\n",
    width, height, format, RateCtrlMode, frame_n, frame_d,
    enc->target_bitrate, enc->max_bitrate, SliceQP,
    enc->max_qp, enc->min_qp, (double) (enc->cpb_size) / 1000,
    (double) (enc->initial_delay) / 1000, GopCtrlMode, GDRMode,
    enc->gop_length, enc->b_frames, enc->periodicity_idr, Profile,
    enc->level, enc->num_slices,
    QPCtrlMode, enc->slice_size, EnableFillerData, AspectRatio, ScalingList,
    EntropyMode, LoopFilter, ConstIntraPred, PrefetchBuffer, enc->num_cores);

  priv->static_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->static_cfg_buf == NULL) {
    ERROR_PRINT("failed to allocate encoder config memory handle");
    return;
  }
  fsize = strlen (params);

  iret = alloc_xrt_buffer (priv->xcl_handle, fsize, XCL_BO_DEVICE_RAM, MEM_BANK,
      priv->static_cfg_buf);
  if (iret < 0) {
    ERROR_PRINT("failed to allocate encoder config buffer..");
    goto error;
  }
  strcpy ((char *)priv->static_cfg_buf->user_ptr, params);
  if (xclSyncBO (priv->xcl_handle, priv->static_cfg_buf->bo,
          XCL_BO_SYNC_BO_TO_DEVICE, priv->static_cfg_buf->size, 0)) {
    ERROR_PRINT("unable to sync to static configuration to device");
    goto error1;
  }
  return;

error1:
  free_xrt_buffer (priv->xcl_handle, priv->static_cfg_buf);

error:
  free (priv->static_cfg_buf);
  priv->static_cfg_buf = NULL;
  return;
}

static int
xrt_validate_encoder(uint8_t *out_buffer, uint32_t out_size)
{
  uint8_t *exp_buffer = NULL;
  uint32_t exp_size = 0;

  exp_buffer = (uint8_t *)&encoder_output_buf;
  exp_size = encoder_output_buf_len;

  if (out_size != exp_size)
    return FALSE;

  if (memcmp(out_buffer, exp_buffer, out_size))
    return FALSE;

  return TRUE;
}

int vcu_enc_test(const char *xclbin_path, int sk_idx, int dev_idx)
{
  XrtIvas_XVCUEnc *enc = NULL;
  static XrtFlowReturn fret = XRT_FLOW_ERROR;
  int iret = FALSE;
  uint8_t *in_buffer = NULL;
  uint32_t in_size = 0;
  uint8_t *out_buffer = NULL;
  uint32_t out_size = 0;
  struct timespec tms;

  /* Create local memory for main encoder structure */
  enc = (XrtIvas_XVCUEnc *) calloc (1, sizeof (XrtIvas_XVCUEnc));
  if (!enc) {
    ERROR_PRINT ("failed to allocate encoder memory\n");
    goto error;
  }

  /* Get the input buffer array from the file */
  in_buffer = (uint8_t *)&decoder_output_buf;
  enc->input_buf_size = decoder_output_buf_len;

  /* Initialize xrt and open device */
  iret = gst_ivas_xvcuenc_open (enc, xclbin_path, sk_idx, dev_idx);
  if (iret == FALSE) {
    ERROR_PRINT ("xrt VCU encoder initialization failed!!\n");
    goto error;
  }

  /* Initialize and Configure encoder device */
  iret = gst_ivas_xvcuenc_set_format (enc);
  if (iret == FALSE) {
    ERROR_PRINT ("xrt VCU encoder configuration failed!!\n");
    goto error;
  }

  DEBUG_PRINT ("Decoder initialization is done Succesfully\n");

  /* Prepare and send the input frame buffer */
  fret = gst_ivas_xvcuenc_handle_frame (enc, (uint8_t *)in_buffer, enc->input_buf_size);
  if (fret != XRT_FLOW_OK) {
    ERROR_PRINT ("VCU send command failed!!\n");
    goto error;
  }

  /* Allocate memory for final output buffer */
  out_buffer = (uint8_t *) calloc (1, OUT_MEM_SIZE);
  if (!out_buffer) {
    ERROR_PRINT ("failed to allocate output buffer memory");
    goto error;
  }

  /* Wait for the response of encoded frame buffer from the device */
  fret = gst_ivas_xvcuenc_finish(enc, (uint8_t *)out_buffer, &out_size);
  if (fret != XRT_FLOW_OK) {
    ERROR_PRINT ("VCU receive command failed!!\n");
    goto error;
  }

  /* Response Received. Stop the device and cleanup */
  iret = gst_ivas_xvcuenc_stop(enc);
  if (iret == FALSE) {
    ERROR_PRINT ("VCU encoder stop failed!!\n");
    goto error;
  }

  gst_ivas_xvcuenc_close(enc);

  /* Validate Results */
  iret = xrt_validate_encoder(out_buffer, out_size);
  if (iret == FALSE) {
    INFO_PRINT ("Test validation failed!!\n");
  }

  INFO_PRINT ("Test validation passed!!\n");

  if (enc)
    free (enc);

  if (out_buffer)
    free (out_buffer);

  INFO_PRINT ("***** Test is Done *****\n");

  return TRUE;

error:
  if (enc->priv)
    free (enc->priv);

  if (enc)
    free (enc);

  if (out_buffer)
    free (out_buffer);

  return FALSE;
}
