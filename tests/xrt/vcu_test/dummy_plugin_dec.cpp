#include <stdio.h>
#include "dummy_plugin_dec.h"
#include "xrt_utils.h"

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

  /* allocate softkernel payload buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (sk_payload_data),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->sk_payload_buf);
  if (iret < 0) {
    printf("failed to allocate softkernel payload buffer..");
    goto error;
  }

  /* allocate decoder config buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (dec_params_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->dec_cfg_buf);
  if (iret < 0) {
    printf("failed to allocate decoder config buffer..");
    goto error;
  }


  // TODO : Revisit this later
  priv->dec_out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dec_out_bufs_handle == NULL) {
    printf("failed to allocate decoder output buffers structure");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle,
      priv->num_out_bufs * sizeof (uint64_t), XCL_BO_DEVICE_RAM, MEM_BANK,
      priv->dec_out_bufs_handle);
  if (iret < 0) {
    printf("failed to allocate decoder out buffers handle..\n");
    goto error;
  }


  return TRUE;

error:
  return FALSE;
}

static boolean
gstivas_xvcudec_open (XrtVideoDecoder * decoder)
{
  XrtIvas_XVCUDec *dec = (XrtIvas_XVCUDec *)decoder->dec;
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int cu_index = 0;
  unsigned dev_index = dec->dev_index;
  boolean bret = FALSE;

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

  printf("Initialization of XRT is successfully. xrt handle = %p", priv->xcl_handle);

  bret = ivas_xvcudec_allocate_internal_buffers (dec);
  if (bret == FALSE) {
    printf("failed to allocate internal buffers");
    return FALSE;
  }
  
  return TRUE;
}

static boolean
ivas_xvcudec_check_softkernel_response (XrtIvas_XVCUDec * dec,
    sk_payload_data * payload_buf)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  int iret;

  printf("%s : Before SyncBo\n", __func__);
  memset (payload_buf, 0, priv->sk_payload_buf->size);
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret, strerror (errno));
    printf("failed to sync response from softkernel. reason : %s", strerror (errno));
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

static void
gstivas_xvcudec_init (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  dec->priv = priv;
  dec->bit_depth = 8;
  dec->num_entropy_bufs = 2;
  dec->sk_start_idx = -1;
  dec->dev_index = 0;           /* Default device Index */
  dec->sk_cur_idx = 0;

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  //priv->oidx_hash =
  //    g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free);
  //priv->out_buf_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
  //priv->out_bufs_arr = NULL;
  //priv->pool = NULL;
  //priv->outbufs_allocated = FALSE;
  //priv->free_oidx_list = NULL;
  priv->init_done = FALSE;
  priv->flush_done = FALSE;
  priv->max_ibuf_size = 0;
  //priv->host_to_dev_ibuf_idx = 0;
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

static boolean
ivas_xvcudec_preinit (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;
  sk_payload_data *payload_buf;
  dec_params_t *dec_cfg;
  const char *mimetype;
  //const GstStructure *structure;
  unsigned int payload_data[ERT_CMD_DATA_LEN];
  unsigned int num_idx = 0;
  int iret = 0;
  boolean bret = FALSE;
  //GstVideoInfo vinfo;
  const char *chroma_format;
  uint32_t bit_depth_luma, bit_depth_chroma;

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
    printf("failed to sync PREINIT command payload to device. reason : %s",
            strerror (errno));
    goto error;
  }

  /* update decoder config params */
  dec_cfg = (dec_params_t *) (priv->dec_cfg_buf->user_ptr);
  memset (dec_cfg, 0, priv->dec_cfg_buf->size);

  //structure = gst_caps_get_structure (dec->input_state->caps, 0);
  //mimetype = gst_structure_get_name (structure);

  dec_cfg->codec_type = 0;
#if 0
  if (!strcmp (mimetype, "video/x-h264")) {
    dec_cfg->codec_type = 0;
    GST_INFO_OBJECT (dec, "input stream is H264");
  } else {
    dec_cfg->codec_type = 1;
    GST_INFO_OBJECT (dec, "input stream is H265");
  }
#endif

  dec_cfg->bitdepth = dec->bit_depth;
  dec_cfg->low_latency = dec->low_latency;
  dec_cfg->entropy_buffers_count = dec->num_entropy_bufs;

#if 0
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
  dec_cfg->frame_rate = 30;
  dec_cfg->clk_ratio = 1;
  dec_cfg->width = 1920;
  dec_cfg->height = 1080;
  dec_cfg->level = 40;
  dec_cfg->profile =100;
  dec_cfg->scan_type = 1;
  dec_cfg->chroma_mode = 420;
    
  iret =
      xclSyncBO (priv->xcl_handle, priv->dec_cfg_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dec_cfg_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    printf("failed to sync decoder configuration to device. reason : %s",
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
    printf("failed to send VCU_PREINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    printf("********** bret %d \n", bret);
    if (bret != TRUE) {
      printf("********** Inside if bret %d \n", bret);
      printf("softkernel pre-initialization failed\n");
      printf("decoder softkernel pre-initialization failed\n");
      goto error;
    }
  }

  priv->num_out_bufs = payload_buf->obuff_num;
  priv->out_buf_size = payload_buf->obuff_size;

  printf("min output buffers required by softkernel %d and outbuf size %lu",
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

#if 0
  if (!gst_video_info_from_caps (&vinfo, dec->input_state->caps)) {
    printf("failed to get video info from caps");
    return FALSE;
  }

  priv->max_ibuf_size = GST_VIDEO_INFO_WIDTH (&vinfo) *
      GST_VIDEO_INFO_HEIGHT (&vinfo);
#endif

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);
  priv->max_ibuf_size = 0x1FA400;

  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  payload_buf->cmd_id = VCU_INIT;
  payload_buf->obuff_num = priv->num_out_bufs;

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    printf("synbo failed - %d, reason : %s", iret,
        strerror (errno));
    printf("failed to sync INIT command payload to device. reason : %s",
            strerror (errno));
    goto error;
  }

  memset (payload_data, 0, ERT_CMD_DATA_LEN * sizeof (int));

  payload_data[num_idx++] = 0;
  payload_data[num_idx++] = priv->sk_payload_buf->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->sk_payload_buf->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = sizeof (sk_payload_data);

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
  }

  payload_data[num_idx++] = priv->dec_out_bufs_handle->phy_addr & 0xFFFFFFFF;
  payload_data[num_idx++] =
      ((uint64_t) (priv->dec_out_bufs_handle->phy_addr) >> 32) & 0xFFFFFFFF;
  payload_data[num_idx++] = priv->dec_out_bufs_handle->size;

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, dec->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    printf(        "failed to send VCU_INIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    goto error;
  } else {
    bret = ivas_xvcudec_check_softkernel_response (dec, payload_buf);
    if (bret != TRUE) {
      printf("&&&&&&&&&&& softkernel initialization failed");
      printf("decoder softkernel initialization failed");
      goto error;
    }
  }

  priv->init_done = TRUE;
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

  if (priv->deinit_done) {
    printf("deinit already issued to softkernel, hence returning\n");
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
  XrtIvas_XVCUDec *dec = (XrtIvas_XVCUDec *)decoder->dec;
  boolean bret = TRUE;

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);

  if (dec->priv->init_done) {
  //TODO : Need to check this func
#if 0
    bret = ivas_xvcudec_send_flush (GST_IVAS_XVCUDEC (decoder));
    if (!bret)
      return bret;
#endif

    bret = ivas_xvcudec_deinit (dec);
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


static void
ivas_xvcudec_free_output_buffers (XrtIvas_XVCUDec * dec)
{
  XrtIvas_XVCUDecPrivate *priv = dec->priv;

  if (priv->dec_out_bufs_handle) {
    free_xrt_buffer (priv->xcl_handle, priv->dec_out_bufs_handle);
    free (priv->dec_out_bufs_handle);
  }
}

static boolean
gstivas_xvcudec_close (XrtVideoDecoder *decoder)
{
  XrtIvas_XVCUDec *dec = (XrtIvas_XVCUDec *)decoder->dec;
  XrtIvas_XVCUDecPrivate *priv = dec->priv;

  printf(">>>>>>>>>>>>>>> %s : %d >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __func__, __LINE__);

  /* free all output buffers allocated */
  ivas_xvcudec_free_output_buffers (dec);

  /* free all internal buffers */
  ivas_xvcudec_free_internal_buffers (dec);

  xclCloseContext (priv->xcl_handle, priv->xclbinId, 0);
  xclClose (priv->xcl_handle);

#if 0
#ifdef ENABLE_XRM_SUPPORT
  if (priv->resource_inuse == TRUE) {
    gboolean ret;
    int result;
    ret = xrmCuListRelease (priv->xrm_ctx, &priv->dec_cu_list_res);

    if (ret == FALSE)
      GST_ERROR_OBJECT (dec, "DECODER: Decoder Close:xrmCuListRelease Failed");

    priv->resource_inuse = FALSE;

    result = xrmDestroyContext (priv->xrm_ctx);

    if (result)
      GST_ERROR_OBJECT (dec, "DECODER: Close: xrmDestroy Failed");
  }

  GST_INFO_OBJECT (dec, "DECODER: Device Closed Successfully");
#endif
#endif

  return TRUE;
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

	printf("Initialization is Done\n");
	gstivas_xvcudec_stop(decoderPtr);
	ivas_xvcudec_deinit(decptr);
	gstivas_xvcudec_close(decoderPtr);
	printf("Test is Done\n");

	return 0;
}
