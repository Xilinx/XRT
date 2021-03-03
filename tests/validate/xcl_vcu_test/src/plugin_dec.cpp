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

#include <stdio.h>
#include <time.h>
#include "plugin_dec.h"
#include "xrt_utils.h"
#include "input_out_frame_dump.h"

#define RETRY_COUNT 100

#undef DEBUG_VCU_DECODER
#define ERROR_PRINT(...) {\
  do {\
    printf("[%s:%d] ERROR[DECODER] : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}

#ifdef DEBUG_VCU_DECODER
#define INFO_PRINT(...) {\
  do {\
    printf("[%s:%d] INFO[DECODER] : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}

#define DEBUG_PRINT(...) {\
  do {\
    printf("[%s:%d] DEBUG[DECODER] : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}
#else
#define DEBUG_PRINT(...) ((void)0)
#define INFO_PRINT(...) ((void)0)
#endif


static void xvcudec_free_internal_buffers (XrtIvas_XVCUDec * dec);

static int
xvcudec_check_softkernel_response (XrtIvas_XVCUDec * dec,
    sk_payload_data * payload_buf)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int iret;

  memset (payload_buf, 0, priv->sk_payload_buf->size);
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    return FALSE;
  }

  /* check response from softkernel */
  if (!payload_buf->cmd_rsp) {
    return FALSE;
  }

  return TRUE;
}

static int
xvcudec_allocate_internal_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int iret = 0, i;

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

  for (i = 0; i < MAX_IBUFFS; i++) {
    priv->in_xrt_bufs[i] = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
    if (priv->in_xrt_bufs[i] == NULL) {
      ERROR_PRINT ("failed to allocate sk payload memory");
      goto error;
    }
  }

  priv->dec_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dec_cfg_buf == NULL) {
    ERROR_PRINT ("failed to allocate decoder config memory handle");
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

  /* allocate decoder config buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (dec_params_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->dec_cfg_buf);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate decoder config buffer..");
    goto error;
  }


  /* allocate softkernel payload buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (sk_payload_data),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->sk_payload_buf);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate softkernel payload buffer..");
    goto error;
  }

  DEBUG_PRINT ("Memory allocation done succesfully..");
  return TRUE;

error:
  xvcudec_free_internal_buffers(dec);

  return FALSE;
}

static void
xvcudec_free_output_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int i = 0;

  if (!priv)
    return;

  if (priv->dec_out_bufs_handle) {
    free_xrt_buffer (priv->xcl_handle, priv->dec_out_bufs_handle);
    free (priv->dec_out_bufs_handle);
    priv->dec_out_bufs_handle = NULL;
  }

  for (i = 0; i < (int)priv->num_out_bufs; i++) {
    if (priv->out_bufs_arr[i]) {
      free_xrt_buffer (priv->xcl_handle, priv->out_bufs_arr[i]);
      free (priv->out_bufs_arr[i]);
      priv->out_bufs_arr[i] = NULL;
    }
  }

  if (priv->out_bufs_arr) {
    free (priv->out_bufs_arr);
    priv->out_bufs_arr = NULL;
  }
}

static int
vcu_dec_outbuffer_alloc_and_map (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  uint64_t *out_bufs_addr;
  int iret = 0, i;

  if (!priv->num_out_bufs || !priv->out_buf_size) {
    ERROR_PRINT ("invalid output allocation parameters : "
        "num_out_bufs = %d & out_buf_size = %lu", priv->num_out_bufs,
        priv->out_buf_size);
    return FALSE;
  }

  DEBUG_PRINT ("minimum number of output buffers required by vcu decoder = %d "
      "and output buffer size = %lu\n", priv->num_out_bufs, priv->out_buf_size);

  priv->dec_out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dec_out_bufs_handle == NULL) {
    ERROR_PRINT ("failed to allocate decoder output buffers structure");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle,
      priv->num_out_bufs * sizeof (uint64_t), XCL_BO_DEVICE_RAM, MEM_BANK,
      priv->dec_out_bufs_handle);
  if (iret < 0) {
    ERROR_PRINT ("failed to allocate decoder out buffers handle..");
    goto error;
  }

  out_bufs_addr = (uint64_t *) (priv->dec_out_bufs_handle->user_ptr);

  if (priv->out_bufs_arr)
    free (priv->out_bufs_arr);

  priv->out_bufs_arr =
      (xrt_buffer **) calloc (priv->num_out_bufs, sizeof (xrt_buffer *));
  if (!priv->out_bufs_arr) {
    ERROR_PRINT ("failed to allocate memory");
    goto error;
  }

  for (i = 0; i < (int)priv->num_out_bufs; i++) {
    xrt_buffer *outmem = NULL;

    outmem = (xrt_buffer *) calloc (1, OUT_MEM_SIZE);
    if (!outmem) {
      ERROR_PRINT ("outmem buffer allocation failed\n");
      goto error;
    }

    iret = alloc_xrt_buffer (priv->xcl_handle,
      OUT_MEM_SIZE, XCL_BO_DEVICE_RAM, MEM_BANK, outmem);
    if (iret < 0) {
      ERROR_PRINT ("Failed to acquire %d-th buffer", i);
      goto error;
    }

    DEBUG_PRINT ("Output buffer Index %d Details : Phy Addr : %lx, Size %d bo %d\n", 
          i, outmem->phy_addr, outmem->size, outmem->bo);
    out_bufs_addr[i] = outmem->phy_addr;

    priv->out_bufs_arr[i] = outmem;
  }

  iret = xclSyncBO (priv->xcl_handle, priv->dec_out_bufs_handle->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dec_out_bufs_handle->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("syncbo failed - %d, reason : %s\n", iret,
        strerror (errno));
    goto error;
  }

  return TRUE;

error:
  xvcudec_free_output_buffers(dec);

  return FALSE;
}

static void 
gstivas_xvcudec_close (XrtIvas_XVCUDec *dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;

  DEBUG_PRINT ("decoder device closing");

  /* free all output buffers allocated */
  xvcudec_free_output_buffers (dec);

  /* free all internal buffers */
  xvcudec_free_internal_buffers (dec);

  /* Close XRT context */
  xclCloseContext (priv->xcl_handle, priv->xclbinId, 0);
  xclClose (priv->xcl_handle);

  if (priv)
    free (priv);
}

static int
xvcudec_preinit (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  dec_params_t *dec_cfg;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  struct timespec init_time;
  unsigned int num_idx = 0;
  int bret = FALSE;
  int iret = 0;

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PREINIT;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s\n", iret,
        strerror (errno));
    return FALSE;
  }

  /* update decoder config params */
  dec_cfg = (dec_params_t *) (priv->dec_cfg_buf->user_ptr);
  memset (dec_cfg, 0, priv->dec_cfg_buf->size);

  /* As XRT doesn't have knowledge about video frame, based on the input 
   * frame we are using for this test, hard-coding the following configuration here.
   */
  dec_cfg->codec_type = 0; // input stream is H264
  dec_cfg->bitdepth = dec->bit_depth;
  dec_cfg->low_latency = dec->low_latency;
  dec_cfg->entropy_buffers_count = dec->num_entropy_bufs;
  dec_cfg->frame_rate = 30;
  dec_cfg->clk_ratio = 1;
  dec_cfg->width = 1920;
  dec_cfg->height = 1080;
  dec_cfg->level = 40;
  dec_cfg->profile =100;
  dec_cfg->scan_type = 1;
  dec_cfg->chroma_mode = 420;

  DEBUG_PRINT ("Video Info : frame_rate %d clk_ratio %d width %d height %d level %d profile %d chroma_mode %d\n", 
		  dec_cfg->frame_rate, dec_cfg->clk_ratio, dec_cfg->width, dec_cfg->height, 
      dec_cfg->level, dec_cfg->profile, dec_cfg->chroma_mode);
  iret =
      xclSyncBO (priv->xcl_handle, priv->dec_cfg_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dec_cfg_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("syncbo failed - %d, reason : %s\n", iret,
        strerror (errno));
    return FALSE;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
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
  payload_data[num_idx++] = priv->dec_cfg_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->dec_cfg_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->dec_cfg_buf->size;

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_PREINIT command to softkernel - %d, reason : %s\n",
        iret, strerror (errno));
    return FALSE;
  } else {
    bret = xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel pre-initialization failed\n");
      return FALSE;
    }
  }

  priv->num_out_bufs = payload_buf->obuff_num;
  priv->out_buf_size = payload_buf->obuff_size;

  DEBUG_PRINT ("min output buffers required by softkernel %d and outbuf size %lu\n",
      priv->num_out_bufs, priv->out_buf_size);

  DEBUG_PRINT ("VCU pre-initialization successful..\n");  

  return TRUE;
}

static int
xvcudec_init (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  int iret = 0, i;
  int bret = FALSE;

  priv->max_ibuf_size = dec->input_buf_size;

  /* Sending command for VCU init */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  payload_buf->cmd_id = VCU_INIT;
  payload_buf->obuff_num = priv->num_out_bufs;

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("syncbo failed - %d, reason : %s", iret,
        strerror (errno));
    return FALSE;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));

  payload_data[num_idx++] = 0;

  payload_data[num_idx++] = VCU_INIT;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;

  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);
  
  for (i = 0; i < MAX_IBUFFS; i++) {
    /* allocate input buffer */
    iret = alloc_xrt_buffer (priv->xcl_handle, priv->max_ibuf_size,
        XCL_BO_DEVICE_RAM, MEM_BANK, priv->in_xrt_bufs[i]);
    if (iret < 0) {
      ERROR_PRINT ("failed to allocate input buffer..");
      return FALSE;
    }

    payload_data[num_idx++] = priv->in_xrt_bufs[i]->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
        ((uint64_t) (priv->in_xrt_bufs[i]->phy_addr) >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = priv->in_xrt_bufs[i]->size;
  }

  payload_data[num_idx++] = priv->dec_out_bufs_handle->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->dec_out_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->dec_out_bufs_handle->size;

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_INIT command to softkernel - %d, reason : %s\n",
        iret, strerror (errno));
    return FALSE;
  } else {
    bret = xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel initialization failed\n");
      return FALSE;
    }
  }

  priv->init_done = TRUE;

  DEBUG_PRINT ("VCU initialization successful..\n");  
  
  return TRUE;
}

static XrtFlowReturn
xvcudec_read_out_buffer (XrtIvas_XVCUDec * dec, uint32_t idx, uint8_t *out_buffer, uint32_t *out_size)
{
  int rc = 0;
  XrtFlowReturn fret = XRT_FLOW_ERROR;

  if (idx == 0xBAD) {
    ERROR_PRINT ("bad output index received...\n");
    return XRT_FLOW_ERROR;
  }

  DEBUG_PRINT ("reading output buffer at index %d\n", idx);

  xrt_buffer *out_buf =  dec->priv->out_bufs_arr[idx];
  /* transfer output frame contents from device */
  rc = xclSyncBO (dec->priv->xcl_handle,
		  out_buf->bo,
		  XCL_BO_SYNC_BO_FROM_DEVICE, OUT_MEM_SIZE, 0);
  if (rc != 0) {
	  ERROR_PRINT ("xclSyncBO failed %d\n", rc);
	  return fret;
  }

  rc = xclReadBO(dec->priv->xcl_handle, out_buf->bo, out_buffer, OUT_MEM_SIZE, 0);
  if (rc != 0) {
	  ERROR_PRINT ("xclReadBO failed %d\n", rc);
	  return fret;
  }

  *out_size = out_buf->size;

  return fret;
}

static int
xvcudec_send_flush (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  int bret = FALSE;
  int iret = 0;
  unsigned int num_idx = 0;

  if (priv->flush_done == TRUE) {
    DEBUG_PRINT ("flush already issued to softkernel, hence returning\n");
    return TRUE;
  }
  
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_FLUSH;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    return FALSE;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
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
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_FLUSH command to softkernel - %d", iret);
    return FALSE;
  } else {
    bret = xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel flush failed");
      return FALSE;
    }
  }

  DEBUG_PRINT ("successfully sent flush command\n");
  priv->flush_done = TRUE;

  return TRUE;
}

static int
xvcudec_deinit (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  int iret = 0;

  if (priv->deinit_done == TRUE) {
    DEBUG_PRINT ("deinit already issued to softkernel, hence returning");
    return TRUE;
  }

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_DEINIT;
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    return FALSE;
  }
  
  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
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
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_DEINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    return FALSE;
  }

  DEBUG_PRINT ("Successfully deinitialized softkernel\n");
  return TRUE;
}

static int
gstivas_xvcudec_stop (XrtIvas_XVCUDec *dec)
{
  int bret = TRUE;

  if (dec->priv->init_done) {
    bret = xvcudec_send_flush (dec);
    if (bret != TRUE)
      return bret;

    bret = xvcudec_deinit (dec);
    if (bret != TRUE)
      return bret;

    dec->priv->init_done = FALSE;
  }

  return bret;
}

static int
xvcudec_prepare_send_frame (XrtIvas_XVCUDec * dec, void *inbuf,
    size_t insize, uint32_t *payload_data, uint32_t *payload_num_idx)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  int iret = 0, i;
  uint32_t num_idx = 0;
  struct timespec tms;
  /* POSIX.1-2008 way */
  if (clock_gettime(CLOCK_REALTIME, &tms)) {
	  return -1;
  }

  DEBUG_PRINT ("sending input buffer index %d with size %lu\n",
      priv->host_to_dev_ibuf_idx, insize);

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PUSH;
  payload_buf->ibuff_valid_size = insize;
  payload_buf->ibuff_meta.pts = tms.tv_sec * 1000000; 
  payload_buf->host_to_dev_ibuf_idx = priv->host_to_dev_ibuf_idx;

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;

  payload_data[num_idx++] = VCU_PUSH;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;

  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);
  payload_data[num_idx++] =
      priv->in_xrt_bufs[priv->host_to_dev_ibuf_idx]->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->in_xrt_bufs[priv->
              host_to_dev_ibuf_idx]->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->max_ibuf_size;

  /* reset all free out buf indexes */
  for (i = 0; i < MAX_OUT_INFOS; i++)
    payload_buf->obuf_info[i].freed_obuf_index = 0xBAD;

  /* transfer payload settings to device */
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    ERROR_PRINT ("synbo failed - %d, reason : %s\n", iret,
        strerror (errno));
    ERROR_PRINT ("failed to sync PUSH command payload to device. reason : %s\n",
            strerror (errno));
    return FALSE;
  }

  *payload_num_idx = num_idx;

  DEBUG_PRINT ("Input command prepared successfully..\n");

  return TRUE;
}

static XrtFlowReturn
xvcudec_receive_out_frames (XrtIvas_XVCUDec * dec, uint8_t *out_buffer, uint32_t *out_size)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  XrtFlowReturn fret = XRT_FLOW_OK;
  int iret = 0;
  int bret = FALSE;

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
    return XRT_FLOW_ERROR;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;

  payload_data[num_idx++] = VCU_RECEIVE;
  payload_data[num_idx++] = getpid();
  payload_data[num_idx++] = priv->timestamp & 0xFFFFFFFF;
  payload_data[num_idx++] = (priv->timestamp  >> 32) & 0xFFFFFFFF;

  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  DEBUG_PRINT ("sending VCU_RECEIVE command to softkernel\n");
  /* send command to softkernel */
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_RECEIVE command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    return XRT_FLOW_ERROR;
  } else {
    bret = xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel receive frame failed");
      return XRT_FLOW_ERROR;
    }
  }

  DEBUG_PRINT ("successfully completed VCU_RECEIVE command\n");

  DEBUG_PRINT ("number of available output buffers %d for consumption\n",
      payload_buf->free_index_cnt);

  memcpy (&priv->last_rcvd_payload, payload_buf, sizeof (sk_payload_data));

  if (priv->last_rcvd_payload.free_index_cnt) {
    priv->last_rcvd_oidx = 0;
    fret =
        xvcudec_read_out_buffer (dec,
        priv->last_rcvd_payload.obuff_index[priv->last_rcvd_oidx], out_buffer, out_size);

    if (fret != XRT_FLOW_OK)
      goto exit;

    priv->last_rcvd_payload.free_index_cnt--;
    priv->last_rcvd_oidx++;

  } else if (payload_buf->end_decoding) {
    DEBUG_PRINT ("EOS recevied from softkernel\n");
    fret = XRT_FLOW_EOS;

    goto exit;
  }

  DEBUG_PRINT ("softkernel receive successful fret %d\n", fret);

exit:
  return fret;
}

static XrtFlowReturn
gstivas_xvcudec_handle_frame (XrtIvas_XVCUDec *dec,
    uint8_t *indata, size_t insize)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  XrtFlowReturn fret = XRT_FLOW_OK;
  sk_payload_data *payload_buf = NULL;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  uint32_t num_idx = 0;
  uint32_t retry = RETRY_COUNT;
  int iret = 0;
  int bret = TRUE;

  if (indata) {
    /* copy input frame buffer to xrt memory */
    iret = xclWriteBO (priv->xcl_handle,
        priv->in_xrt_bufs[priv->host_to_dev_ibuf_idx]->bo, indata, insize, 0);
    if (iret != 0) {
      ERROR_PRINT ("failed to write input frame to xrt memory. "
          "reason : %s\n", strerror (errno));
      fret = XRT_FLOW_ERROR;
      return fret;
    }

    /* transfer input frame contents to device */
    iret = xclSyncBO (priv->xcl_handle,
        priv->in_xrt_bufs[priv->host_to_dev_ibuf_idx]->bo,
        XCL_BO_SYNC_BO_TO_DEVICE, insize, 0);
    if (iret != 0) {
      ERROR_PRINT ("failed to sync input frame. reason : %s\n",
          strerror (errno));
      fret = XRT_FLOW_ERROR;
      return fret;
    }
  }

  bret = xvcudec_prepare_send_frame (dec, indata, insize, payload_data,
      &num_idx);
  if (bret != TRUE) {
    ERROR_PRINT ("failed to prepare send frame command\n");
    fret = XRT_FLOW_ERROR;
    return fret;
  }

  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);

try_again:
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    ERROR_PRINT ("failed to send VCU_PUSH command to softkernel - %d, " "reason : %s",
        iret, strerror (errno));
    fret = XRT_FLOW_ERROR;
    return fret;
  } else {
    bret = xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      ERROR_PRINT ("softkernel send frame failed\n");
      fret = XRT_FLOW_ERROR;
      return fret;
    }
  }

  if (payload_buf->dev_to_host_ibuf_idx != 0xBAD) {
    priv->host_to_dev_ibuf_idx = payload_buf->dev_to_host_ibuf_idx;
    DEBUG_PRINT ("Input buffer index %d consumed by device\n",
        priv->host_to_dev_ibuf_idx);
  } else {
    DEBUG_PRINT ("input buffer index %d not consumed, try again...",
        priv->host_to_dev_ibuf_idx);

    if (--retry)
      goto try_again;
  }

  DEBUG_PRINT ("Successfully completed VCU_PUSH command\n");

  return XRT_FLOW_OK;
}

static XrtFlowReturn
gstivas_xvcudec_finish (XrtIvas_XVCUDec *dec, uint8_t *out_buffer, uint32_t *out_size)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  XrtFlowReturn fret = XRT_FLOW_OK;
  int bret = FALSE;
  uint32_t retry = RETRY_COUNT;

  if (priv->init_done != TRUE)
    return XRT_FLOW_OK;

  // TODO: add support when decoder not negotiated
  bret = xvcudec_send_flush (dec);
  if (bret != TRUE) {
    return XRT_FLOW_ERROR;
  }

  do {
    fret = xvcudec_receive_out_frames (dec, out_buffer, out_size);
  } while (fret == XRT_FLOW_OK && --retry);

  if (!retry) {
    ERROR_PRINT ("Not received the output.. Retry Done!!\n");
    return XRT_FLOW_ERROR;
  }

  return XRT_FLOW_OK;
}

static void
xvcudec_free_internal_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int i;

  if (!priv)
    return;

  if (priv->dec_cfg_buf) {
    free_xrt_buffer (priv->xcl_handle, priv->dec_cfg_buf);
    free (priv->dec_cfg_buf);
    priv->dec_cfg_buf = NULL;
  }

  for (i = 0; i < MAX_IBUFFS; i++) {
    if (priv->in_xrt_bufs[i]) {
      free_xrt_buffer (priv->xcl_handle, priv->in_xrt_bufs[i]);
      free (priv->in_xrt_bufs[i]);
      priv->in_xrt_bufs[i] = NULL;
    }
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
xrt_initialization (XrtIvas_XVCUDec *dec)
{
  XrtIvas_XVCUDecPrivate *priv = NULL;
  unsigned dev_index = 0;
  int cu_index = 0;
  int iret = FALSE;

  if (!dec) {
    ERROR_PRINT ("Invalid memory\n");
    return iret;
  }

  priv = dec->priv;
  dev_index = dec->dev_index;

  if (iret = download_xclbin (dec->xclbin_path, dev_index, &cu_index, &(priv->xcl_handle),
          &(priv->xclbinId))) {
    if (iret < 0)
      ERROR_PRINT ("failed to download xclbin %s]", dec->xclbin_path);

    return NOTSUPP;
  }

  if (xclOpenContext (priv->xcl_handle, priv->xclbinId, cu_index, true)) {
    ERROR_PRINT ("failed to get xclOpenContext...\n");
    return iret;
  }

  DEBUG_PRINT ("Initialization of XRT is successfully. xrt handle = %p", priv->xcl_handle);

  iret = xvcudec_allocate_internal_buffers (dec);
  if (iret == FALSE) {
    ERROR_PRINT ("failed to allocate internal buffers\n");
    return iret;
  }

  return TRUE;
}

static int
xvcudec_open (XrtIvas_XVCUDec *dec, const char *xclbin_path, int sk_idx, int dev_idx)
{
  XrtIvas_XVCUDecPrivate *priv = NULL;
  int iret = FALSE;

  if (!dec)
    return FALSE;

  priv = (XrtIvas_XVCUDecPrivate *) calloc (1, sizeof (XrtIvas_XVCUDecPrivate));
  if (!priv) {
    ERROR_PRINT ("failed to allocate private memory structure");
    return FALSE;
  }

  dec->priv = priv;

  /* gstreamer specific initialization */
  dec->bit_depth = 8;
  dec->num_entropy_bufs = 2;
  dec->sk_start_idx = -1;
  dec->low_latency = 0;

  /* Update device and vcu index here */
  dec->xclbin_path = (char *) xclbin_path;
  dec->dev_index = dev_idx;
  dec->sk_cur_idx = sk_idx;

  priv->init_done = FALSE;
  priv->flush_done = FALSE;
  priv->max_ibuf_size = dec->input_buf_size;
  priv->host_to_dev_ibuf_idx = 0;

  /* Initialize Xrt and get the device context */
  iret = xrt_initialization (dec);
  if (iret != TRUE) {
    if (dec->priv) {
      free (dec->priv);
      dec->priv = NULL;
    }

    if (iret == FALSE) 
      ERROR_PRINT ("xrt initialization failed!!\n");

    return iret;
  }

  return TRUE;
}


static int
xvcudec_set_format (XrtIvas_XVCUDec * dec)
{
  int iret = FALSE;

  if (!dec)
    return FALSE;

  /* softkernel pre-init initiated */
  iret = xvcudec_preinit (dec);
  if (iret != TRUE) {
    ERROR_PRINT ("failed to pre-init vcu decoder!!\n");
    return FALSE;
  }

  /* Going to allocate output buffer based on the negotiation */
  iret = vcu_dec_outbuffer_alloc_and_map (dec);
  if (iret != TRUE) {
    ERROR_PRINT ("failed to allocate & map output buffers!!\n");
    return FALSE;
  }

  /* softkernel initalization */
  iret = xvcudec_init(dec);
  if (iret != TRUE) {
    ERROR_PRINT ("failed to init vcu decoder!!\n");
    return FALSE;
  }

  return TRUE;
}

static int
xrt_validate_output(uint8_t *out_buffer, uint32_t out_size)
{
  uint8_t *exp_buffer = NULL;
  uint32_t exp_size = 0;

  exp_buffer = (uint8_t *)&expected_out_frame;
  exp_size = sizeof (expected_out_frame);

  if (out_size != exp_size)
    return FALSE;

  if (memcmp(out_buffer, exp_buffer, out_size))
    return FALSE;

  return TRUE;
}

int vcu_dec_test(const char *xclbin_path, int sk_idx, int dev_idx)
{
  XrtIvas_XVCUDec *dec = NULL;
  static XrtFlowReturn fret = XRT_FLOW_ERROR;
  int iret = FALSE;
  uint8_t *in_buffer = NULL;
  uint32_t in_size = 0;
  uint8_t *out_buffer = NULL;
  uint32_t out_size = 0;

  /* Create local memory for main decoder structure */
  dec = (XrtIvas_XVCUDec *) calloc (1, sizeof (XrtIvas_XVCUDec));
  if (!dec) {
    ERROR_PRINT ("failed to allocate decoder memory\n");
    goto error;
  }

  /* Get the input buffer array from the file */
  in_buffer = (uint8_t *)&input_frame;
  in_size = sizeof (input_frame);
  dec->input_buf_size = in_size;

  /* Initialize xrt and open device */
  iret = xvcudec_open (dec, xclbin_path, sk_idx, dev_idx);
  if (iret != TRUE) {
    goto error;
  }

  /* Initialize and Configure decoder device */
  iret = xvcudec_set_format (dec);
  if (iret == FALSE) {
    ERROR_PRINT ("xrt VCU decoder configuration failed!!\n");
    goto error;
  }

  DEBUG_PRINT ("Decoder initialization is done Succesfully\n");

  /* Prepare and send the input frame buffer */
  fret = gstivas_xvcudec_handle_frame (dec, (uint8_t *)in_buffer, in_size);
  if (fret != XRT_FLOW_OK) {
    ERROR_PRINT ("VCU send command failed!!\n");
    iret = FALSE;
    goto error;
  }

  /* Allocate memory for final output buffer */
  out_buffer = (uint8_t *) calloc (1, OUT_MEM_SIZE);
  if (!out_buffer) {
    ERROR_PRINT ("failed to allocate output buffer memory");
    iret = FALSE;
    goto error;
  }

  /* Wait for the response of decoded frame buffer from the device */
  fret = gstivas_xvcudec_finish(dec, (uint8_t *)out_buffer, &out_size);
  if (fret != XRT_FLOW_OK) {
    ERROR_PRINT ("VCU receive command failed!!\n");
    iret = FALSE;
    goto error;
  }

  /* Response Received. Stop the device and cleanup */
  iret = gstivas_xvcudec_stop(dec);
  if (iret == FALSE) {
    ERROR_PRINT ("VCU decoder stop failed!!\n");
    goto error;
  }

  gstivas_xvcudec_close(dec);

  /* Validate Results */ 
  iret = xrt_validate_output(out_buffer, out_size);
  if (iret == FALSE) {
    INFO_PRINT ("Test validation failed!!\n");
  }
    
  INFO_PRINT ("Test validation passed!!\n");

  if (dec)
    free (dec);

  if (out_buffer)
    free (out_buffer);

  INFO_PRINT ("***** Test is Done *****\n");

  return TRUE;

error:
  if (dec->priv)
    free (dec->priv);

  if (dec)
    free (dec);

  if (out_buffer)
    free (out_buffer);

  return iret;
} 
