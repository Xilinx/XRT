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
#include "dummy_plugin_dec.h"
#include "xrt_utils.h"

#define guint uint32_t
#define gchar char
#define GST_IVAS_XVCUDEC_PRIVATE(dec) (XrtIvas_XVCUDecPrivate *) (dec->priv)
#define GST_IVAS_XVCUDEC(decoder) (XrtIvas_XVCUDec *) (decoder->dec) 

void hexDump_1 (const char * desc, const void * addr, const int len);

static boolean
ivas_xvcudec_check_softkernel_response (XrtIvas_XVCUDec * dec,
    sk_payload_data * payload_buf)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int iret;

  printf("%s : Before SyncBo, payload_buf %p\n", __func__, (void *)payload_buf);
  memset (payload_buf, 0, priv->sk_payload_buf->size);
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    return FALSE;
  }

  printf("%s : After SyncBo iret %d \n", __func__, iret);

  printf("%s : After SyncBo payload_buf->cmd_rsp %d \n", __func__, payload_buf->cmd_rsp);

  /* check response from softkernel */
  if (!payload_buf->cmd_rsp) {
    printf("Returning False\n");
    return FALSE;
  }

  printf("Returning True\n");
  return TRUE;
}

static boolean
ivas_xvcudec_allocate_internal_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int iret = 0, i;

  priv->ert_cmd_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->ert_cmd_buf == NULL) {
    printf("failed to allocate ert cmd memory");
    goto error;
  }

  priv->sk_payload_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->sk_payload_buf == NULL) {
    printf("failed to allocate sk payload memory");
    goto error;
  }

  for (i = 0; i < MAX_IBUFFS; i++) {
    priv->in_xrt_bufs[i] = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
    if (priv->in_xrt_bufs[i] == NULL) {
      printf("failed to allocate sk payload memory");
      goto error;
    }
  }

  priv->dec_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dec_cfg_buf == NULL) {
    printf("failed to allocate decoder config memory handle");
    goto error;
  }

  /* allocate ert command buffer */
  iret =
      alloc_xrt_buffer (priv->xcl_handle, ERT_CMD_SIZE, XCL_BO_SHARED_VIRTUAL,
      1 << 31, priv->ert_cmd_buf);
  if (iret < 0) {
    printf("failed to allocate ert command buffer..");
    goto error;
  }

    printf("*********************** ert_cmd_buf physical address : %llx\n", (uint64_t)priv->ert_cmd_buf->phy_addr);
  /* allocate softkernel payload buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (sk_payload_data),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->sk_payload_buf);
  if (iret < 0) {
    printf("failed to allocate softkernel payload buffer..");
    goto error;
  }
  printf("*********************** sk_payload_buf physical address : %llx\n", (uint64_t)priv->sk_payload_buf->phy_addr);


  printf("*********************** ert_cmd_buf physical address : %llx\n", (uint64_t)priv->ert_cmd_buf->phy_addr);
  /* allocate softkernel payload buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (sk_payload_data),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->sk_payload_buf);
  if (iret < 0) {
    printf("failed to allocate softkernel payload buffer..");
    goto error;
  }
  printf("*********************** sk_payload_buf physical address : %llx\n", (uint64_t)priv->sk_payload_buf->phy_addr);

  /* allocate decoder config buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (dec_params_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->dec_cfg_buf);
  if (iret < 0) {
    printf("failed to allocate decoder config buffer..");
    goto error;
  }
  printf("*********************** dec_cfg_buf physical address : %llx\n", (uint64_t)priv->dec_cfg_buf->phy_addr);

  return TRUE;

error:
  return FALSE;
}

static void
ivas_xvcudec_free_internal_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int i;

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

static boolean
ivas_vcu_dec_outbuffer_alloc_and_map (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  uint64_t *out_bufs_addr;
  int iret = 0, i;

  if (!priv->num_out_bufs || !priv->out_buf_size) {
    printf("invalid output allocation parameters : "
        "num_out_bufs = %d & out_buf_size = %lu", priv->num_out_bufs,
        priv->out_buf_size);
    return FALSE;
  }

  printf("minimum number of output buffers required by vcu decoder = %d "
      "and output buffer size = %lu\n", priv->num_out_bufs, priv->out_buf_size);

  priv->dec_out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dec_out_bufs_handle == NULL) {
    printf(        "failed to allocate decoder output buffers structure");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle,
      priv->num_out_bufs * sizeof (uint64_t), XCL_BO_DEVICE_RAM, MEM_BANK,
      priv->dec_out_bufs_handle);
  if (iret < 0) {
    printf("failed to allocate decoder out buffers handle..");
    goto error;
  }

  out_bufs_addr = (uint64_t *) (priv->dec_out_bufs_handle->user_ptr);

  if (priv->out_bufs_arr)
    free (priv->out_bufs_arr);

  priv->out_bufs_arr =
      (xrt_buffer **) calloc (priv->num_out_bufs, sizeof (xrt_buffer *));
  if (!priv->out_bufs_arr) {
    printf("failed to allocate memory");
    goto error;
  }

  for (i = 0; i < priv->num_out_bufs; i++) {
    xrt_buffer *outmem = NULL;

    XlnxOutputBuffer *xlnx_buf =
        (XlnxOutputBuffer *) calloc (1, sizeof (XlnxOutputBuffer));
    if (xlnx_buf == NULL) {
      printf("failed to allocate decoder output buffer");
      goto error;
    }

    outmem = (xrt_buffer *) calloc (1, 3342336);
    if (!outmem) {
      printf("outmem buffer allocation failed\n");
      goto error;
    }

    iret = alloc_xrt_buffer (priv->xcl_handle,
      3342336, XCL_BO_DEVICE_RAM, MEM_BANK, outmem);
    if (iret < 0) {
      printf("Failed to acquire %d-th buffer", i);
      goto error;
    }

    xlnx_buf->idx = i;
    xlnx_buf->xrt_buf.phy_addr = outmem->phy_addr;
    xlnx_buf->xrt_buf.size = outmem->size;
    xlnx_buf->xrt_buf.bo = outmem->bo;

    printf("Phy %lx, Size %d bo %d\n", outmem->phy_addr, outmem->size, outmem->bo);
    out_bufs_addr[i] = xlnx_buf->xrt_buf.phy_addr;

    priv->out_bufs_arr[i] = outmem;


    printf("output [%d] : mapping memory %p with paddr = %p, out_bufs_arr %p\n", i,
        outmem, (void *) xlnx_buf->xrt_buf.phy_addr, (void *)priv->out_bufs_arr[i]);


  }

  iret = xclSyncBO (priv->xcl_handle, priv->dec_out_bufs_handle->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dec_out_bufs_handle->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s\n", iret,
        strerror (errno));
    goto error;
  }

  priv->outbufs_allocated = TRUE;

  return TRUE;

error:
  return FALSE;
}

#if 0
static boolean
ivas_vcu_dec_outbuffer_alloc_and_map (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  uint64_t *out_bufs_addr;
  int iret = 0, i;

  printf(" >>>>>>>>>>>>>>>> Entering %s : %d >>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  if (!priv->num_out_bufs || !priv->out_buf_size) {
    printf("invalid output allocation parameters : "
        "num_out_bufs = %d & out_buf_size = %lu\n", priv->num_out_bufs,
        priv->out_buf_size);
    return FALSE;
  }

  printf(
      "minimum number of output buffers required by vcu decoder = %d "
      "and output buffer size = %lu\n", priv->num_out_bufs, priv->out_buf_size);

  /* TODO AGAIN */
#if 0 
  gst_ivas_buffer_pool_set_release_buffer_cb ((GstIvasBufferPool *) priv->pool,
      ivas_vcu_dec_release_buffer_cb, dec);
#endif

  priv->dec_out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dec_out_bufs_handle == NULL) {
    printf("failed to allocate decoder output buffers structure\n");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle,
      priv->num_out_bufs * sizeof (uint64_t), XCL_BO_DEVICE_RAM, MEM_BANK,
      priv->dec_out_bufs_handle);
  if (iret < 0) {
    printf("failed to allocate decoder out buffers handle..");
    goto error;
  }

  out_bufs_addr = (uint64_t *) (priv->dec_out_bufs_handle->user_ptr);

  printf(" >>>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* TODO AGAIN */
  printf("out_bufs_arr : %s : %d\n", __func__, __LINE__);
  if (priv->out_bufs_arr)
    free (priv->out_bufs_arr);

  printf("out_bufs_arr : %s : %d\n", __func__, __LINE__);
  priv->out_bufs_arr =
      (xrt_buffer **) calloc (priv->num_out_bufs, sizeof (xrt_buffer *));
  if (!priv->out_bufs_arr) {
    printf("failed to allocate memory");
    goto error;
  }

#if 1
  for (i = 0; i < priv->num_out_bufs; i++) {
    //GstMemory *outmem = NULL;
    //GstBuffer *outbuf = NULL;

    XlnxOutputBuffer *xlnx_buf =
        (XlnxOutputBuffer *) calloc (1, sizeof (XlnxOutputBuffer));
    if (xlnx_buf == NULL) {
      printf("failed to allocate decoder output buffer");
      goto error;
    }

#if 0
    if (gst_buffer_pool_acquire_buffer (priv->pool, &outbuf,
            NULL) != GST_FLOW_OK) {
      GST_INFO_OBJECT (dec, "Failed to acquire %d-th buffer", i);
      goto error;
    }

    outmem = gst_buffer_get_memory (outbuf, 0);

    printf("outmem %p, outbuf %p\n", (void *)outmem, (void *)outbuf);
    if (!gst_is_ivas_memory (outmem)) {
      GST_ERROR_OBJECT (dec, "not an xrt memory");
      gst_memory_unref (outmem);
      gst_buffer_unref (outbuf);
      goto error;
    }
    xlnx_buf->idx = i;
    xlnx_buf->xrt_buf.phy_addr = gst_ivas_allocator_get_paddr (outmem);
    xlnx_buf->xrt_buf.size = gst_buffer_get_size (outbuf);
    xlnx_buf->xrt_buf.bo = gst_ivas_allocator_get_bo (outmem);
    printf("phy_addr %lx, size %d, bo %lx, user ptr %p\n", xlnx_buf->xrt_buf.phy_addr, xlnx_buf->xrt_buf.size, xlnx_buf->xrt_buf.bo, (void *)xlnx_buf->xrt_buf.user_ptr);
#endif
    xrt_buffer buf;
    xrt_buffer *outmem = &buf;
    /* allocate out buffer */
    iret = alloc_xrt_buffer (priv->xcl_handle, 3342336,
        XCL_BO_DEVICE_RAM, MEM_BANK, outmem);
    if (iret < 0) {
      printf("failed to allocate input buffer..");
      goto error;
    }

    xlnx_buf->idx = i;
    xlnx_buf->xrt_buf.phy_addr = outmem->phy_addr;
    xlnx_buf->xrt_buf.size = 3342336;
    xlnx_buf->xrt_buf.bo = outmem->bo;
    printf("phy_addr %lx, size %d, bo %lx, user ptr %p\n", xlnx_buf->xrt_buf.phy_addr, xlnx_buf->xrt_buf.size, xlnx_buf->xrt_buf.bo, (void *)xlnx_buf->xrt_buf.user_ptr);

    out_bufs_addr[i] = xlnx_buf->xrt_buf.phy_addr;

    //g_hash_table_insert (priv->oidx_hash, outmem, xlnx_buf);
    priv->out_bufs_arr[i] = outmem;
  printf("out_bufs_arr : %s : %d\n", __func__, __LINE__);

    printf("output [%d] : mapping memory %p with paddr = %p, out_bufs_arr %p\n", i,
        outmem, (void *) xlnx_buf->xrt_buf.phy_addr, (void *)priv->out_bufs_arr[i]);

    //gst_memory_unref (outmem);
  }
  printf("\n\t\t\t ################################# %s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__,
                        dec->priv->out_bufs_arr[0], dec->priv->out_bufs_arr[0]->phy_addr, dec->priv->out_bufs_arr[0]->bo);
#endif

#if 0
  for (i = 0; i < priv->num_out_bufs; i++) {
    GstMemory *outmem = NULL;
    GstBuffer *outbuf = NULL;

    XlnxOutputBuffer *xlnx_buf =
        (XlnxOutputBuffer *) calloc (1, sizeof (XlnxOutputBuffer));
    if (xlnx_buf == NULL) {
      printf("failed to allocate decoder output buffer");
      goto error;
    }

    if (gst_buffer_pool_acquire_buffer (priv->pool, &outbuf,
            NULL) != XRT_FLOW_OK) {
      GST_INFO_OBJECT (dec, "Failed to acquire %d-th buffer", i);
      goto error;
    }

    outmem = gst_buffer_get_memory (outbuf, 0);

    printf("outmem %p, outbuf %p\n", (void *)outmem, (void *)outbuf);
    if (!gst_is_ivas_memory (outmem)) {
      printf("not an xrt memory");
      gst_memory_unref (outmem);
      gst_buffer_unref (outbuf);
      goto error;
    }

    xlnx_buf->idx = i;
    xlnx_buf->xrt_buf.phy_addr = gst_ivas_allocator_get_paddr (outmem);
    xlnx_buf->xrt_buf.size = gst_buffer_get_size (outbuf);
    xlnx_buf->xrt_buf.bo = gst_ivas_allocator_get_bo (outmem);
    printf("phy_addr %lx, size %d, bo %lx, user ptr %p\n", xlnx_buf->xrt_buf.phy_addr, xlnx_buf->xrt_buf.size, xlnx_buf->xrt_buf.bo, (void *)xlnx_buf->xrt_buf.user_ptr);

    out_bufs_addr[i] = xlnx_buf->xrt_buf.phy_addr;

    g_hash_table_insert (priv->oidx_hash, outmem, xlnx_buf);
    priv->out_bufs_arr[i] = outbuf;

    printf("output [%d] : mapping memory %p with paddr = %p", i,
        outmem, (void *) xlnx_buf->xrt_buf.phy_addr);
    printf("output [%d] : mapping memory %p with paddr = %p\n", i,
        outmem, (void *) xlnx_buf->xrt_buf.phy_addr);

    gst_memory_unref (outmem);
  }
#endif

  iret = xclSyncBO (priv->xcl_handle, priv->dec_out_bufs_handle->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dec_out_bufs_handle->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  priv->outbufs_allocated = TRUE;
  printf(" >>>>>>>>>>>>>>>> Returning %s : %d >>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);

  return TRUE;

error:
  printf(" >>>>>>>>>>>>>>>> Returning False %s : %d >>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  return FALSE;
}
#endif

static void
ivas_xvcudec_free_output_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;

  if (priv->out_bufs_arr)
    free (priv->out_bufs_arr);

  if (priv->dec_out_bufs_handle) {
    free_xrt_buffer (priv->xcl_handle, priv->dec_out_bufs_handle);
    free (priv->dec_out_bufs_handle);
  }
}

static boolean
gstivas_xvcudec_open (XrtVideoDecoder * decoder)
{
  XrtIvas_XVCUDec *dec = GST_IVAS_XVCUDEC (decoder);
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int cu_index = 0;
  unsigned dev_index = dec->dev_index;
  boolean bret = FALSE;

  printf("opening  priv->deinit_done %d\n", priv->deinit_done);

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  if (download_xclbin (dec->xclbin_path, dev_index, NULL, &(priv->xcl_handle),
          &(priv->xclbinId))) {
    printf("failed to download xclbin %s", dec->xclbin_path);
    return FALSE;
  }

  if (xclOpenContext (priv->xcl_handle, priv->xclbinId, cu_index, true)) {
    printf("failed to do xclOpenContext...");
    return FALSE;
  }

  printf("Initialization of XRT is successfully. xrt handle = %p\n", priv->xcl_handle);

  bret = ivas_xvcudec_allocate_internal_buffers (dec);
  if (bret == FALSE) {
    printf("failed to allocate internal buffers\n");
    return FALSE;
  }

  return TRUE;
}

static boolean
gstivas_xvcudec_close (XrtVideoDecoder * decoder)
{
  XrtIvas_XVCUDec *dec = GST_IVAS_XVCUDEC (decoder);
  XrtIvas_XVCUDecPrivate *priv = dec->priv;

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  printf("closing");

  /* free all output buffers allocated */
  ivas_xvcudec_free_output_buffers (dec);

  /* free all internal buffers */
  ivas_xvcudec_free_internal_buffers (dec);

  xclCloseContext (priv->xcl_handle, priv->xclbinId, 0);
  xclClose (priv->xcl_handle);

#if 0
#ifdef ENABLE_XRM_SUPPORT
  if (priv->resource_inuse == TRUE) {
    boolean ret;
    int result;
    ret = xrmCuListRelease (priv->xrm_ctx, &priv->dec_cu_list_res);

    if (ret == FALSE)
      printf("DECODER: Decoder Close:xrmCuListRelease Failed");

    priv->resource_inuse = FALSE;

    result = xrmDestroyContext (priv->xrm_ctx);

    if (result)
      printf("DECODER: Close: xrmDestroy Failed");
  }

  GST_INFO_OBJECT (dec, "DECODER: Device Closed Successfully");
#endif
#endif

  return TRUE;
}

static boolean
ivas_xvcudec_preinit (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  dec_params_t *dec_cfg;
  const gchar *mimetype;
  //const GstStructure *structure;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  int iret = 0;
  boolean bret = FALSE;
  //GstVideoInfo vinfo;
  //const gchar *chroma_format;
  guint bit_depth_luma, bit_depth_chroma;

  printf("%s priv->deinit_done %d\n", __func__, priv->deinit_done);
  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PREINIT;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  /* update decoder config params */
  dec_cfg = (dec_params_t *) (priv->dec_cfg_buf->user_ptr);
  memset (dec_cfg, 0, priv->dec_cfg_buf->size);

#if 0
  structure = gst_caps_get_structure (dec->input_state->caps, 0);
  mimetype = gst_structure_get_name (structure);

  if (!strcmp (mimetype, "video/x-h264")) {
    dec_cfg->codec_type = 0;
    GST_INFO_OBJECT (dec, "input stream is H264");
  } else {
    dec_cfg->codec_type = 1;
    GST_INFO_OBJECT (dec, "input stream is H265");
  }

  dec_cfg->bitdepth = dec->bit_depth;
  dec_cfg->low_latency = dec->low_latency;
  dec_cfg->entropy_buffers_count = dec->num_entropy_bufs;

  if (!dec->input_state || !dec->input_state->caps) {
    printf("Frame resolution not available. Exiting");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&vinfo, dec->input_state->caps)) {
    printf("failed to get video info from caps");
    return FALSE;
  }

  dec_cfg->frame_rate = GST_VIDEO_INFO_FPS_N (&vinfo);
  dec_cfg->clk_ratio = GST_VIDEO_INFO_FPS_D (&vinfo);
  dec_cfg->width = GST_VIDEO_INFO_WIDTH (&vinfo);
  dec_cfg->height = GST_VIDEO_INFO_HEIGHT (&vinfo);
  dec_cfg->level =
      get_level_value (gst_structure_get_string (structure, "level"));
  dec_cfg->profile =
      get_profile_value (gst_structure_get_string (structure, "profile"));
  dec_cfg->scan_type = 1;       // progressive

  chroma_format = gst_structure_get_string (structure, "chroma-format");
  if (structure
      && gst_structure_get_uint (structure, "bit-depth-luma", &bit_depth_luma)
      && gst_structure_get_uint (structure, "bit-depth-chroma",
          &bit_depth_chroma)) {

    dec_cfg->chroma_mode =
        get_color_format_from_chroma (chroma_format, bit_depth_luma,
        bit_depth_chroma);
  }

  GST_INFO_OBJECT (dec, "frame rate:%d  clock rate:%d", dec_cfg->frame_rate,
      dec_cfg->clk_ratio);

  // Tmp hack
  if (dec_cfg->frame_rate == 0) {
    dec_cfg->frame_rate = 30;
    GST_INFO_OBJECT (dec,
        "DECODER: Frame rate not received, assuming it to be 30 fps");
  }

  if (dec_cfg->clk_ratio == 0)
    dec_cfg->clk_ratio = 1;
#endif

  dec_cfg->codec_type = 0;
  dec_cfg->bitdepth = 8;
  dec_cfg->low_latency = 0;
  dec_cfg->entropy_buffers_count = 2;
  dec_cfg->frame_rate = 30;
  dec_cfg->clk_ratio = 1;
  dec_cfg->width = 1920;
  dec_cfg->height = 1080;
  dec_cfg->level = 40;
  dec_cfg->profile =100;
  dec_cfg->scan_type = 1;
  dec_cfg->chroma_mode = 420;

  printf("Video Info : frame_rate %d clk_ratio %d width %d height %d level %d profile %d chroma_mode %d\n", 
		  dec_cfg->frame_rate, dec_cfg->clk_ratio, dec_cfg->width, dec_cfg->height, dec_cfg->level, dec_cfg->profile, dec_cfg->chroma_mode);
  iret =
      xclSyncBO (priv->xcl_handle, priv->dec_cfg_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dec_cfg_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;
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
    printf("failed to send VCU_PREINIT command to softkernel - %d, reason : %s\n",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      printf("softkernel pre-initialization failed\n");
      goto error;
    }
  }

  priv->num_out_bufs = payload_buf->obuff_num;
  priv->out_buf_size = payload_buf->obuff_size;

  printf("min output buffers required by softkernel %d and outbuf size %lu\n",
      priv->num_out_bufs, priv->out_buf_size);

  return TRUE;

error:
  return FALSE;
}

static boolean
ivas_xvcudec_init (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  //GstVideoInfo vinfo;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  int iret = 0, i;
  boolean bret = FALSE;


  printf("%s priv->deinit_done %d\n", __func__, priv->deinit_done);
  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* TODO SAIF : Adding this call here for now */
  if (ivas_vcu_dec_outbuffer_alloc_and_map (dec) != TRUE) {
    printf("failed to allocate & map output buffers\n");
    return FALSE;
  }
  printf("\n\t\t\t ################################# %s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, 
			dec->priv->out_bufs_arr[0], dec->priv->out_bufs_arr[0]->phy_addr, dec->priv->out_bufs_arr[0]->bo);

#if 0
  if (!gst_video_info_from_caps (&vinfo, dec->input_state->caps)) {
    printf("failed to get video info from caps");
    return FALSE;
  }

  priv->max_ibuf_size = GST_VIDEO_INFO_WIDTH (&vinfo) *
      GST_VIDEO_INFO_HEIGHT (&vinfo);
#endif
  priv->max_ibuf_size = 0x1FA400;
  printf(">>>>>>>>>>>>>>> %s : %d max_ibuf_size %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__, priv->max_ibuf_size);

  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  payload_buf->cmd_id = VCU_INIT;
  payload_buf->obuff_num = priv->num_out_bufs;

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));

  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);
  printf(">>>>>>>>>>>>>>> %s : %d Physical addr %lx payload_buf %p >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__, (uint64_t)priv->sk_payload_buf->phy_addr, (void *)payload_buf);

  printf("%s : sk_payload_buf->phy_addr %p\n", __func__, (void *)priv->sk_payload_buf->phy_addr);
  for (i = 0; i < MAX_IBUFFS; i++) {
    /* allocate input buffer */
    iret = alloc_xrt_buffer (priv->xcl_handle, priv->max_ibuf_size,
        XCL_BO_DEVICE_RAM, MEM_BANK, priv->in_xrt_bufs[i]);
    if (iret < 0) {
      printf("failed to allocate input buffer..");
      goto error;
    }

    payload_data[num_idx++] = priv->in_xrt_bufs[i]->phy_addr & 0xFFFFFFFF;
    payload_data[num_idx++] =
        ((uint64_t) (priv->in_xrt_bufs[i]->phy_addr) >> 32) & 0xFFFFFFFF;
    payload_data[num_idx++] = priv->in_xrt_bufs[i]->size;
    printf(">>>>>>>> %d >>>>>>> %s : %d payload_data phy_addr %lx >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", i,  __func__, __LINE__, (uint64_t)priv->in_xrt_bufs[i]->phy_addr);
  }

  payload_data[num_idx++] = priv->dec_out_bufs_handle->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->dec_out_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->dec_out_bufs_handle->size;

  printf(">>>>>>>> >>>>>>> %s : %d dec_out_bufs_handle phy_addr %lx >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__, (uint64_t)priv->dec_out_bufs_handle->phy_addr);
    for (i=0; i<num_idx; i++)
          printf("%lx \t", payload_data[i]);

  printf("\n");

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    printf("failed to send VCU_INIT command to softkernel - %d, reason : %s\n",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      printf("softkernel initialization failed\n");
      goto error;
    }
  }

  priv->init_done = TRUE;
  return TRUE;

error:
  return FALSE;
}

/* TODO AGAIN */
static XrtFlowReturn
ivas_xvcudec_read_out_buffer (XrtIvas_XVCUDec * dec, uint32_t idx, void *out_buffer)
{
  int rc = 0;
  //GstVideoCodecFrame *frame = NULL;
  XrtFlowReturn fret = XRT_FLOW_ERROR;
  //XrtMemory *outmem = NULL;

  if (idx == 0xBAD) {
    printf("bad output index received...\n");
    return XRT_FLOW_ERROR;
  }

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  printf("reading output buffer at index %d\n", idx);

#if 0
  frame = gst_video_decoder_get_oldest_frame (GST_VIDEO_DECODER (dec));
  if (!frame) {
    /* Can only happen in finish() */
    printf("no input frames available...returning EOS");
    return XRT_FLOW_EOS;
  }

  printf(">>>>>>>>>>>>>>> %s : %d Frame %p>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__, (void *)frame);
  frame->pts = -1;
#endif

#if 0
  if (dec->priv->need_copy) {
    printf(">>>>>>>>>>>>>>> need_copy  %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
    GstBuffer *new_outbuf, *outbuf;
    GstVideoFrame new_frame, out_frame;
    GstMemory *outmem;

    outbuf = dec->priv->out_bufs_arr[idx];
    outmem = gst_buffer_get_memory (outbuf, 0);
    /* when plugins/app request to map this memory, sync will occur */
    gst_ivas_memory_set_sync_flag (outmem, IVAS_SYNC_FROM_DEVICE);
    gst_memory_unref (outmem);

    new_outbuf =
        gst_buffer_new_and_alloc (GST_VIDEO_INFO_SIZE (&dec->out_vinfo));
    if (!new_outbuf) {
      printf("failed to allocate output buffer\n");
      return XRT_FLOW_ERROR;
    }

    gst_video_frame_map (&out_frame, &dec->out_vinfo, outbuf, GST_MAP_READ);
    gst_video_frame_map (&new_frame, &dec->out_vinfo, new_outbuf,
        GST_MAP_WRITE);
    printf("slow copy data from %p to %p of size %d\n", outbuf, new_outbuf, sizeof(new_outbuf));
    gst_video_frame_copy (&new_frame, &out_frame);
    gst_video_frame_unmap (&out_frame);
    gst_video_frame_unmap (&new_frame);

    gst_buffer_copy_into (new_outbuf, outbuf, GST_BUFFER_COPY_FLAGS, 0, -1);
    gst_buffer_unref (outbuf);

    frame->output_buffer = new_outbuf;
  } else {
    printf(">>>>>>>>>>>>>>> Else need_copy %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
    frame->output_buffer = dec->priv->out_bufs_arr[idx];
    outmem = gst_buffer_get_memory (frame->output_buffer, 0);
    printf(">>>>>>>>>>>>>>> Else need_copy %s : %d size %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__, outmem->size);

    /* when plugins/app request to map this memory, sync will occur */
    gst_ivas_memory_set_sync_flag (outmem, IVAS_SYNC_FROM_DEVICE);
    gst_memory_unref (outmem);
  }

  g_hash_table_remove (dec->priv->out_buf_hash, GINT_TO_POINTER (idx));

  GST_LOG_OBJECT (dec, "processing index %d buffer %" GST_PTR_FORMAT, idx,
      frame->output_buffer);

  printf("processing index %d buffer %p of size %d \n" GST_PTR_FORMAT, idx,
      frame->output_buffer, sizeof(*frame->output_buffer));
  fret = gst_video_decoder_finish_frame (GST_VIDEO_DECODER (dec), frame);
  if (fret != XRT_FLOW_OK) {
    printf("failed to push frame. reason %s\n",
        gst_flow_get_name (fret));
    frame = NULL;
    goto error;
  }
#endif

  xrt_buffer *out_buf =  dec->priv->out_bufs_arr[idx];
  printf("%s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, dec->priv->out_bufs_arr[idx], dec->priv->out_bufs_arr[idx]->phy_addr, dec->priv->out_bufs_arr[idx]->bo);
  printf("out_buf %p, phy_addr %lx , Size %ld, bo %d, out_bufs_arr %p\n", out_buf, out_buf->phy_addr, out_buf->size, out_buf->bo, dec->priv->out_bufs_arr[idx]);
  /* transfer input frame contents to device */

  printf("bo %d \n", out_buf->bo);
  rc = xclSyncBO (dec->priv->xcl_handle,
		  out_buf->bo,
		  XCL_BO_SYNC_BO_FROM_DEVICE, 3342336, 0);
  if (rc != 0) {
	  printf("xclSyncBO failed %d\n", rc);
	  return fret;
  }

  rc = xclReadBO(dec->priv->xcl_handle, out_buf->bo, out_buffer, 3342336, 0);
  if (rc != 0) {
	  printf("xclReadBO failed %d\n", rc);
	  return fret;
  }

  hexDump_1 ("out buffer", out_buffer, 1000);

  return fret;

error:
  //if (frame)
  //  gst_video_codec_frame_unref (frame);
  return fret;
}

static boolean
ivas_xvcudec_send_flush (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  boolean bret = FALSE;
  int iret = 0;
  unsigned int num_idx = 0;

  if (priv->flush_done == TRUE) {
    printf("flush already issued to softkernel, hence returning\n");
    return TRUE;
  }
  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_FLUSH;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    printf("failed to send VCU_FLUSH command to softkernel - %d", iret);
    goto error;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      printf("softkernel flush failed");
      goto error;
    }
  }
  printf("successfully sent flush command\n");
  priv->flush_done = TRUE;
  return TRUE;

error:
  return FALSE;
}

static boolean
ivas_xvcudec_deinit (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  int iret = 0;

  printf("%s priv->deinit_done %d\n", __func__, priv->deinit_done);
  if (priv->deinit_done == TRUE) {
    printf("deinit already issued to softkernel, hence returning");
    return TRUE;
  }

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_DEINIT;
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    goto error;
  }
  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  priv->deinit_done = TRUE;     // irrespective of error

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    printf("failed to send VCU_DEINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  }

  printf("Successfully deinitialized softkernel");
  return TRUE;

error:
  return FALSE;
}

static boolean
gstivas_xvcudec_stop (XrtVideoDecoder * decoder)
{
  XrtIvas_XVCUDec *dec = GST_IVAS_XVCUDEC (decoder);
  boolean bret = TRUE;

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  printf("%s priv->deinit_done %d\n", __func__, dec->priv->deinit_done);

  if (dec->priv->init_done) {
    bret = ivas_xvcudec_send_flush (GST_IVAS_XVCUDEC (decoder));
    if (bret != TRUE)
      return bret;

    bret = ivas_xvcudec_deinit (GST_IVAS_XVCUDEC (decoder));
    dec->priv->init_done = FALSE;
  }

#if 0
  if (dec->priv->mem_released_handler > 0) {
    g_signal_handler_disconnect (dec->priv->allocator,
        dec->priv->mem_released_handler);
    dec->priv->mem_released_handler = 0;
  }

  gst_clear_object (&dec->priv->allocator);
  gst_clear_object (&dec->priv->pool);
#endif

  return bret;
}

static boolean
ivas_xvcudec_prepare_send_frame (XrtIvas_XVCUDec * dec, void *inbuf,
    size_t insize, uint32_t *payload_data, uint32_t *payload_num_idx)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  int iret = 0, i;
  uint32_t num_idx = 0;
  struct timespec tms;
  /* POSIX.1-2008 way */
  if (clock_gettime(CLOCK_REALTIME,&tms)) {
	  return -1;
  }

  printf("sending input buffer index %d with size %lu",
      priv->host_to_dev_ibuf_idx, insize);

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PUSH;
  payload_buf->ibuff_valid_size = insize;
  payload_buf->ibuff_meta.pts = tms.tv_sec * 1000000; 
  payload_buf->host_to_dev_ibuf_idx = priv->host_to_dev_ibuf_idx;

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;
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

  printf("sending VCU_PUSH command to softkernel\n");

  /* reset all free out buf indexes */
  for (i = 0; i < MAX_OUT_INFOS; i++)
    payload_buf->obuf_info[i].freed_obuf_index = 0xBAD;

#if 0
  //g_mutex_lock (&priv->obuf_lock);

  payload_buf->valid_oidxs = g_list_length (priv->free_oidx_list);
  for (i = 0; i < payload_buf->valid_oidxs; i++) {
    XlnxOutputBuffer *xlnxbuf = g_list_first (priv->free_oidx_list)->data;
    XlnxOutputBuffer *tmpbuf = NULL;
    GstBuffer *outbuf = NULL;
    GstFlowReturn fret = GST_FLOW_OK;
    GstMemory *outmem = NULL;

    fret = gst_buffer_pool_acquire_buffer (priv->pool, &outbuf, NULL);
    if (fret != GST_FLOW_OK) {
      printf"failed to acquire buffer from pool %p",
          priv->pool);
      g_mutex_unlock (&priv->obuf_lock);
      goto error;
    }

    outmem = gst_buffer_get_memory (outbuf, 0);
    if (!g_hash_table_contains (dec->priv->oidx_hash, outmem)) {
      printf("new output memory received %p", outmem);
      printf(("unexpected behaviour: new output memory received %p", outmem));
      gst_memory_unref (outmem);
      g_mutex_unlock (&priv->obuf_lock);
      goto error;
    }

    tmpbuf = g_hash_table_lookup (dec->priv->oidx_hash, outmem);
    priv->out_bufs_arr[tmpbuf->idx] = outbuf;
    printf("filling addr %p free out index %d in SEND command",
        (void *) tmpbuf->xrt_buf.phy_addr, tmpbuf->idx);

    payload_buf->obuf_info[i].freed_obuf_index = tmpbuf->idx;
    payload_buf->obuf_info[i].freed_obuf_paddr = tmpbuf->xrt_buf.phy_addr;
    payload_buf->obuf_info[i].freed_obuf_size = tmpbuf->xrt_buf.size;
    priv->free_oidx_list = g_list_remove (priv->free_oidx_list, xlnxbuf);
    gst_memory_unref (outmem);
    g_hash_table_insert (priv->out_buf_hash, GINT_TO_POINTER (tmpbuf->idx),
        outbuf);
  }

  /* Making NULL as we consumed all indexes */
  priv->free_oidx_list = NULL;
  g_mutex_unlock (&priv->obuf_lock);
#endif

  /* transfer payload settings to device */
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s\n", iret,
        strerror (errno));
    printf("failed to sync PUSH command payload to device. reason : %s\n",
            strerror (errno));
    goto error;
  }

  *payload_num_idx = num_idx;

  return TRUE;

error:
  return FALSE;
}

static XrtFlowReturn
ivas_xvcudec_receive_out_frames (XrtIvas_XVCUDec * dec, void *out_buffer)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  //GstVideoCodecState *out_state = NULL;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  XrtFlowReturn fret = XRT_FLOW_OK;
  int iret = 0;
  boolean bret = FALSE;

#if 0
  if (priv->last_rcvd_payload.free_index_cnt) {
    out_state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (dec));

    GST_LOG_OBJECT (dec, "receiving cached output frames count %d",
        priv->last_rcvd_payload.free_index_cnt);
    fret =
        ivas_xvcudec_read_out_buffer (dec,
        priv->last_rcvd_payload.obuff_index[priv->last_rcvd_oidx], out_state);
    if (fret != GST_FLOW_OK)
      goto exit;

    priv->last_rcvd_payload.free_index_cnt--;
    priv->last_rcvd_oidx++;
    gst_video_codec_state_unref (out_state);
    return fret;
  }
#endif
  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_RECEIVE;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    fret = XRT_FLOW_ERROR;
    goto exit;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));
  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

  printf("sending VCU_RECEIVE command to softkernel\n");
  /* send command to softkernel */
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    printf("failed to send VCU_RECEIVE command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    fret = XRT_FLOW_ERROR;
    goto exit;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      printf("softkernel receive frame failed");
      fret = XRT_FLOW_ERROR;
      goto exit;
    }
  }

  printf("successfully completed VCU_RECEIVE command\n");

#if 0
  // TODO: get width and height from softkernel
  if (!gst_pad_has_current_caps (GST_VIDEO_DECODER_SRC_PAD (dec))) {
    GstVideoInfo vinfo;
    GstCaps *outcaps = NULL;
    // TODO: add check for resolution change

    // HACK: taking input resolution and setting instead of taking from softkernel output
    if (!gst_video_info_from_caps (&vinfo, dec->input_state->caps)) {
      printf("failed to get video info from caps");
      fret = GST_FLOW_ERROR;
      goto exit;
    }

    out_state =
        gst_video_decoder_set_output_state (GST_VIDEO_DECODER (dec),
        GST_VIDEO_FORMAT_NV12, GST_VIDEO_INFO_WIDTH (&vinfo),
        GST_VIDEO_INFO_HEIGHT (&vinfo), dec->input_state);

    if (!gst_video_decoder_negotiate (GST_VIDEO_DECODER (dec))) {
      printf("Failed to negotiate with downstream elements");
      gst_video_codec_state_unref (out_state);
      return GST_FLOW_NOT_NEGOTIATED;
    }

    outcaps = gst_pad_get_current_caps (GST_VIDEO_DECODER_SRC_PAD (dec));

    if (!gst_video_info_from_caps (&dec->out_vinfo, outcaps)) {
      printf("failed to get out video info from caps");
      fret = GST_FLOW_ERROR;
      goto exit;
    }
    GST_INFO_OBJECT (dec, "negotiated caps on source pad : %" GST_PTR_FORMAT,
        outcaps);
  } else {
    out_state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (dec));
  }

#endif
  printf("number of available output buffers %d for consumption\n",
      payload_buf->free_index_cnt);

  memcpy (&priv->last_rcvd_payload, payload_buf, sizeof (sk_payload_data));

  if (priv->last_rcvd_payload.free_index_cnt) {
    priv->last_rcvd_oidx = 0;
    fret =
        ivas_xvcudec_read_out_buffer (dec,
        priv->last_rcvd_payload.obuff_index[priv->last_rcvd_oidx], out_buffer);
    if (fret != XRT_FLOW_OK)
      goto exit;
    priv->last_rcvd_payload.free_index_cnt--;
    priv->last_rcvd_oidx++;
  } else if (payload_buf->end_decoding) {
    printf("EOS recevied from softkernel\n");
    fret = XRT_FLOW_EOS;
    goto exit;
  }

  printf("softkernel receive successful fret %d\n", fret);

exit:
  /*
  if (out_state)
    gst_video_codec_state_unref (out_state);
  */
  return fret;
}

static XrtFlowReturn
gstivas_xvcudec_handle_frame (XrtVideoDecoder * decoder,
    uint8_t *indata, size_t insize)
{
  XrtIvas_XVCUDec *dec = (XrtIvas_XVCUDec *)decoder->dec;
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  XrtFlowReturn fret = XRT_FLOW_OK;
  sk_payload_data *payload_buf = NULL;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  uint32_t num_idx = 0;
  int iret = 0;
  boolean send_again = FALSE;
  boolean bret = TRUE;
  void *inbuf = NULL;

  hexDump_1 ("input_buffer", indata, 1000);
  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  if (indata) {
    /* copy input frame to xrt memory */
    iret = xclWriteBO (priv->xcl_handle,
        priv->in_xrt_bufs[priv->host_to_dev_ibuf_idx]->bo, indata, insize, 0);
    if (iret != 0) {
      printf("failed to write input frame to xrt memory. "
          "reason : %s\n", strerror (errno));
      fret = XRT_FLOW_ERROR;
      return fret;
    }

    printf("host_to_dev_ibuf_idx %d\n", priv->host_to_dev_ibuf_idx);
    /* transfer input frame contents to device */
    iret = xclSyncBO (priv->xcl_handle,
        priv->in_xrt_bufs[priv->host_to_dev_ibuf_idx]->bo,
        XCL_BO_SYNC_BO_TO_DEVICE, insize, 0);
    if (iret != 0) {
      printf("failed to sync input frame. reason : %s\n",
          strerror (errno));
      fret = XRT_FLOW_ERROR;
      return fret;
    }

    //gst_buffer_unmap (frame->input_buffer, &map_info);
  } else {
    printf("^^^^^^^^^^^^^^^^^^^^^ Am I coming here ^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    printf("no input frames available...returning EOS\n");
  }

  //inbuf = gst_buffer_ref (frame->input_buffer);
  inbuf = (void *)indata;
  printf("Frame inbuffer : %p\n", (void *)inbuf);

  //gst_video_codec_frame_unref (frame);
  //frame = NULL;

//try_again:
  bret = ivas_xvcudec_prepare_send_frame (dec, inbuf, insize, payload_data,
      &num_idx);
  if (bret != TRUE) {
    printf("failed to prepare send frame command\n");
    fret = XRT_FLOW_ERROR;
    return fret;
  }

  send_again = FALSE;
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    printf("failed to send VCU_PUSH command to softkernel - %d, " "reason : %s",
        iret, strerror (errno));
    fret = XRT_FLOW_ERROR;
    return fret;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      printf("softkernel send frame failed\n");
      fret = XRT_FLOW_ERROR;
      return fret;
    }
    printf("successfully completed VCU_PUSH command\n");
  }

  if (payload_buf->dev_to_host_ibuf_idx != 0xBAD) {
    priv->host_to_dev_ibuf_idx = payload_buf->dev_to_host_ibuf_idx;
    printf("input buffer index %d consumed\n",
        priv->host_to_dev_ibuf_idx);
  } else {
    printf("input buffer index %d not consumed, try again...",
        priv->host_to_dev_ibuf_idx);
    send_again = TRUE;
  }

  char out_buffer[3342336] = { 0 };
  fret = ivas_xvcudec_receive_out_frames (dec, &out_buffer);
  if (fret != XRT_FLOW_OK) {
    return fret;
  }

#if 0
  if (send_again) {
    guint num_free_obuf = 0;

    g_mutex_lock (&dec->priv->obuf_lock);
    num_free_obuf = g_list_length (dec->priv->free_oidx_list);

    if (num_free_obuf) {
      /* send again may get success when free outbufs available */
      GST_LOG_OBJECT (dec, "send free output buffers %d back to decoder",
          num_free_obuf);
      g_mutex_unlock (&dec->priv->obuf_lock);
      goto try_again;
    } else {
      gint64 end_time =
          g_get_monotonic_time () + IVAS_DEC_RETRY_TIMEOUT_IN_MSEC;
      if (!g_cond_wait_until (&priv->obuf_cond, &priv->obuf_lock, end_time)) {
        GST_LOG_OBJECT (dec, "timeout occured, try PUSH command again");
      } else {
        GST_LOG_OBJECT (dec, "received free output buffer(s), "
            "send PUSH command again");
      }
      g_mutex_unlock (&dec->priv->obuf_lock);
      goto try_again;
    }
  }
#endif
  printf("<<<<<<<<<<<<<< Returning %s : %d <<<<<<<<<<<<<<<<<<<<<<<<<\n", __func__, __LINE__);

  return XRT_FLOW_OK;

#if 0
  if (inbuf)
    gst_buffer_unref (inbuf);

  if (frame)
    gst_video_codec_frame_unref (frame);
#endif

}

static void
gstivas_xvcudec_init (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = GST_IVAS_XVCUDEC_PRIVATE (dec);
  dec->priv = priv;
  dec->bit_depth = 8;
  dec->num_entropy_bufs = 2;
  dec->sk_start_idx = -1;
  dec->dev_index = 0;           /* Default device Index */
  dec->sk_cur_idx = 0; 		/* SAIF Hard coding SK Index */

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
#if 0
  priv->oidx_hash =
      g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free);
  priv->out_buf_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
#endif
  priv->out_bufs_arr = NULL;
  printf("out_bufs_arr : %s : %d\n", __func__, __LINE__);
  //priv->pool = NULL;
  priv->outbufs_allocated = FALSE;
  //priv->free_oidx_list = NULL;
  priv->init_done = FALSE;
  priv->flush_done = FALSE;
  priv->max_ibuf_size = 0;
  priv->host_to_dev_ibuf_idx = 0;
  priv->deinit_done = FALSE;
  //priv->need_copy = TRUE;
#if 0
  g_mutex_init (&priv->obuf_lock);
  g_cond_init (&priv->obuf_cond);
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (dec), TRUE);
  gst_video_decoder_set_needs_format (GST_VIDEO_DECODER (dec), TRUE);
#ifdef ENABLE_XRM_SUPPORT
  priv->reservation_id = 0;
  priv->xrm_ctx = NULL;
  dec->sk_start_idx = -1;
#endif
#endif
  printf("<<<<<<<<<<<<<<<<<<<<<<<<< Retruning %s : %d <<<<<<<<<<<<<<<<<<<<<<<<<\n", __func__, __LINE__);
}

void hexDump_1 (const char * desc, const void * addr, const int len) {
    int i;
    unsigned char buff[17];
    const unsigned char * pc = (const unsigned char *)addr;

    // Output description if given.

    if (desc != NULL)
        printf ("%s:\n", desc);

    // Length checks.

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    else if (len < 0) {
        printf("  NEGATIVE LENGTH: %d\n", len);
        return;
    }

    // Process every byte in the data.

    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Don't print ASCII buffer for the "zeroth" line.

            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.

            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And buffer a printable ASCII character for later.

        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.

    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII buffer.

    printf ("  %s\n", buff);
}

static XrtFlowReturn
gstivas_xvcudec_finish (XrtVideoDecoder * decoder)
{
  XrtIvas_XVCUDec *dec = (XrtIvas_XVCUDec *)decoder->dec;
  XrtFlowReturn fret = XRT_FLOW_OK;
  boolean bret = FALSE;
  uint32_t len = 0;
  XrtIvas_XVCUDecPrivate *priv = dec->priv;

  char out_buffer[MEM_MAX_SIZE] = { 0 };

    printf("%s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, dec->priv->out_bufs_arr[0], dec->priv->out_bufs_arr[0]->phy_addr, dec->priv->out_bufs_arr[0]->bo);
  printf(">>>>>>>>>>>>>>> %s : %d init_done %d>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__, dec->priv->init_done);
  if (dec->priv->init_done != TRUE)
    return XRT_FLOW_OK;

  // TODO: add support when decoder not negotiated
  bret = ivas_xvcudec_send_flush (dec);
  if (bret != TRUE) {
    printf(">>>>>>>>>>>>>>> goto error  %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
    goto error;
  }

  printf(">>>>>>>>>>>>>>> After Flash  %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  do {
#if 0
    g_mutex_lock (&dec->priv->obuf_lock);
    len = g_list_length (dec->priv->free_oidx_list);
    g_mutex_unlock (&dec->priv->obuf_lock);

    if (len) {
      GstVideoCodecFrame *frame =
          gst_video_decoder_get_oldest_frame (GST_VIDEO_DECODER (dec));
      unsigned int payload_data[ERT_CMD_DATA_LEN];
      sk_payload_data *payload_buf = NULL;
      guint num_idx = 0;
      gint iret = 0;

      if (!frame) {
        GST_WARNING_OBJECT (dec, "failed to get frame");
        break;
      }
      uint32_t num_idx = 0;
      int32_t iret = 0;
      unsigned int payload_data[ERT_CMD_DATA_LEN];

      bret =
          ivas_xvcudec_prepare_send_frame (dec, frame->input_buffer, 0,
          payload_data, &num_idx);
      if (!bret)
        break;

      payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);

      iret =
          send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
          payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
      if (iret < 0) {
        printf("failed to send VCU_PUSH command to softkernel - %d, reason : %s", iret, strerror (errno));
        break;
      } else {
        bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
        if (!bret) {
          printf("softkernel send frame failed");
          break;
        }
      }
      printf("successfully completed VCU_PUSH command");
    }
#endif

    printf("%s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, dec->priv->out_bufs_arr[0], dec->priv->out_bufs_arr[0]->phy_addr, dec->priv->out_bufs_arr[0]->bo);
    fret = ivas_xvcudec_receive_out_frames (dec, &out_buffer);
    printf("%s : successfully completed VCU_PUSH command, fret %d\n", __func__, fret);
  } while (fret == XRT_FLOW_OK);

  hexDump_1 ("output", &out_buffer, 1000);
#if 0
  /* release output buffers already sent to device */
  g_hash_table_foreach_remove (dec->priv->out_buf_hash,
      ivas_free_output_hash_value, dec);
#endif

  return fret;

error:
  return XRT_FLOW_ERROR;
}

int load_file(char const* path, char* buffer)
{
    //char* buffer = 0;
    long length = 0;
    FILE * f = fopen (path, "rb"); //was "rb"

    if (f)
    {
      fseek (f, 0, SEEK_END);
      length = ftell (f);
      fseek (f, 0, SEEK_SET);
      buffer = (char*)malloc ((length+1)*sizeof(char));
      if (buffer)
      {
        fread (buffer, sizeof(char), length, f);
      }
      fclose (f);
    }
    buffer[length] = '\0';
    // for (int i = 0; i < length; i++) {
    //     printf("buffer[%d] == %c\n", i, buffer[i]);
    // }
    //printf("buffer = %s\n", buffer);

    return length;
}

int vcu_dec_test()
{
        XrtIvas_XVCUDecPrivate priv;
        XrtIvas_XVCUDec dec;
        XrtVideoDecoder decoder;
        XrtVideoDecoder *decoderPtr = &decoder;
        XrtIvas_XVCUDec *decptr = &dec;
        XrtIvas_XVCUDecPrivate *privptr = &priv;

        char path[100] = XCLBIN_PATH;
	int length  = 0;
	char *buffer = NULL;

        decptr->priv = privptr;
        decoderPtr->dec = decptr;

        decptr->xclbin_path = &path[0];
        decptr->dev_index = 0;


        printf("I am inside vcu_dec_test\n");

        // Start Test
        gstivas_xvcudec_init(decptr);
        gstivas_xvcudec_open(decoderPtr);
        ivas_xvcudec_preinit(decptr);
        ivas_xvcudec_init(decptr);
        printf("\n\t\t\t ################################# %s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, 
			decptr->priv->out_bufs_arr[0], decptr->priv->out_bufs_arr[0]->phy_addr, decptr->priv->out_bufs_arr[0]->bo);

        printf("Initialization is Done\n");

	FILE * f = fopen ("./frame0.dmp", "rb");
	if (f)
    	{
      		fseek (f, 0, SEEK_END);
      		length = ftell (f);
      		fseek (f, 0, SEEK_SET);
      		buffer = (char*)malloc ((length+1)*sizeof(char));
  		if (buffer)
      		{
        		fread (buffer, sizeof(char), length, f);
      		}
      		fclose (f);
    	}
    	buffer[length] = '\0';

        printf("\n\t\t\t ################################# %s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, 
			decptr->priv->out_bufs_arr[0], decptr->priv->out_bufs_arr[0]->phy_addr, decptr->priv->out_bufs_arr[0]->bo);
	gstivas_xvcudec_handle_frame(decoderPtr, (uint8_t *)buffer, length); 

        printf("\n\t\t\t ################################# %s : %d -> out_bufs_arr %p, phy %lx, bo %d\n", __func__, __LINE__, 
			decptr->priv->out_bufs_arr[0], decptr->priv->out_bufs_arr[0]->phy_addr, decptr->priv->out_bufs_arr[0]->bo);
	gstivas_xvcudec_finish(decoderPtr);


        gstivas_xvcudec_stop(decoderPtr);
        ivas_xvcudec_deinit(decptr);
        gstivas_xvcudec_close(decoderPtr);
        printf("Test is Done\n");

        return 0;
}
