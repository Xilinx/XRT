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

#if 0
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#endif

#include "plugin_enc.h"
#include "xrt_utils.h"

#if 0
#ifdef ENABLE_XRM_SUPPORT
#include <xrm.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_ivas_xvcuenc_debug_category);
#define GST_CAT_DEFAULT gst_ivas_xvcuenc_debug_category
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

#define GST_CAPS_FEATURE_MEMORY_XLNX "memory:XRT"
#endif

static const int ERT_CMD_SIZE = 4096;
#define CMD_EXEC_TIMEOUT 1000   // 1 sec
#define MIN_POOL_BUFFERS 1

#define MEM_BANK 0
#define ENABLE_DMABUF 0
#define WIDTH_ALIGN 256
#define HEIGHT_ALIGN 64
#define ALIGN(size,align) (((size) + (align) - 1) & ~((align) - 1))
//////////////// FROM VCU XMA Plugin START //////////////////////
#define MAX_OUT_BUFF_COUNT 50
#define MIN_LOOKAHEAD_DEPTH (1)
#define MAX_LOOKAHEAD_DEPTH (30)

enum cmd_type
{
  VCU_PREINIT = 0,
  VCU_INIT,
  VCU_PUSH,
  VCU_RECEIVE,
  VCU_FLUSH,
  VCU_DEINIT,
};

enum rc_mode {
  AL_RC_CONST_QP = 0x00,
  AL_RC_CBR = 0x01,
  AL_RC_VBR = 0x02,
  AL_RC_LOW_LATENCY = 0x03,
  AL_RC_CAPPED_VBR = 0x04,
  AL_RC_BYPASS = 0x3F,
  AL_RC_PLUGIN = 0x40,
  AL_RC_MAX_ENUM,
};

typedef struct enc_dynamic_params
{
  uint16_t width;
  uint16_t height;
  double framerate;
  uint16_t rc_mode;
} enc_dynamic_params_t;

typedef struct _vcu_enc_usermeta
{
  int64_t pts;
  int frame_type;
} vcu_enc_usermeta;

typedef struct __obuf_info
{
  uint32_t obuff_index;
  uint32_t recv_size;
  vcu_enc_usermeta obuf_meta;
} obuf_info;

typedef struct xlnx_rc_fsfa
{
  uint32_t fs_upper, fs_lower;
  uint32_t fa_upper, fa_lower;
} xlnx_rc_fsfa_t;

typedef struct host_dev_data
{
  uint32_t cmd_id;
  uint32_t cmd_rsp;
  uint32_t ibuf_size;
  uint32_t ibuf_count;
  uint32_t ibuf_index;
  uint64_t ibuf_paddr;
  uint32_t qpbuf_size;
  uint32_t qpbuf_count;
  uint32_t qpbuf_index;
  uint32_t obuf_size;
  uint32_t obuf_count;
  uint32_t freed_ibuf_index;
  uint32_t freed_qpbuf_index;
  vcu_enc_usermeta ibuf_meta;
  obuf_info obuf_info_data[MAX_OUT_BUFF_COUNT];
  uint32_t freed_index_cnt;
  uint32_t obuf_indexes_to_release[MAX_OUT_BUFF_COUNT];
  uint32_t obuf_indexes_to_release_valid_cnt;
  bool is_idr;
  bool end_encoding;
  uint32_t frame_sad[MAX_LOOKAHEAD_DEPTH];
  uint32_t frame_activity[MAX_LOOKAHEAD_DEPTH];
  uint32_t la_depth;
} sk_payload_data;

//////////////// FROM VCU XMA Plugin END //////////////////////

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-h264, stream-format=(string)byte-stream, alignment=(string)au;"
        "video/x-h265, stream-format=(string)byte-stream, alignment=(string)au"));

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{NV12}"))
    );

#define GST_TYPE_IVAS_VIDEO_ENC_CONTROL_RATE (gst_ivas_video_enc_control_rate_get_type ())
typedef enum
{
  RC_CONST_QP,
  RC_VBR,
  RC_CBR,
  RC_LOW_LATENCY,
  RC_CAPPED_VBR,
} GstIvasVideoEncControlRate;

static GType
gst_ivas_video_enc_control_rate_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {RC_CONST_QP, "Disable", "disable"},
      {RC_VBR, "Variable", "variable"},
      {RC_CBR, "Constant", "constant"},
      {RC_LOW_LATENCY, "Low Latency", "low-latency"},
      {RC_CAPPED_VBR, "Capped Variable", "capped-variable"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasVideoEncControlRate", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_VIDEO_ENC_QP_MODE (gst_ivas_video_enc_qp_mode_get_type ())
typedef enum
{
  UNIFORM_QP,
  AUTO_QP,
  ROI_QP,
  RELATIVE_LOAD,
} GstIvasVideoEncQpMode;

static GType
gst_ivas_video_enc_qp_mode_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {UNIFORM_QP, "Use the same QP for all coding units of the frame",
          "uniform"},
      {AUTO_QP,
            "Let the VCU encoder change the QP for each coding unit according to its content",
          "auto"},
      {ROI_QP,
            "Adjust QP according to the regions of interest defined on each frame. Must be set to handle ROI metadata.",
          "roi"},
      {RELATIVE_LOAD,
            "Use the information gathered in the lookahead to calculate the best QP",
          "relative-load"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasVideoEncQpMode", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_VIDEO_ENC_GOP_MODE (gst_ivas_video_enc_gop_mode_get_type ())
typedef enum
{
  DEFAULT_GOP,
  PYRAMIDAL_GOP,
  ADAPTIVE_GOP,
  LOW_DELAY_P,
  LOW_DELAY_B,
} GstIvasVideoEncGopMode;

static GType
gst_ivas_video_enc_gop_mode_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {DEFAULT_GOP, "Basic GOP settings", "basic"},
      {PYRAMIDAL_GOP, "Advanced GOP pattern with hierarchical B-frames",
          "pyramidal"},
      {ADAPTIVE_GOP, "Advanced GOP pattern with adaptive B-frames", "adaptive"},
      {LOW_DELAY_P, "Single I-frame followed by P-frames only", "low-delay-p"},
      {LOW_DELAY_B, "Single I-frame followed by B-frames only", "low-delay-b"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasVideoEncGopMode", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_VIDEO_ENC_GDR_MODE (gst_ivas_video_enc_gdr_mode_get_type ())
typedef enum
{
  GDR_DISABLE,
  GDR_VERTICAL,
  GDR_HORIZONTAL,
} GstIvasVideoEncGdrMode;

static GType
gst_ivas_video_enc_gdr_mode_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {GDR_DISABLE, "No GDR", "disable"},
      {GDR_VERTICAL,
            "Gradual refresh using a vertical bar moving from left to right",
          "vertical"},
      {GDR_HORIZONTAL,
            "Gradual refresh using a horizontal bar moving from top to bottom",
          "horizontal"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasVideoEncGdrMode", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_VIDEO_ENC_SCALING_LIST (gst_ivas_video_enc_scaling_list_get_type ())
typedef enum
{
  SCALING_LIST_FLAT,
  SCALING_LIST_DEFAULT,
} GstIvasVideoEncScalingList;

static GType
gst_ivas_video_enc_scaling_list_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {SCALING_LIST_FLAT, "Flat scaling list mode", "flat"},
      {SCALING_LIST_DEFAULT, "Default scaling list mode", "default"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasVideoEncScalingList", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_VIDEO_ENC_ASPECT_RATIO (gst_ivas_video_enc_aspect_ratio_get_type ())
typedef enum
{
  ASPECT_RATIO_AUTO,
  ASPECT_RATIO_4_3,
  ASPECT_RATIO_16_9,
  ASPECT_RATIO_NONE,
} GstIvasVideoEncAspectRatio;

static GType
gst_ivas_video_enc_aspect_ratio_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {ASPECT_RATIO_AUTO,
            "4:3 for SD video,16:9 for HD video,unspecified for unknown format",
          "auto"},
      {ASPECT_RATIO_4_3, "4:3 aspect ratio", "4-3"},
      {ASPECT_RATIO_16_9, "16:9 aspect ratio", "16-9"},
      {ASPECT_RATIO_NONE,
          "Aspect ratio information is not present in the stream", "none"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasVideoEncAspectRatio", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_ENC_ENTROPY_MODE (gst_ivas_enc_entropy_mode_get_type ())
typedef enum
{
  MODE_CAVLC,
  MODE_CABAC,
} GstIvasEncEntropyMode;

static GType
gst_ivas_enc_entropy_mode_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {MODE_CAVLC, "CAVLC entropy mode", "CAVLC"},
      {MODE_CABAC, "CABAC entropy mode", "CABAC"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasEncEntropyMode", values);
  }
  return qtype;
}

#define GST_TYPE_IVAS_ENC_LOOP_FILTER_MODE (gst_ivas_enc_loop_filter_mode_get_type ())
typedef enum
{
  LOOP_FILTER_ENABLE,
  LOOP_FILTER_DISABLE,
  LOOP_FILTER_DISALE_SLICE_BOUNDARY,
} GstIvasEncLoopFilter;

static GType
gst_ivas_enc_loop_filter_mode_get_type (void)
{
  static GType qtype = 0;
  if (qtype == 0) {
    static const GEnumValue values[] = {
      {LOOP_FILTER_ENABLE, "Enable deblocking filter", "enable"},
      {LOOP_FILTER_DISABLE, "Disable deblocking filter", "disable"},
      {LOOP_FILTER_DISALE_SLICE_BOUNDARY,
            "Disables deblocking filter on slice boundary",
          "disable-slice-boundary"},
      {0, NULL, NULL}
    };
    qtype = g_enum_register_static ("GstIvasEncLoopFilter", values);
  }
  return qtype;
}

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
#define GST_IVAS_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT          G_MAXUINT
#define GST_IVAS_VIDEO_ENC_B_FRAMES_DEFAULT                                   0
#define GST_IVAS_VIDEO_ENC_GOP_LENGTH_DEFAULT                                30
#define GST_IVAS_VIDEO_ENC_ENTROPY_MODE_DEFAULT                      MODE_CABAC
#define GST_IVAS_VIDEO_ENC_CONSTRAINED_INTRA_PREDICTION_DEFAULT           FALSE
#define GST_IVAS_VIDEO_ENC_LOOP_FILTER_MODE_DEFAULT          LOOP_FILTER_ENABLE
#define GST_IVAS_VIDEO_ENC_LOW_BANDWIDTH_DEFAULT                          FALSE
#define GST_IVAS_VCU_ENC_SK_DEFAULT_NAME                   "kernel_vcu_encoder"
#define GST_IVAS_VIDEO_ENC_RC_MODE_DEFAULT                                FALSE
#define GST_IVAS_VIDEO_ENC_KERNEL_NAME_DEFAULT              "encoder:encoder_1"

/* Properties */
enum
{
  PROP_0,
  PROP_XCLBIN_LOCATION,
  PROP_SK_NAME,
  PROP_SK_LIB_PATH,
  PROP_SK_START_INDEX,
  PROP_SK_CURRENT_INDEX,
  PROP_DEVICE_INDEX,
#ifdef ENABLE_XRM_SUPPORT
  PROP_RESERVATION_ID,
#endif
  /* VCU specific properties START */
  PROP_ASPECT_RATIO,
  PROP_B_FRAMES,
  PROP_CONSTRAINED_INTRA_PREDICTION,
  PROP_CONTROL_RATE,
  PROP_CPB_SIZE,
  PROP_ENTROPY_MODE,
  PROP_FILLER_DATA,
  PROP_GDR_MODE,
  PROP_GOP_LENGTH,
  PROP_GOP_MODE,
  PROP_INITIAL_DELAY,
  PROP_LOOP_FILTER_MODE,
  PROP_LOW_BANDWIDTH,
  PROP_MAX_BITRATE,
  PROP_MAX_QP,
  PROP_MIN_QP,
  PROP_NUM_SLICES,
  PROP_IDR_PERIODICITY,
  PROP_PREFETCH_BUFFER,
  PROP_QP_MODE,
  PROP_SCALING_LIST,
  PROP_SLICE_QP,
  PROP_SLICE_SIZE,
  PROP_TARGET_BITRATE,
  PROP_PROFILE,
  PROP_LEVEL,
  PROP_TIER,
  PROP_NUM_CORES,
  PROP_RATE_CONTROL_MODE,
  PROP_KERNEL_NAME,
};

typedef struct _enc_obuf_info
{
  gint idx;
  gint size;
} enc_obuf_info;

struct _GstIvasXVCUEncPrivate
{
  GstBufferPool *input_pool;
  gboolean use_inpool;
  gboolean validate_import;
  GHashTable *in_idx_hash;
  GHashTable *in_buf_hash;
  xclDeviceHandle xcl_handle;
  uuid_t xclbinId;
  gint cu_idx;
  xrt_buffer *ert_cmd_buf;
  xrt_buffer *sk_payload_buf;
  xrt_buffer *static_cfg_buf;
  xrt_buffer *dyn_cfg_buf;
  GArray *out_xrt_bufs;
  GArray *qp_xrt_bufs;
  xrt_buffer *out_bufs_handle;
  xrt_buffer *qp_bufs_handle;
  gint num_in_idx;
  GList *read_oidx_list;
  gboolean init_done;
  gboolean flush_done;          /* to make sure FLUSH cmd issued to softkernel while exiting */
  gboolean deinit_done;         // TODO: instead maintain state of softkernel
  guint min_num_inbufs;
  guint in_buf_size;
  guint cur_qp_idx;
  gboolean intial_qpbufs_consumed;
  guint qpbuf_count;
  GstVideoAlignment in_align;
  sk_payload_data last_rcvd_payload;
  guint last_rcvd_oidx;
  uint64_t timestamp; /* get current time when sending PREINIT command */
#ifdef ENABLE_XRM_SUPPORT
  xrmContext xrm_ctx;
  gboolean resource_inuse;
  xrmCuListResource enc_cu_list_res;
  uint64_t reservation_id;
  int sk_cuID;
#endif
};
#define gst_ivas_xvcuenc_parent_class parent_class

G_DEFINE_TYPE_WITH_PRIVATE (GstIvasXVCUEnc, gst_ivas_xvcuenc,
    GST_TYPE_VIDEO_ENCODER);
#define GST_IVAS_XVCUENC_PRIVATE(enc) \
    (GstIvasXVCUEncPrivate *) (gst_ivas_xvcuenc_get_instance_private (enc))

static const gchar *
ivas_get_vcu_h264_profile_string (const gchar * profile)
{
  if (!g_strcmp0 ("baseline", profile)) {
    return "AVC_BASELINE";
  } else if (!g_strcmp0 ("constrained-baseline", profile)) {
    return "AVC_C_BASELINE";
  } else if (!g_strcmp0 ("constrained-high", profile)) {
    return "AVC_C_HIGH";
  } else if (!g_strcmp0 ("main", profile)) {
    return "AVC_MAIN";
  } else if (!g_strcmp0 ("high", profile)) {
    return "AVC_HIGH";
  } else if (!g_strcmp0 ("high-10", profile)) {
    return "AVC_HIGH10";
  } else if (!g_strcmp0 ("high-4:2:2", profile)) {
    return "AVC_HIGH_422";
  } else if (!g_strcmp0 ("high-10-intra", profile)) {
    return "AVC_HIGH10_INTRA";
  } else if (!g_strcmp0 ("high-4:2:2-intra", profile)) {
    return "AVC_HIGH_422_INTRA,";
  } else if (!g_strcmp0 ("progressive-high", profile)) {
    return "AVC_PROG_HIGH";
  } else if (!g_strcmp0 ("xavc-high-10-intra-cbg", profile)) {
    return "XAVC_HIGH10_INTRA_CBG";
  } else if (!g_strcmp0 ("xavc-high-10-intra-vbr", profile)) {
    return "XAVC_HIGH10_INTRA_VBR";
  } else if (!g_strcmp0 ("xavc-high-4:2:2-intra-cbg", profile)) {
    return "XAVC_HIGH_422_INTRA_CBG";
  } else if (!g_strcmp0 ("xavc-high-4:2:2-intra-vbr", profile)) {
    return " XAVC_HIGH_422_INTRA_VBR";
  } else if (!g_strcmp0 ("xavc-long-gop-main-mp4", profile)) {
    return "XAVC_LONG_GOP_MAIN_MP4";
  } else if (!g_strcmp0 ("xavc-long-gop-high-mp4", profile)) {
    return "XAVC_LONG_GOP_HIGH_MP4";
  } else if (!g_strcmp0 ("xavc-long-gop-high-mxf", profile)) {
    return "XAVC_LONG_GOP_HIGH_MXF";
  } else if (!g_strcmp0 ("xavc-long-gop-high-4:2:2-mxf", profile)) {
    return "XAVC_LONG_GOP_HIGH_422_MXF";
  }
  return NULL;
}

static const gchar *
ivas_get_vcu_h265_profile_string (const gchar * profile)
{
  if (!g_strcmp0 ("main", profile)) {
    return "HEVC_MAIN";
  } else if (!g_strcmp0 ("main-10", profile)) {
    return "HEVC_MAIN10";
  } else if (!g_strcmp0 ("main-still-picture", profile)) {
    return "HEVC_MAIN_STILL";
  } else if (!g_strcmp0 ("monochrome", profile)) {
    return "HEVC_MONO";
  } else if (!g_strcmp0 ("monochrome-10", profile)) {
    return "HEVC_MONO10";
  } else if (!g_strcmp0 ("main-422", profile)) {
    return "HEVC_MAIN_422";
  } else if (!g_strcmp0 ("main-422-10", profile)) {
    return "HEVC_MAIN_422_10";
  } else if (!g_strcmp0 ("main-intra", profile)) {
    return "HEVC_MAIN_INTRA";
  } else if (!g_strcmp0 ("main-10-intra", profile)) {
    return "HEVC_MAIN10_INTRA";
  } else if (!g_strcmp0 ("main-422-10-intra", profile)) {
    return "HEVC_MAIN_422_10_INTRA";
  }
  return NULL;
}

static gboolean
gst_ivas_xvcuenc_map_params (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  gchar params[2048];
  gint width, height;
  const gchar *RateCtrlMode = "CONST_QP";
  const gchar *PrefetchBuffer = "DISABLE";
  const gchar *format = "NV12";
  const gchar *GopCtrlMode = "DEFAULT_GOP";
  const gchar *EntropyMode = "MODE_CABAC";
  const gchar *QPCtrlMode = "UNIFORM_QP";
  const gchar *ScalingList = "DEFAULT";
  const gchar *LoopFilter = "ENABLE";
  const gchar *AspectRatio = "ASPECT_RATIO_AUTO";
  const gchar *EnableFillerData = "ENABLE";
  const gchar *GDRMode = "DISABLE";
  const gchar *ConstIntraPred = "DISABLE";
  const gchar *Profile = "AVC_BASELINE";
  gchar SliceQP[10];
  //GstVideoInfo in_vinfo;
  gboolean bret = FALSE;
  gboolean iret;
  unsigned int fsize;


#if 0
  gst_video_info_init (&in_vinfo);
  bret = gst_video_info_from_caps (&in_vinfo, enc->input_state->caps);
  if (!bret) {
    GST_ERROR_OBJECT (enc, "failed to get video info from input caps");
    return FALSE;
  }
#endif

  width = GST_VIDEO_INFO_WIDTH (&in_vinfo);
  height = GST_VIDEO_INFO_HEIGHT (&in_vinfo);

  switch (enc->control_rate) {
    case RC_CONST_QP:
      RateCtrlMode = "CONST_QP";
      break;
    case RC_VBR:
      RateCtrlMode = "VBR";
      break;
    case RC_CBR:
      RateCtrlMode = "CBR";
      break;
    case RC_LOW_LATENCY:
      RateCtrlMode = "LOW_LATENCY";
      break;
    case RC_CAPPED_VBR:
      RateCtrlMode = "CAPPED_VBR";
      break;
  }
  switch (enc->gop_mode) {
    case DEFAULT_GOP:
      GopCtrlMode = "DEFAULT_GOP";
      break;
    case PYRAMIDAL_GOP:
      GopCtrlMode = "PYRAMIDAL_GOP";
      break;
    case ADAPTIVE_GOP:
      GopCtrlMode = "ADAPTIVE_GOP";
      break;
    case LOW_DELAY_P:
      GopCtrlMode = "LOW_DELAY_P";
      break;
    case LOW_DELAY_B:
      GopCtrlMode = "LOW_DELAY_B";
      break;
  }
  switch (enc->entropy_mode) {
    case MODE_CAVLC:
      EntropyMode = "MODE_CAVLC";
      break;
    case MODE_CABAC:
      EntropyMode = "MODE_CABAC";
      break;
  }
  switch (enc->qp_mode) {
    case UNIFORM_QP:
      QPCtrlMode = "UNIFORM_QP";
      break;
    case AUTO_QP:
      QPCtrlMode = "AUTO_QP";
      break;
    case ROI_QP:
      QPCtrlMode = "ROI_QP";
      break;
    case RELATIVE_LOAD:
      QPCtrlMode = "LOAD_QP | RELATIVE_QP";
      break;
  }
  switch (enc->scaling_list) {
    case SCALING_LIST_FLAT:
      ScalingList = "FLAT";
      break;
    case SCALING_LIST_DEFAULT:
      ScalingList = "DEFAULT";
      break;
  }
  switch (enc->loop_filter_mode) {
    case LOOP_FILTER_ENABLE:
      LoopFilter = "ENABLE";
      break;
    case LOOP_FILTER_DISABLE:
      LoopFilter = "DISABLE";
      break;
    case LOOP_FILTER_DISALE_SLICE_BOUNDARY:
      LoopFilter = "DISALE_SLICE_BOUNDARY";
      break;
  }
  switch (enc->prefetch_buffer) {
    case FALSE:
      PrefetchBuffer = "DISABLE";
      break;
    case TRUE:
      PrefetchBuffer = "ENABLE";
      break;
  }
  switch (enc->aspect_ratio) {
    case ASPECT_RATIO_AUTO:
      AspectRatio = "ASPECT_RATIO_AUTO";
      break;
    case ASPECT_RATIO_4_3:
      AspectRatio = "ASPECT_RATIO_4_3";
      break;
    case ASPECT_RATIO_16_9:
      AspectRatio = "ASPECT_RATIO_16_9";
      break;
    case ASPECT_RATIO_NONE:
      AspectRatio = "ASPECT_RATIO_NONE";
      break;
  }
  switch (enc->filler_data) {
    case FALSE:
      EnableFillerData = "DISABLE";
      break;
    case TRUE:
      EnableFillerData = "ENABLE";
      break;
  }
  switch (enc->gdr_mode) {
    case GDR_DISABLE:
      GDRMode = "DISABLE";
      break;
    case GDR_VERTICAL:
      GDRMode = "GDR_VERTICAL";
      break;
    case GDR_HORIZONTAL:
      GDRMode = "GDR_HORIZONTAL";
      break;
  }
  switch (enc->constrained_intra_prediction) {
    case FALSE:
      ConstIntraPred = "DISABLE";
      break;
    case TRUE:
      ConstIntraPred = "ENABLE";
      break;
  }

  if (enc->slice_qp == -1)
    strcpy (SliceQP, "AUTO");
  else
    sprintf (SliceQP, "%d", enc->slice_qp);

  if (!enc->level)
    enc->level = "5.1";

  if (enc->codec_type == XLNX_CODEC_H264) {
    Profile = ivas_get_vcu_h264_profile_string (enc->profile);
    if (!Profile) {
      g_warning ("profile %s not valid... using default AVC_MAIN",
          enc->profile);
      Profile = "AVC_MAIN";
    }
    GST_LOG_OBJECT (enc, "profile = %s and level = %s", Profile, enc->level);

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
        width, height, format, RateCtrlMode,
        GST_VIDEO_INFO_FPS_N (&in_vinfo), GST_VIDEO_INFO_FPS_D (&in_vinfo),
        enc->target_bitrate, enc->max_bitrate, SliceQP,
        enc->max_qp, enc->min_qp, (double) (enc->cpb_size) / 1000,
        (double) (enc->initial_delay) / 1000, GopCtrlMode, GDRMode,
        enc->gop_length, enc->b_frames, enc->periodicity_idr, Profile,
        enc->level, enc->num_slices,
        QPCtrlMode, enc->slice_size, EnableFillerData, AspectRatio, ScalingList,
        EntropyMode, LoopFilter, ConstIntraPred, PrefetchBuffer, enc->num_cores);
  } else if (enc->codec_type == XLNX_CODEC_H265) {
    Profile = ivas_get_vcu_h265_profile_string (enc->profile);
    if (!Profile) {
      g_warning ("profile %s not valid... using default HEVC_MAIN",
          enc->profile);
      Profile = "HEVC_MAIN";
    }

    if (!enc->tier) {
      enc->tier = g_strdup ("MAIN_TIER");
    } else {
      if (g_strcmp0 (enc->tier, "HIGH_TIER")
          && g_strcmp0 (enc->tier, "MAIN_TIER")) {
        GST_ERROR_OBJECT (enc, "wrong tier %s received", enc->tier);
        goto error;
      }
      enc->tier =
          g_strdup (g_strcmp0 (enc->tier, "high") ? "MAIN_TIER" : "HIGH_TIER");
    }
    GST_LOG_OBJECT (enc, "profile = %s and level = %s and tier = %s", Profile,
        enc->level, enc->tier);

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
        "Tier = %s\n"
        "BitDepth = 8\n"
        "NumSlices = %d\n"
        "QPCtrlMode = %s\n"
        "SliceSize = %d\n"
        "EnableFillerData = %s\n"
        "AspectRatio = %s\n"
        "ScalingList = %s\n"
        "LoopFilter = %s\n"
        "ConstrainedIntraPred = %s\n"
        "LambdaCtrlMode = DEFAULT_LDA\n"
        "CacheLevel2 = %s\n"
        "NumCore = %d\n",
        width, height, format, RateCtrlMode,
        GST_VIDEO_INFO_FPS_N (&in_vinfo), GST_VIDEO_INFO_FPS_D (&in_vinfo),
        enc->target_bitrate, enc->max_bitrate, SliceQP,
        enc->max_qp, enc->min_qp, (double) (enc->cpb_size) / 1000,
        (double) (enc->initial_delay) / 1000, GopCtrlMode, GDRMode,
        enc->gop_length, enc->b_frames, enc->periodicity_idr, Profile,
        enc->level, enc->tier, enc->num_slices, QPCtrlMode, enc->slice_size,
        EnableFillerData, AspectRatio, ScalingList, LoopFilter, ConstIntraPred,
        PrefetchBuffer, enc->num_cores);
  }

  priv->static_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->static_cfg_buf == NULL) {
    GST_ERROR_OBJECT (enc, "failed to allocate encoder config memory handle");
    return FALSE;
  }
  fsize = strlen (params);

  iret = alloc_xrt_buffer (priv->xcl_handle, fsize, XCL_BO_DEVICE_RAM, MEM_BANK,
      priv->static_cfg_buf);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc, "failed to allocate encoder config buffer..");
    goto error;
  }
  strcpy (priv->static_cfg_buf->user_ptr, params);
  if (xclSyncBO (priv->xcl_handle, priv->static_cfg_buf->bo,
          XCL_BO_SYNC_BO_TO_DEVICE, priv->static_cfg_buf->size, 0)) {
    GST_ERROR_OBJECT (enc, "unable to sync to static configuration to device");
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync static configuration to device. reason : %s",
            strerror (errno)));
    goto error1;
  }
  return TRUE;

error1:
  free_xrt_buffer (priv->xcl_handle, priv->static_cfg_buf);

error:
  free (priv->static_cfg_buf);
  priv->static_cfg_buf = NULL;
  return FALSE;
}

static uint16_t
set_rc_mode (gboolean rate_control_mode)
{
  uint16_t rc_mode = AL_RC_CONST_QP;
  if (rate_control_mode) {
    rc_mode = AL_RC_PLUGIN;
  }
  return rc_mode;
}

static gboolean
ivas_xvcuenc_check_softkernel_response (GstIvasXVCUEnc * enc,
    sk_payload_data * payload_buf)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  int iret;

  memset (payload_buf, 0, priv->sk_payload_buf->size);
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync response from encoder softkernel. reason : %s",
            strerror (errno)));
    return FALSE;
  }

  /* check response from softkernel */
  if (!payload_buf->cmd_rsp)
    return FALSE;

  return TRUE;
}

static gboolean
ivas_xvcuenc_allocate_internal_buffers (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  int iret = 0;

  priv->ert_cmd_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->ert_cmd_buf == NULL) {
    GST_ERROR_OBJECT (enc, "failed to allocate ert cmd memory");
    goto error;
  }

  priv->sk_payload_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->sk_payload_buf == NULL) {
    GST_ERROR_OBJECT (enc, "failed to allocate sk payload memory");
    goto error;
  }


  priv->dyn_cfg_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->dyn_cfg_buf == NULL) {
    GST_ERROR_OBJECT (enc,
        "failed to allocate encoder dyncamic config memory handle");
    goto error;
  }

  /* allocate ert command buffer */
  iret =
      alloc_xrt_buffer (priv->xcl_handle, ERT_CMD_SIZE, XCL_BO_SHARED_VIRTUAL,
      1 << 31, priv->ert_cmd_buf);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc, "failed to allocate ert command buffer..");
    goto error;
  }

  /* allocate softkernel payload buffer */
  iret = alloc_xrt_buffer (priv->xcl_handle, sizeof (sk_payload_data),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->sk_payload_buf);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc, "failed to allocate softkernel payload buffer..");
    goto error;
  }

  /* allocate encoder config buffer */
  iret =
      alloc_xrt_buffer (priv->xcl_handle, sizeof (enc_dynamic_params_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->dyn_cfg_buf);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc,
        "failed to allocate encoder dynamic config buffer..");
    goto error;
  }

  return TRUE;

error:
  return FALSE;
}

static void
ivas_xvcuenc_free_internal_buffers (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;

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

static gboolean
ivas_xvcuenc_allocate_output_buffers (GstIvasXVCUEnc * enc, guint num_out_bufs,
    guint out_buf_size)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  uint64_t *out_bufs_addr;
  int iret = 0, i;

  GST_INFO_OBJECT (enc,
      "output buffer allocation: nbuffers = %u and output buffer size = %u",
      num_out_bufs, out_buf_size);

  priv->out_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->out_bufs_handle == NULL) {
    GST_ERROR_OBJECT (enc,
        "failed to allocate encoder output buffers structure");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle, num_out_bufs * sizeof (uint64_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->out_bufs_handle);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc, "failed to allocate encoder out buffers handle..");
    goto error;
  }

  out_bufs_addr = (uint64_t *) (priv->out_bufs_handle->user_ptr);

  for (i = 0; i < num_out_bufs; i++) {
    xrt_buffer *out_xrt_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
    if (out_xrt_buf == NULL) {
      GST_ERROR_OBJECT (enc, "failed to allocate encoder output buffer");
      goto error;
    }

    iret =
        alloc_xrt_buffer (priv->xcl_handle, out_buf_size, XCL_BO_DEVICE_RAM,
        MEM_BANK, out_xrt_buf);
    if (iret < 0) {
      GST_ERROR_OBJECT (enc, "failed to allocate encoder output buffer..");
      goto error;
    }
    /* store each out physical address in priv strucuture */
    out_bufs_addr[i] = out_xrt_buf->phy_addr;
    g_array_append_val (priv->out_xrt_bufs, out_xrt_buf);
  }

  iret = xclSyncBO (priv->xcl_handle, priv->out_bufs_handle->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->out_bufs_handle->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync output buffers handles to device. reason : %s",
            strerror (errno)));
    goto error;
  }

  return TRUE;
error:
  return FALSE;
}

static gboolean
ivas_xvcuenc_allocate_qp_buffers (GstIvasXVCUEnc * enc, guint num_qp_bufs,
    guint qp_buf_size)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  uint64_t *qp_bufs_addr;
  int iret = 0, i;

  GST_INFO_OBJECT (enc,
      "qp buffer allocation: nbuffers = %u and qp buffer size = %u",
      num_qp_bufs, qp_buf_size);

  priv->qp_bufs_handle = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
  if (priv->out_bufs_handle == NULL) {
    GST_ERROR_OBJECT (enc, "failed to allocate encoder qp buffers structure");
    goto error;
  }

  iret = alloc_xrt_buffer (priv->xcl_handle, num_qp_bufs * sizeof (uint64_t),
      XCL_BO_DEVICE_RAM, MEM_BANK, priv->qp_bufs_handle);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc, "failed to allocate encoder qp buffers handle..");
    goto error;
  }

  qp_bufs_addr = (uint64_t *) (priv->qp_bufs_handle->user_ptr);

  for (i = 0; i < num_qp_bufs; i++) {
    xrt_buffer *qp_xrt_buf = (xrt_buffer *) calloc (1, sizeof (xrt_buffer));
    if (qp_xrt_buf == NULL) {
      GST_ERROR_OBJECT (enc, "failed to allocate encoder qp buffer");
      goto error;
    }

    iret = alloc_xrt_buffer (priv->xcl_handle, qp_buf_size, XCL_BO_DEVICE_RAM,
        MEM_BANK, qp_xrt_buf);
    if (iret < 0) {
      GST_ERROR_OBJECT (enc, "failed to allocate encoder qp buffer..");
      goto error;
    }
    /* store each out physical address in priv strucuture */
    qp_bufs_addr[i] = qp_xrt_buf->phy_addr;
    g_array_append_val (priv->qp_xrt_bufs, qp_xrt_buf);
  }

  iret =
      xclSyncBO (priv->xcl_handle, priv->qp_bufs_handle->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->qp_bufs_handle->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync QP buffers handles to device. reason : %s",
            strerror (errno)));
    goto error;
  }

  return TRUE;
error:
  return FALSE;
}

static void
ivas_xvcuenc_free_output_buffers (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  xrt_buffer *out_xrt_buf = NULL;
  guint num_out_bufs;
  int i;

  num_out_bufs = priv->out_xrt_bufs->len;
  for (i = (num_out_bufs - 1); i >= 0; i--) {
    out_xrt_buf = g_array_index (priv->out_xrt_bufs, xrt_buffer *, i);
    g_array_remove_index (priv->out_xrt_bufs, i);
    free_xrt_buffer (priv->xcl_handle, out_xrt_buf);
    free (out_xrt_buf);
  }

  if (priv->out_bufs_handle) {
    free_xrt_buffer (priv->xcl_handle, priv->out_bufs_handle);
    free (priv->out_bufs_handle);
  }
}

static void
ivas_xvcuenc_free_qp_buffers (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  xrt_buffer *qp_xrt_buf = NULL;
  int i;

  for (i = 0; i < priv->qp_xrt_bufs->len; i++) {
    qp_xrt_buf = g_array_index (priv->qp_xrt_bufs, xrt_buffer *, i);
    g_array_remove_index (priv->qp_xrt_bufs, i);
    free_xrt_buffer (priv->xcl_handle, qp_xrt_buf);
    free (qp_xrt_buf);
  }

  if (priv->qp_bufs_handle) {
    free_xrt_buffer (priv->xcl_handle, priv->qp_bufs_handle);
    free (priv->qp_bufs_handle);
  }
}

static gboolean
gst_ivas_xvcuenc_open (GstVideoEncoder * encoder)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);
  GstIvasXVCUEncPrivate *priv = enc->priv;
  unsigned dev_index = enc->dev_index;
  gboolean bret = FALSE;

  priv->cu_idx = 0; // For validation there will be only one CU

  GST_DEBUG_OBJECT (enc, "opening");

  if (download_xclbin (enc->xclbin_path, dev_index, NULL, &(priv->xcl_handle),
          &(priv->xclbinId))) {
    GST_ERROR_OBJECT (enc, "failed to download xclbin %s", enc->xclbin_path);
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to download xclbin : %s", enc->xclbin_path));
    return FALSE;
  }

#if 0
  priv->cu_idx = xclIPName2Index (priv->xcl_handle, enc->kernel_name);
  if (priv->cu_idx < 0) {
    GST_ERROR_OBJECT (enc, "failed to get cu index for kernel/IP name %s",
        enc->kernel_name);
    return FALSE;
  }

  GST_INFO_OBJECT (enc, "kernel %s corresponding CU index %d", enc->kernel_name,
      priv->cu_idx);
#endif

  if (xclOpenContext (priv->xcl_handle, priv->xclbinId, priv->cu_idx, true)) {
    GST_ERROR_OBJECT (enc, "failed to open context CU index %d. "
        "reason : %s", priv->cu_idx, strerror (errno));
    return FALSE;
  }

  GST_INFO_OBJECT (enc,
      "Initialization of XRT is successfully. xrt handle = %p",
      priv->xcl_handle);

  bret = ivas_xvcuenc_allocate_internal_buffers (enc);
  if (bret == FALSE) {
    GST_ERROR_OBJECT (enc, "failed to allocate internal buffers");
    return FALSE;
  }

  priv->intial_qpbufs_consumed = FALSE;

  return TRUE;
}

static gboolean
gst_ivas_xvcuenc_close (GstVideoEncoder * encoder)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);
  GstIvasXVCUEncPrivate *priv = enc->priv;
  int iret;

  /* free all output buffers allocated */
  ivas_xvcuenc_free_output_buffers (enc);

  ivas_xvcuenc_free_qp_buffers (enc);

  /* free all internal buffers */
  ivas_xvcuenc_free_internal_buffers (enc);

  GST_DEBUG_OBJECT (enc, "closing");

#ifdef ENABLE_XRM_SUPPORT
  if (priv->resource_inuse) {
    xrmCuListRelease (priv->xrm_ctx, &priv->enc_cu_list_res);
    priv->resource_inuse = FALSE;
    xrmDestroyContext (priv->xrm_ctx);
  }
#endif

  iret = xclCloseContext (priv->xcl_handle, priv->xclbinId, priv->cu_idx);
  if (iret)
    GST_ERROR_OBJECT (enc, "failed to close context of CU index %d. "
        "reason : %s", priv->cu_idx, strerror (errno));

  xclClose (priv->xcl_handle);
  return TRUE;
}

static gboolean
ivas_xvcuenc_preinit (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  //GstVideoInfo in_vinfo;
  gboolean bret = FALSE;
  enc_dynamic_params_t *dyn_cfg_params;
  int iret = -1;
  struct timespec init_time;

#if 0
  gst_video_info_init (&in_vinfo);
  bret = gst_video_info_from_caps (&in_vinfo, enc->input_state->caps);
  if (!bret) {
    GST_ERROR_OBJECT (enc, "failed to get video info from input caps");
    goto error;
  }
#endif

  dyn_cfg_params = (enc_dynamic_params_t *) (priv->dyn_cfg_buf->user_ptr);
  memset (dyn_cfg_params, 0, priv->dyn_cfg_buf->size);

  dyn_cfg_params->width = GST_VIDEO_INFO_WIDTH (&in_vinfo);
  dyn_cfg_params->height = GST_VIDEO_INFO_HEIGHT (&in_vinfo);
  dyn_cfg_params->framerate =
      ((double) GST_VIDEO_INFO_FPS_N (&in_vinfo)) /
      GST_VIDEO_INFO_FPS_D (&in_vinfo);
  dyn_cfg_params->rc_mode = set_rc_mode(enc->rc_mode);

  GST_INFO_OBJECT (enc,
      "dynamic parameters to enc sk : w = %d, h = %d, fps = %f, rc_mode = %d",
      dyn_cfg_params->width, dyn_cfg_params->height, dyn_cfg_params->framerate,
      dyn_cfg_params->rc_mode);

  // Hack from Bharat for WItcher Stream
  if (dyn_cfg_params->framerate == 0)
    dyn_cfg_params->framerate = 30;

  iret = xclSyncBO (priv->xcl_handle, priv->dyn_cfg_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->dyn_cfg_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync dynamic configuration to device. reason : %s",
            strerror (errno)));
    goto error;
  }

  bret = gst_ivas_xvcuenc_map_params (enc);
  if (!bret) {
    GST_ERROR_OBJECT (enc,
        "Failed to map encoder user parameters to device parameters");
    goto error;
  }

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PREINIT;

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync VCU_PREINIT command payload to device. reason : %s",
            strerror (errno)));
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

  GST_INFO_OBJECT (enc, "sending pre-init command to softkernel");

  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc,
        "failed to send VCU_PREINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to issue VCU_PREINIT command. reason : %s", strerror (errno)));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (!bret) {
      GST_ERROR_OBJECT (enc, "softkernel pre-initialization failed");
      GST_ELEMENT_ERROR (enc, LIBRARY, FAILED, NULL,
          ("encoder softkernel pre-initialization failed"));
      goto error;
    }
  }

  enc->priv->min_num_inbufs = payload_buf->ibuf_count;
  enc->priv->in_buf_size = payload_buf->ibuf_size;
  enc->priv->qpbuf_count = payload_buf->qpbuf_count;

  GST_INFO_OBJECT (enc,
      "minimum input buffers required by encoder %u and input buffer size %u",
      payload_buf->ibuf_count, payload_buf->ibuf_size);
  GST_INFO_OBJECT (enc,
      "minimum output buffers required by encoder %u and output buffer size %u",
      payload_buf->obuf_count, payload_buf->obuf_size);
  GST_DEBUG_OBJECT (enc, "qp buffer count %u and size %u",
      payload_buf->qpbuf_count, payload_buf->qpbuf_size);

  if (!payload_buf->obuf_count || !payload_buf->obuf_size) {
    GST_ERROR_OBJECT (enc,
        "invalid params received from softkernel : outbuf count %u, outbuf size %u",
        payload_buf->obuf_count, payload_buf->obuf_size);
    goto error;
  }

  /* allocate number of output buffers based on softkernel requirement */
  bret = ivas_xvcuenc_allocate_output_buffers (enc, payload_buf->obuf_count,
      payload_buf->obuf_size);
  if (!bret)
    goto error;

  if (payload_buf->qpbuf_count && payload_buf->qpbuf_size) {
    /* allocate number of qp buffers based on softkernel requirement */
    bret = ivas_xvcuenc_allocate_qp_buffers (enc, payload_buf->qpbuf_count,
        payload_buf->qpbuf_size);
    if (!bret)
      goto error;
  }

  GST_INFO_OBJECT (enc, "Successfully pre-initialized softkernel");
  return TRUE;

error:
  return FALSE;
}

static gboolean
ivas_xvcuenc_init (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0;
  gboolean bret = FALSE;

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_INIT;
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync VCU_INIT command payload to device. reason : %s",
            strerror (errno)));
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
    GST_ERROR_OBJECT (enc,
        "failed to send VCU_INIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to issue VCU_INIT command. reason : %s", strerror (errno)));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (!bret) {
      GST_ERROR_OBJECT (enc, "softkernel initialization failed");
      GST_ELEMENT_ERROR (enc, LIBRARY, FAILED, NULL,
          ("softkernel initialization failed"));
      goto error;
    }
  }

  GST_INFO_OBJECT (enc, "Successfully initialized softkernel");
  return TRUE;

error:
  return FALSE;
}

static gboolean
ivas_xvcuenc_allocate_internal_pool (GstIvasXVCUEnc * enc, GstCaps * caps)
{
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstAllocator *allocator = NULL;
  guint width, height;
  GstVideoAlignment align;
  GstAllocationParams alloc_params;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (enc, "Failed to parse caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  width = GST_VIDEO_INFO_WIDTH (&info);
  height = GST_VIDEO_INFO_HEIGHT (&info);

  pool = gst_ivas_buffer_pool_new (WIDTH_ALIGN, HEIGHT_ALIGN);
  allocator = gst_ivas_allocator_new (enc->dev_index, ENABLE_DMABUF);

  gst_allocation_params_init (&alloc_params);
  alloc_params.flags = GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS;

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  /* set alignment requirements */
  gst_video_alignment_reset (&align);
  align.padding_top = 0;
  align.padding_left = 0;
  align.padding_right = ALIGN (width, WIDTH_ALIGN) - width;
  align.padding_bottom = ALIGN (height, HEIGHT_ALIGN) - height;
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  gst_buffer_pool_config_set_video_alignment (config, &align);

  gst_buffer_pool_config_set_allocator (config, allocator, &alloc_params);
  gst_buffer_pool_config_set_params (config, caps, enc->priv->in_buf_size,
      enc->priv->min_num_inbufs, enc->priv->min_num_inbufs + 1);
  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (enc, "Failed to set config on input pool");
    goto error;
  }

  if (enc->priv->input_pool)
    gst_object_unref (enc->priv->input_pool);

  enc->priv->input_pool = pool;

  GST_INFO_OBJECT (enc, "allocated %" GST_PTR_FORMAT " pool",
      enc->priv->input_pool);
  return TRUE;

error:
  return FALSE;
}

static gboolean
ivas_xvcuenc_validate_buffer_import (GstIvasXVCUEnc * enc, GstBuffer * inbuf)
{
  GstStructure *in_config = NULL, *own_config = NULL;
  gboolean bret = TRUE;
  GstMemory *in_mem = NULL;

  if (inbuf->pool) {
    GstVideoAlignment in_align = { 0, };

    /* check pool alignment matches even if we receive our pool */
    in_config = gst_buffer_pool_get_config (inbuf->pool);

    bret = gst_buffer_pool_config_get_video_alignment (in_config, &in_align);
    if (!bret) {
      GST_INFO_OBJECT (enc,
          "failed to get video alignment, use our internal pool");
      enc->priv->use_inpool = TRUE;
      bret = TRUE;              /* not an error */
      goto exit;
    }

    if (in_align.padding_top != enc->priv->in_align.padding_top ||
        in_align.padding_bottom != enc->priv->in_align.padding_bottom ||
        in_align.padding_left != enc->priv->in_align.padding_left ||
        in_align.padding_right != enc->priv->in_align.padding_right) {
      enc->priv->use_inpool = TRUE;
      GST_INFO_OBJECT (enc,
          "padding alignment not matching, use our internal pool");
      goto exit;
    }

    GST_INFO_OBJECT (enc, "can import input pool %p", inbuf->pool);
  }

  in_mem = gst_buffer_get_memory (inbuf, 0);
  if (in_mem == NULL) {
    GST_ERROR_OBJECT (enc, "failed to get memory from input buffer");
    bret = FALSE;
    goto exit;
  }

#if 0
  /* check allocator inside input pool */
  if (!gst_ivas_memory_can_avoid_copy (in_mem, enc->dev_index)) {
    /* use internal pool */
    enc->priv->use_inpool = TRUE;
  }

  GST_INFO_OBJECT (enc, "going to use %s pool as input pool",
      enc->priv->use_inpool ? "internal" : "upstream");
#endif

exit:
  if (in_mem)
    gst_memory_unref (in_mem);
  if (in_config)
    gst_structure_free (in_config);
  if (own_config)
    gst_structure_free (own_config);
  return bret;
}

static gboolean
ivas_xvcuenc_send_frame (GstIvasXVCUEnc * enc, GstVideoCodecFrame * frame)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0, i;
  gboolean bret = FALSE;
  GstMemory *in_mem = NULL;
  guint cur_in_idx = 0xBAD;
  GstBuffer *inbuf = NULL;
  GstIvasLAMeta *lameta = NULL;

#if 0
  if (enc->priv->validate_import) {
    bret = ivas_xvcuenc_validate_buffer_import (enc, frame->input_buffer);
    if (!bret)
      goto error;
    enc->priv->validate_import = FALSE;
  }

  if (GST_BUFFER_FLAG_IS_SET (frame->input_buffer, GST_BUFFER_FLAG_GAP)) {
    GST_DEBUG_OBJECT (enc, "ignoring gap buffer %" GST_PTR_FORMAT,
        frame->input_buffer);
    return TRUE;
  }

  if (G_UNLIKELY (priv->use_inpool)) {
    GstVideoFrame in_vframe, own_vframe;
    GstFlowReturn fret;
    GstBuffer *own_inbuf = NULL;

    memset (&in_vframe, 0x0, sizeof (GstVideoFrame));
    memset (&own_vframe, 0x0, sizeof (GstVideoFrame));

    if (!priv->input_pool) {
      /* allocate internal buffer pool to copy input frames */
      bret = ivas_xvcuenc_allocate_internal_pool (enc, enc->input_state->caps);
      if (!bret)
        goto error;

      if (!gst_buffer_pool_is_active (priv->input_pool))
        gst_buffer_pool_set_active (priv->input_pool, TRUE);
    }

    /* acquire buffer from own input pool */
    fret =
        gst_buffer_pool_acquire_buffer (enc->priv->input_pool, &own_inbuf,
        NULL);
    if (fret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (enc, "failed to allocate buffer from pool %p",
          enc->priv->input_pool);
      goto error;
    }
    GST_LOG_OBJECT (enc, "acquired buffer %p from own pool", own_inbuf);

    /* map internal buffer in write mode */
    if (!gst_video_frame_map (&own_vframe, &enc->input_state->info, own_inbuf,
            GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (enc, "failed to map internal input buffer");
      goto error;
    }

    /* map input buffer in read mode */
    if (!gst_video_frame_map (&in_vframe, &enc->input_state->info,
            frame->input_buffer, GST_MAP_READ)) {
      GST_ERROR_OBJECT (enc, "failed to map input buffer");
      goto error;
    }

    GST_CAT_LOG_OBJECT (GST_CAT_PERFORMANCE, enc,
        "slow copy data from %p to %p", own_inbuf, frame->input_buffer);
    gst_video_frame_copy (&own_vframe, &in_vframe);

    gst_video_frame_unmap (&in_vframe);
    gst_video_frame_unmap (&own_vframe);
    gst_buffer_copy_into (own_inbuf, frame->input_buffer,
        GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS, 0, -1);
    inbuf = own_inbuf;
  } else {
    inbuf = frame->input_buffer;
  }

  in_mem = gst_buffer_get_memory (inbuf, 0);
  if (in_mem == NULL) {
    GST_ERROR_OBJECT (enc, "failed to get memory from internal input buffer");
    goto error;
  }

  bret = gst_ivas_memory_sync_bo (in_mem);
  if (!bret) {
    GST_ERROR_OBJECT (enc, "failed to sync data");
    goto error;
  }

  if (!g_hash_table_contains (priv->in_idx_hash, in_mem)) {
    g_hash_table_insert (priv->in_idx_hash, in_mem,
        GINT_TO_POINTER (priv->num_in_idx));
    GST_DEBUG_OBJECT (enc, "insert new index %d with memory %p and buffer %p",
        priv->num_in_idx, in_mem, inbuf);
    cur_in_idx = priv->num_in_idx++;
  } else {
    cur_in_idx =
        GPOINTER_TO_INT (g_hash_table_lookup (priv->in_idx_hash, in_mem));
    GST_DEBUG_OBJECT (enc, "acquired index %d with memory %p and buffer %p",
        cur_in_idx, in_mem, inbuf);
  }

  inbuf = gst_buffer_ref (inbuf);
  g_hash_table_insert (priv->in_buf_hash, GINT_TO_POINTER (cur_in_idx), inbuf);

  GST_LOG_OBJECT (enc, "mapping buf %p at index %d", inbuf, cur_in_idx);
#endif

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_PUSH;
  payload_buf->ibuf_index = cur_in_idx;
  payload_buf->ibuf_size = gst_buffer_get_size (inbuf);
  payload_buf->ibuf_paddr = gst_ivas_allocator_get_paddr (in_mem);
  payload_buf->ibuf_meta.pts = GST_BUFFER_PTS (inbuf);
  payload_buf->obuf_indexes_to_release_valid_cnt =
      g_list_length (priv->read_oidx_list);

  lameta = gst_buffer_get_ivas_la_meta (frame->input_buffer);
  if (lameta) {
    if (lameta->qpmap && priv->qpbuf_count) {
      GstMapInfo qpinfo = GST_MAP_INFO_INIT;
      gboolean bret = FALSE;
      xrt_buffer *qp_xrt_buf = NULL;

      payload_buf->qpbuf_index = priv->cur_qp_idx;

      qp_xrt_buf = g_array_index (priv->qp_xrt_bufs, xrt_buffer *,
          priv->cur_qp_idx);

      bret = gst_buffer_map (lameta->qpmap, &qpinfo, GST_MAP_READ);
      if (!bret) {
        GST_ERROR_OBJECT (enc, "failed to map qpmap buffer");
        goto error;
      }

      if (qp_xrt_buf->size - 64 < qpinfo.size) {
        GST_ERROR_OBJECT (enc, "qpinfo size (%lu) > device qp buffer size (%d)",
            qpinfo.size, qp_xrt_buf->size - 64);
        goto error;
      }

      memcpy ((gchar *) qp_xrt_buf->user_ptr + 64, qpinfo.data, qpinfo.size);
      gst_buffer_unmap (lameta->qpmap, &qpinfo);

      iret = xclSyncBO (priv->xcl_handle, qp_xrt_buf->bo,
          XCL_BO_SYNC_BO_TO_DEVICE, qp_xrt_buf->size, 0);
      if (iret != 0) {
        GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
            strerror (errno));
        GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
            ("failed to sync QP buffer data to device. reason : %s",
                strerror (errno)));
        goto error;
      }
      GST_LOG_OBJECT (enc, "sent qpmap buffer idx %u of size %u to device",
          priv->cur_qp_idx, qp_xrt_buf->size);
    } else {
      payload_buf->qpbuf_index = 0xBAD;
    }

    /* send rate control data i.e. FSFA data if enabled */
    if (enc->rc_mode && lameta->rc_fsfa) {
      GstMapInfo fsfa_info = GST_MAP_INFO_INIT;
      gboolean bret = FALSE;
      xlnx_rc_fsfa_t *fsfa_ptr;
      guint fsfa_num;
      int i;

      bret = gst_buffer_map (lameta->rc_fsfa, &fsfa_info, GST_MAP_READ);
      if (!bret) {
        GST_ERROR_OBJECT (enc, "failed to map fsfa buffer");
        goto error;
      }

      fsfa_num = fsfa_info.size / sizeof(xlnx_rc_fsfa_t);
      /* Supporting a LA Depth of 20 now */
      if (fsfa_num < MIN_LOOKAHEAD_DEPTH || fsfa_num > MAX_LOOKAHEAD_DEPTH) {
        GST_ERROR_OBJECT (enc, "RC Param Num %d does not match supported LA Depth ", fsfa_num);
        goto error;
      }
      fsfa_ptr = (xlnx_rc_fsfa_t *)fsfa_info.data;

      /* Custom RC */
      for (i = 0; i < fsfa_num; i++) {
        payload_buf->frame_sad[i] = fsfa_ptr[i].fs_upper;
        payload_buf->frame_activity[i] = fsfa_ptr[i].fa_upper;
      }
      /* la_depth is same as number of fsfa items */
      payload_buf->la_depth = fsfa_num;
    } else {
      /* Default RC */
      memset(payload_buf->frame_sad, 0,
          MAX_LOOKAHEAD_DEPTH * sizeof(payload_buf->frame_sad[0]));
      memset(payload_buf->frame_activity, 0,
          MAX_LOOKAHEAD_DEPTH * sizeof(payload_buf->frame_activity[0]));
    }
  } else {
    payload_buf->qpbuf_index = 0xBAD;
    /* Default RC */
    memset(payload_buf->frame_sad, 0,
        MAX_LOOKAHEAD_DEPTH * sizeof(payload_buf->frame_sad[0]));
    memset(payload_buf->frame_activity, 0,
        MAX_LOOKAHEAD_DEPTH * sizeof(payload_buf->frame_activity[0]));
  }

  for (i = 0; i < payload_buf->obuf_indexes_to_release_valid_cnt; i++) {
    gpointer read_oidx = g_list_first (priv->read_oidx_list)->data;
    payload_buf->obuf_indexes_to_release[i] = GPOINTER_TO_INT (read_oidx);
    priv->read_oidx_list = g_list_remove (priv->read_oidx_list, read_oidx);
    GST_DEBUG_OBJECT (enc, "sending back outbuf index to sk = %d",
        GPOINTER_TO_INT (read_oidx));
  }

  GST_LOG_OBJECT (enc, "sending input index %d : size = %lu, paddr = %p",
      cur_in_idx, gst_buffer_get_size (inbuf),
      (void *) gst_ivas_allocator_get_paddr (in_mem));

  /* transfer payload settings to device */
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync VCU_PUSH command payload to device. reason : %s",
            strerror (errno)));
    goto error;
  }

  gst_memory_unref (in_mem);

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

  GST_LOG_OBJECT (enc, "sending VCU_PUSH command to softkernel");

  /* send command to softkernel */
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc,
        "failed to send VCU_PUSH command to softkernel - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to issue VCU_PUSH command. reason : %s", strerror (errno)));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (!bret) {
      GST_ERROR_OBJECT (enc, "softkernel send frame failed");
      GST_ELEMENT_ERROR (enc, LIBRARY, FAILED, NULL,
          ("softkernel send frame failed"));
      goto error;
    }
  }

  GST_DEBUG_OBJECT (enc,
      "successfully completed VCU_PUSH command : input buffer index freed %d",
      payload_buf->freed_ibuf_index);

  if (payload_buf->freed_ibuf_index != 0xBAD) {
    GstBuffer *buf = NULL;

    if (!g_hash_table_contains (priv->in_buf_hash,
            GINT_TO_POINTER (payload_buf->freed_ibuf_index))) {
      GST_ERROR_OBJECT (enc, "wrong index received %d",
          payload_buf->freed_ibuf_index);
      return FALSE;
    }
    buf = g_hash_table_lookup (priv->in_buf_hash,
        GINT_TO_POINTER (payload_buf->freed_ibuf_index));
    GST_LOG_OBJECT (enc, "free buffer %p corresponding to index %d", buf,
        payload_buf->freed_ibuf_index);

    g_hash_table_remove (priv->in_buf_hash,
        GINT_TO_POINTER (payload_buf->freed_ibuf_index));
  }

  if (priv->qpbuf_count) {
    if (priv->intial_qpbufs_consumed) {
      if (payload_buf->freed_qpbuf_index != 0xBAD) {
        priv->cur_qp_idx = payload_buf->freed_qpbuf_index;
        GST_DEBUG_OBJECT (enc, "received freed qpbuf index %u from device",
            priv->cur_qp_idx);
      } else {
        GST_ERROR_OBJECT (enc, "unexpected error...freed_qpbuf_index is wrong");
        GST_ELEMENT_ERROR (enc, LIBRARY, FAILED, NULL,
            ("encoder softkernel sent wrong qp buffer index"));
      }
    } else {
      priv->cur_qp_idx++;
      if (priv->cur_qp_idx == (priv->qpbuf_count - 1))
        priv->intial_qpbufs_consumed = TRUE;
    }
  }

  if (inbuf != frame->input_buffer)
    gst_buffer_unref (inbuf);
  return TRUE;

error:
  if (in_mem)
    gst_memory_unref (in_mem);
  if (inbuf != frame->input_buffer)
    gst_buffer_unref (inbuf);
  return FALSE;
}

static gboolean
ivas_xvcuenc_read_output_frame (GstIvasXVCUEnc * enc, GstBuffer * outbuf,
    gint oidx, gint outsize)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  xrt_buffer *out_xrt_buf = NULL;
  GstMapInfo map_info = GST_MAP_INFO_INIT;
  int iret = 0;

  out_xrt_buf = g_array_index (priv->out_xrt_bufs, xrt_buffer *, oidx);

  if (outsize > out_xrt_buf->size) {
    GST_ERROR_OBJECT (enc,
        "received out frame size %d greater than allocated xrt buffer size %d",
        outsize, out_xrt_buf->size);
    goto error;
  }

  iret = xclSyncBO (priv->xcl_handle, out_xrt_buf->bo,
      XCL_BO_SYNC_BO_FROM_DEVICE, outsize, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "xclSyncBO failed for output buffer. error = %d",
        iret);
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync encoded data from device. reason : %s",
            strerror (errno)));
    goto error;
  }

#if 0
  if (!gst_buffer_map (outbuf, &map_info, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (enc, "failed to map output buffer!");
    goto error;
  }
#endif

  //TODO: xclReadBO can be avoided if we allocate buffers using GStreamer pool
  iret =
      xclReadBO (priv->xcl_handle, out_xrt_buf->bo, map_info.data, outsize, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "failed to read output buffer. reason : %s",
        strerror (errno));
    goto error;
  }

  gst_buffer_unmap (outbuf, &map_info);
  return TRUE;

error:
  if (map_info.data)
    gst_buffer_unmap (outbuf, &map_info);
  return FALSE;
}

static GstFlowReturn
ivas_xvcuenc_receive_out_frame (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  GstVideoCodecFrame *frame = NULL;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0;
  gboolean bret = FALSE;
  gint oidx, outsize;
  GstFlowReturn fret = GST_FLOW_ERROR;

  if (priv->last_rcvd_payload.freed_index_cnt) {
    oidx =
        priv->last_rcvd_payload.obuf_info_data[priv->
        last_rcvd_oidx].obuff_index;
    if (oidx == 0xBAD) {
      GST_ERROR_OBJECT (enc, "received bad index from softkernel");
      goto error;
    }

    outsize =
        priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].recv_size;
    frame = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (enc));

    if (gst_video_encoder_allocate_output_frame (GST_VIDEO_ENCODER (enc), frame,
            outsize) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (enc, "Could not allocate output buffer");
      return GST_FLOW_ERROR;
    }

    GST_LOG_OBJECT (enc, "reading encoded output at index %d with size %d",
        oidx, outsize);

    bret =
        ivas_xvcuenc_read_output_frame (enc, frame->output_buffer, oidx,
        outsize);
    if (!bret)
      return GST_FLOW_ERROR;

    GST_BUFFER_PTS (frame->output_buffer) =
        priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].
        obuf_meta.pts;
    GST_LOG_OBJECT (enc, "pushing output buffer %p with pts %" GST_TIME_FORMAT,
        frame->output_buffer,
        GST_TIME_ARGS (GST_BUFFER_PTS (frame->output_buffer)));
    priv->read_oidx_list =
        g_list_append (priv->read_oidx_list, GINT_TO_POINTER (oidx));

    fret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (enc), frame);
    if (fret != GST_FLOW_OK)
      goto error;

    priv->last_rcvd_payload.freed_index_cnt--;
    priv->last_rcvd_oidx++;
    return fret;
  }

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_RECEIVE;
  iret =
      xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync VCU_RECEIVE command payload to device. reason : %s",
            strerror (errno)));
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

  GST_LOG_OBJECT (enc, "sending VCU_RECEIVE command to softkernel");

  /* send command to softkernel */
  iret = send_softkernel_command (priv->xcl_handle, priv->ert_cmd_buf,
      payload_data, num_idx, enc->sk_cur_idx, CMD_EXEC_TIMEOUT);
  if (iret < 0) {
    GST_ERROR_OBJECT (enc,
        "failed to send VCU_RECEIVE command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to issue VCU_RECEIVE command. reason : %s", strerror (errno)));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (!bret) {
      GST_ERROR_OBJECT (enc, "softkernel receive frame failed");
      GST_ELEMENT_ERROR (enc, LIBRARY, FAILED, NULL,
          ("softkernel receive frame failed"));
      goto error;
    }
  }
  GST_LOG_OBJECT (enc, "successfully completed VCU_RECEIVE command");

  GST_LOG_OBJECT (enc, "freed index count received from softkernel = %d",
      payload_buf->freed_index_cnt);
  if (payload_buf->freed_index_cnt == 0) {
    if (payload_buf->end_encoding) {
      GST_INFO_OBJECT (enc, "received EOS from softkernel");
      return GST_FLOW_EOS;
    }
    GST_DEBUG_OBJECT (enc, "no encoded buffers to consume");
    return GST_FLOW_OK;
  }

  memcpy (&priv->last_rcvd_payload, payload_buf, sizeof (sk_payload_data));

  priv->last_rcvd_oidx = 0;

  oidx =
      priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].obuff_index;
  if (oidx == 0xBAD) {
    GST_ERROR_OBJECT (enc, "received bad index from softkernel");
    goto error;
  }

  outsize =
      priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].recv_size;
  frame = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (enc));

  if (gst_video_encoder_allocate_output_frame (GST_VIDEO_ENCODER (enc), frame,
          outsize) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (enc, "Could not allocate output buffer");
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (enc, "reading encoded output at index %d with size %d",
      oidx, outsize);

  bret = ivas_xvcuenc_read_output_frame (enc, frame->output_buffer, oidx,
      outsize);
  if (!bret)
    return GST_FLOW_ERROR;

  GST_BUFFER_PTS (frame->output_buffer) =
      priv->last_rcvd_payload.obuf_info_data[priv->last_rcvd_oidx].
      obuf_meta.pts;
  priv->read_oidx_list =
      g_list_append (priv->read_oidx_list, GINT_TO_POINTER (oidx));

  GST_LOG_OBJECT (enc, "pushing output buffer %p with pts %" GST_TIME_FORMAT,
      frame->output_buffer,
      GST_TIME_ARGS (GST_BUFFER_PTS (frame->output_buffer)));

  fret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (enc), frame);
  if (fret != GST_FLOW_OK)
    goto error;

  priv->last_rcvd_payload.freed_index_cnt--;
  priv->last_rcvd_oidx++;

  return fret;

error:
  return fret;
}

static gboolean
ivas_xvcuenc_send_flush (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  gboolean bret = FALSE;
  int iret = 0;
  unsigned int num_idx = 0;

  if (priv->flush_done) {
    GST_WARNING_OBJECT (enc,
        "flush already issued to softkernel, hence returning");
    return TRUE;
  }
  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_FLUSH;
  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync VCU_FLUSH command payload to device. reason : %s",
            strerror (errno)));
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
    GST_ERROR_OBJECT (enc,
        "failed to send VCU_FLUSH command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to issue VCU_FLUSH command. reason : %s", strerror (errno)));
    goto error;
  } else {
    bret = ivas_xvcuenc_check_softkernel_response (enc, payload_buf);
    if (!bret) {
      GST_ERROR_OBJECT (enc, "softkernel flush failed");
      GST_ELEMENT_ERROR (enc, LIBRARY, FAILED, NULL,
          ("softkernel flush failed"));
      goto error;
    }
  }
  GST_DEBUG_OBJECT (enc, "successfully sent flush command");
  priv->flush_done = TRUE;
  return TRUE;

error:
  return FALSE;
}

static gboolean
ivas_xvcuenc_deinit (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = enc->priv;
  sk_payload_data *payload_buf;
  unsigned int payload_data[1024];
  unsigned int num_idx = 0;
  int iret = 0, i;

  if (priv->deinit_done) {
    GST_WARNING_OBJECT (enc,
        "deinit already issued to softkernel, hence returning");
    return TRUE;
  }

  /* update payload buf */
  payload_buf = (sk_payload_data *) (priv->sk_payload_buf->user_ptr);
  memset (payload_buf, 0, priv->sk_payload_buf->size);

  payload_buf->cmd_id = VCU_DEINIT;
  payload_buf->obuf_indexes_to_release_valid_cnt =
      g_list_length (priv->read_oidx_list);

  GST_INFO_OBJECT (enc, "released buffers sending to deinit %d",
      payload_buf->obuf_indexes_to_release_valid_cnt);
  for (i = 0; i < payload_buf->obuf_indexes_to_release_valid_cnt; i++) {
    gpointer read_oidx = g_list_first (priv->read_oidx_list)->data;
    payload_buf->obuf_indexes_to_release[i] = GPOINTER_TO_INT (read_oidx);
    priv->read_oidx_list = g_list_remove (priv->read_oidx_list, read_oidx);
    GST_LOG_OBJECT (enc, "sending read output index %d to softkernel",
        GPOINTER_TO_INT (read_oidx));
  }

  iret = xclSyncBO (priv->xcl_handle, priv->sk_payload_buf->bo,
      XCL_BO_SYNC_BO_TO_DEVICE, priv->sk_payload_buf->size, 0);
  if (iret != 0) {
    GST_ERROR_OBJECT (enc, "synbo failed - %d, reason : %s", iret,
        strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, SYNC, NULL,
        ("failed to sync VCU_DEINIT command payload to device. reason : %s",
            strerror (errno)));
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
    GST_ERROR_OBJECT (enc,
        "failed to send VCU_DEINIT command to softkernel - %d, reason : %s",
        iret, strerror (errno));
    GST_ELEMENT_ERROR (enc, RESOURCE, FAILED, NULL,
        ("failed to issue VCU_DEINIT command. reason : %s", strerror (errno)));
    goto error;
  }
  GST_INFO_OBJECT (enc, "completed de-initialization");
  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_ivas_xvcuenc_stop (GstVideoEncoder * encoder)
{
  gboolean bret = TRUE;
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);

  GST_DEBUG_OBJECT (GST_IVAS_XVCUENC (encoder), "stop");

  if (enc->priv->init_done) {
    bret = ivas_xvcuenc_send_flush (GST_IVAS_XVCUENC (encoder));
    if (!bret)
      return bret;

    bret = ivas_xvcuenc_deinit (GST_IVAS_XVCUENC (encoder));
    enc->priv->init_done = FALSE;
  }

  if (enc->priv->input_pool) {
    if (gst_buffer_pool_is_active (enc->priv->input_pool))
      gst_buffer_pool_set_active (enc->priv->input_pool, FALSE);
    gst_clear_object (&enc->priv->input_pool);
  }

  return bret;
}

static gboolean
gst_ivas_xvcuenc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);
  gboolean bret = TRUE;
  GstVideoCodecState *output_state;
  GstCaps *outcaps = NULL;
  GstCaps *allowed_caps = NULL;
  GstCaps *peercaps = NULL;
  gchar *caps_str = NULL;

  GST_DEBUG_OBJECT (enc, "input caps: %" GST_PTR_FORMAT, state->caps);

  // TODO: add support for reconfiguration .e.g. deinit the encoder
  if (enc->input_state) {
    gst_video_codec_state_unref (enc->input_state);
    enc->input_state = NULL;
  }
  enc->input_state = gst_video_codec_state_ref (state);

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));
  allowed_caps = gst_caps_truncate (allowed_caps);

  GST_DEBUG_OBJECT (enc, "allowed caps: %" GST_PTR_FORMAT, allowed_caps);

  caps_str = gst_caps_to_string (allowed_caps);
  gst_caps_unref (allowed_caps);

  if (g_str_has_prefix (caps_str, "video/x-h264")) {
    enc->codec_type = XLNX_CODEC_H264;
  } else if (g_str_has_prefix (caps_str, "video/x-h265")) {
    enc->codec_type = XLNX_CODEC_H265;
  }
  g_free (caps_str);

  GST_INFO_OBJECT (enc, "encoder type selected = %s",
      enc->codec_type == XLNX_CODEC_H264 ? "h264" : "h265");

  outcaps = gst_caps_copy (gst_static_pad_template_get_caps (&src_template));

  // TODO: add case for 10-bit as well once encoder support 10-bit formats
  gst_caps_set_simple (outcaps, "chroma-format", G_TYPE_STRING, "4:2:0",
      "bit-depth-luma", G_TYPE_UINT, 8, "bit-depth-chroma", G_TYPE_UINT, 8,
      NULL);

  GST_DEBUG_OBJECT (enc, "output caps: %" GST_PTR_FORMAT, outcaps);

  /* Set profile and level */
  peercaps = gst_pad_peer_query_caps (GST_VIDEO_ENCODER_SRC_PAD (enc),
      gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (enc)));

  if (peercaps && !gst_caps_is_empty (peercaps)) {
    GstStructure *s;

    s = gst_caps_get_structure (peercaps, 0);
    enc->profile = g_strdup (gst_structure_get_string (s, "profile"));
    enc->level = g_strdup (gst_structure_get_string (s, "level"));
    GST_INFO_OBJECT (enc,
        "profile = %s and level = %s received from downstream", enc->profile,
        enc->level);

    if (enc->codec_type == XLNX_CODEC_H265) {
      enc->tier = g_strdup (gst_structure_get_string (s, "tier"));
      GST_INFO_OBJECT (enc, "Tier %s received from downstream", enc->tier);
    }
  }

  if (peercaps)
    gst_caps_unref (peercaps);

  if (enc->codec_type == XLNX_CODEC_H264) {
    if (g_strcmp0 (enc->profile, "") == -1) {
      enc->profile = "main";
    } else if (g_str_has_prefix (enc->profile, "HEVC")) {
      GST_ERROR_OBJECT (enc, "Codec Type and Profile donot match");
      return false;
    }
    gst_caps_remove_structure (outcaps, 1);
  } else if (enc->codec_type == XLNX_CODEC_H265) {
    if (g_strcmp0 (enc->profile, "") == -1) {
      enc->profile = "main";
    } else if (g_str_has_prefix (enc->profile, "AVC")) {
      GST_ERROR_OBJECT (enc, "Codec Type and Profile donot match");
      return false;
    }
    gst_caps_remove_structure (outcaps, 0);
  } else {
    GST_ERROR_OBJECT (enc, "Encoder Type not specified");
    return false;
  }

  if (!enc->priv->init_done) {
#ifdef ENABLE_XRM_SUPPORT
    bret = alloc_encoder (encoder);

    if (!bret) {
      return FALSE;
    }
#endif

    bret = ivas_xvcuenc_preinit (enc);
    if (!bret)
      return FALSE;

    bret = ivas_xvcuenc_init (enc);
    if (!bret)
      return FALSE;

    enc->priv->init_done = TRUE;
    memset (&enc->priv->last_rcvd_payload, 0x00, sizeof (sk_payload_data));
    enc->priv->last_rcvd_oidx = 0;
  }

#if 0
  GST_DEBUG_OBJECT (enc, "output caps modified : %" GST_PTR_FORMAT, outcaps);

  output_state = gst_video_encoder_set_output_state (encoder, outcaps, state);
  gst_video_codec_state_unref (output_state);

  return gst_video_encoder_negotiate (encoder);
#endif

  return TRUE;
}

static GstStructure *
get_allocation_video_meta (GstIvasXVCUEnc * enc, GstVideoInfo * info)
{
  GstStructure *result;
  GstVideoAlignment alig;
  gsize plane_size[GST_VIDEO_MAX_PLANES];
  GstVideoInfo new_info;
  guint width, height;

  gst_video_alignment_reset (&alig);

  /* Create a copy of @info without any offset/stride as we need a
   * 'standard' version to compute the paddings. */
  gst_video_info_init (&new_info);
  gst_video_info_set_interlaced_format (&new_info,
      GST_VIDEO_INFO_FORMAT (info),
      GST_VIDEO_INFO_INTERLACE_MODE (info),
      GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));

  /* Retrieve the plane sizes */
  if (!gst_video_info_align_full (&new_info, &alig, plane_size)) {
    GST_WARNING_OBJECT (enc, "Failed to retrieve plane sizes");
    return NULL;
  }

  width = GST_VIDEO_INFO_WIDTH (info);
  height = GST_VIDEO_INFO_HEIGHT (info);

  alig.padding_top = 0;
  alig.padding_left = 0;
  alig.padding_right = ALIGN (width, WIDTH_ALIGN) - width;
  alig.padding_bottom = ALIGN (height, HEIGHT_ALIGN) - height;

  result = gst_structure_new_empty ("video-meta");

  gst_structure_set (result, "padding-top", G_TYPE_UINT, alig.padding_top,
      "padding-bottom", G_TYPE_UINT, alig.padding_bottom,
      "padding-left", G_TYPE_UINT, alig.padding_left,
      "padding-right", G_TYPE_UINT, alig.padding_right, NULL);

  /* Encoder doesn't support splitting planes on multiple buffers */
  gst_structure_set (result, "single-allocation", G_TYPE_BOOLEAN, TRUE, NULL);

  GST_LOG_OBJECT (enc, "Request buffer layout to producer: %" GST_PTR_FORMAT,
      result);

  return result;
}

static gboolean
gst_ivas_xvcuenc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);
  GstStructure *meta_param;
  GstAllocationParams alloc_params;
  GstCaps *caps;
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstAllocator *allocator = NULL;
  gint width, height;

  gst_query_parse_allocation (query, &caps, NULL);

  if (!caps) {
    GST_WARNING_OBJECT (enc, "allocation query does not contain caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (enc, "Failed to parse caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  meta_param = get_allocation_video_meta (enc, &info);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, meta_param);
  gst_structure_free (meta_param);

  pool = gst_ivas_buffer_pool_new (WIDTH_ALIGN, HEIGHT_ALIGN);

#ifdef ENABLE_XRM_SUPPORT
  if (enc->priv->resource_inuse == FALSE) {
    GST_ERROR_OBJECT (enc, "ENCODER: Propose allocation: Cu not yet allocated");
    return FALSE;
  }
#endif

  allocator = gst_ivas_allocator_new (enc->dev_index, ENABLE_DMABUF);

  gst_allocation_params_init (&alloc_params);
  alloc_params.flags = GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS;

  width = GST_VIDEO_INFO_WIDTH (&info);
  height = GST_VIDEO_INFO_HEIGHT (&info);

  gst_video_alignment_reset (&enc->priv->in_align);
  enc->priv->in_align.padding_top = 0;
  enc->priv->in_align.padding_left = 0;
  enc->priv->in_align.padding_right = ALIGN (width, WIDTH_ALIGN) - width;
  enc->priv->in_align.padding_bottom = ALIGN (height, HEIGHT_ALIGN) - height;

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_allocator (config, allocator, &alloc_params);
  gst_buffer_pool_config_set_video_alignment (config, &enc->priv->in_align);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  gst_buffer_pool_config_set_params (config, caps, enc->priv->in_buf_size,
      enc->priv->min_num_inbufs, 0);
  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_INFO_OBJECT (enc, "Failed to set config on input pool");
    goto error;
  }
  gst_query_add_allocation_pool (query, pool, enc->priv->in_buf_size,
      enc->priv->min_num_inbufs, 0);
  gst_query_add_allocation_param (query, allocator, &alloc_params);

  GST_DEBUG_OBJECT (enc, "query updated %" GST_PTR_FORMAT, query);
  gst_object_unref (allocator);
  gst_object_unref (pool);
  return TRUE;

error:
  if (pool)
    gst_object_unref (pool);
  return FALSE;
}

static void
gst_ivas_xvcuenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (object);

  switch (prop_id) {
    case PROP_XCLBIN_LOCATION:
      if (enc->xclbin_path)
        g_free (enc->xclbin_path);
      enc->xclbin_path = g_value_dup_string (value);
      break;
    case PROP_SK_CURRENT_INDEX:
      enc->sk_cur_idx = g_value_get_int (value);
      break;
    case PROP_DEVICE_INDEX:
      enc->dev_index = g_value_get_int (value);
      break;
#ifdef ENABLE_XRM_SUPPORT
    case PROP_RESERVATION_ID:
      enc->priv->reservation_id = g_value_get_int (value);
      break;
#endif
    case PROP_ASPECT_RATIO:
      enc->aspect_ratio = g_value_get_enum (value);
      break;
    case PROP_B_FRAMES:
      enc->b_frames = g_value_get_uint (value);
      break;
    case PROP_CONSTRAINED_INTRA_PREDICTION:
      enc->constrained_intra_prediction = g_value_get_boolean (value);
      break;
    case PROP_CONTROL_RATE:
      enc->control_rate = g_value_get_enum (value);
      break;
    case PROP_CPB_SIZE:
      enc->cpb_size = g_value_get_uint (value);
      break;
      /*only for H.264 */
    case PROP_ENTROPY_MODE:
      enc->entropy_mode = g_value_get_enum (value);
      break;
    case PROP_FILLER_DATA:
      enc->filler_data = g_value_get_boolean (value);
      break;
    case PROP_GDR_MODE:
      enc->gdr_mode = g_value_get_enum (value);
      break;
    case PROP_GOP_LENGTH:
      enc->gop_length = g_value_get_uint (value);
      break;
    case PROP_GOP_MODE:
      enc->gop_mode = g_value_get_enum (value);
      break;
    case PROP_INITIAL_DELAY:
      enc->initial_delay = g_value_get_uint (value);
      break;
    case PROP_LOOP_FILTER_MODE:
      enc->loop_filter_mode = g_value_get_enum (value);
      break;
    case PROP_MAX_BITRATE:
      enc->max_bitrate = g_value_get_uint (value);
      break;
    case PROP_MAX_QP:
      enc->max_qp = g_value_get_uint (value);
      break;
    case PROP_MIN_QP:
      enc->min_qp = g_value_get_uint (value);
      break;
    case PROP_NUM_SLICES:
      enc->num_slices = g_value_get_uint (value);
      break;
    case PROP_IDR_PERIODICITY:
      enc->periodicity_idr = g_value_get_uint (value);
      break;
    case PROP_PREFETCH_BUFFER:
      enc->prefetch_buffer = g_value_get_boolean (value);
      break;
    case PROP_QP_MODE:
      enc->qp_mode = g_value_get_enum (value);
      break;
    case PROP_SCALING_LIST:
      enc->scaling_list = g_value_get_enum (value);
      break;
    case PROP_SLICE_QP:
      enc->slice_qp = g_value_get_int (value);
      break;
    case PROP_SLICE_SIZE:
      enc->slice_size = g_value_get_uint (value);
      break;
    case PROP_TARGET_BITRATE:
      enc->target_bitrate = g_value_get_uint (value);
      break;
    case PROP_PROFILE:
      enc->profile = g_value_dup_string (value);
      break;
    case PROP_LEVEL:
      enc->level = g_value_dup_string (value);
      break;
      /*only for H.265 */
    case PROP_TIER:
      enc->tier = g_value_dup_string (value);
      break;
    case PROP_NUM_CORES:
      enc->num_cores = g_value_get_uint (value);
      break;
    case PROP_RATE_CONTROL_MODE:
      enc->rc_mode = g_value_get_boolean (value);
      break;
    case PROP_KERNEL_NAME:
      if (enc->kernel_name)
        g_free (enc->kernel_name);

      enc->kernel_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ivas_xvcuenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (object);

  switch (prop_id) {
    case PROP_SK_CURRENT_INDEX:
      g_value_set_int (value, enc->sk_cur_idx);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, enc->dev_index);
      break;
#ifdef ENABLE_XRM_SUPPORT
    case PROP_RESERVATION_ID:
      g_value_set_int (value, enc->priv->reservation_id);
      break;
#endif
    case PROP_ASPECT_RATIO:
      g_value_set_enum (value, enc->aspect_ratio);
      break;
    case PROP_B_FRAMES:
      g_value_set_uint (value, enc->b_frames);
      break;
    case PROP_CONSTRAINED_INTRA_PREDICTION:
      g_value_set_boolean (value, enc->constrained_intra_prediction);
      break;
    case PROP_CONTROL_RATE:
      g_value_set_enum (value, enc->control_rate);
      break;
    case PROP_CPB_SIZE:
      g_value_set_uint (value, enc->cpb_size);
      break;
    case PROP_ENTROPY_MODE:    /*only for H.264 */
      g_value_set_enum (value, enc->entropy_mode);
      break;
    case PROP_FILLER_DATA:
      g_value_set_boolean (value, enc->filler_data);
      break;
    case PROP_GDR_MODE:
      g_value_set_enum (value, enc->gdr_mode);
      break;
    case PROP_GOP_LENGTH:
      g_value_set_uint (value, enc->gop_length);
      break;
    case PROP_GOP_MODE:
      g_value_set_enum (value, enc->gop_mode);
      break;
    case PROP_INITIAL_DELAY:
      g_value_set_uint (value, enc->initial_delay);
      break;
    case PROP_LOOP_FILTER_MODE:
      g_value_set_enum (value, enc->loop_filter_mode);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, enc->max_bitrate);
      break;
    case PROP_MAX_QP:
      g_value_set_uint (value, enc->max_qp);
      break;
    case PROP_MIN_QP:
      g_value_set_uint (value, enc->min_qp);
      break;
    case PROP_NUM_SLICES:
      g_value_set_uint (value, enc->num_slices);
      break;
    case PROP_IDR_PERIODICITY:
      g_value_set_uint (value, enc->periodicity_idr);
      break;
    case PROP_PREFETCH_BUFFER:
      g_value_set_boolean (value, enc->prefetch_buffer);
      break;
    case PROP_QP_MODE:
      g_value_set_enum (value, enc->qp_mode);
      break;
    case PROP_SCALING_LIST:
      g_value_set_enum (value, enc->scaling_list);
      break;
    case PROP_SLICE_QP:
      g_value_set_int (value, enc->slice_qp);
      break;
    case PROP_SLICE_SIZE:
      g_value_set_uint (value, enc->slice_size);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_uint (value, enc->target_bitrate);
      break;
    case PROP_PROFILE:
      g_value_set_string (value, enc->profile);
      break;
    case PROP_LEVEL:
      g_value_set_string (value, enc->level);
      break;
      /*only for H.265 */
    case PROP_TIER:
      g_value_set_string (value, enc->tier);
      break;
    case PROP_NUM_CORES:
      g_value_set_uint (value, enc->num_cores);
      break;
    case PROP_RATE_CONTROL_MODE:
      g_value_set_boolean (value, enc->rc_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_ivas_xvcuenc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);
  gboolean bret = FALSE;

  bret = ivas_xvcuenc_send_frame (enc, frame);
  if (!bret)
    goto error;

  if (frame)
    gst_video_codec_frame_unref (frame);

  return ivas_xvcuenc_receive_out_frame (enc);

error:
  gst_video_codec_frame_unref (frame);
  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_ivas_xvcuenc_finish (GstVideoEncoder * encoder)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (encoder);
  GstFlowReturn fret = GST_FLOW_OK;
  gboolean bret = FALSE;

  GST_DEBUG_OBJECT (enc, "finish");

  // TODO: add support when encoder not negotiated
  bret = ivas_xvcuenc_send_flush (enc);
  if (!bret)
    goto error;

  do {
    fret = ivas_xvcuenc_receive_out_frame (enc);
  } while (fret == GST_FLOW_OK);

  /* EOS from device means, all inputs are free */
  g_hash_table_remove_all (enc->priv->in_buf_hash);

  return fret;

error:
  return GST_FLOW_ERROR;
}

static void
gst_ivas_xvcuenc_finalize (GObject * object)
{
  GstIvasXVCUEnc *enc = GST_IVAS_XVCUENC (object);

  // TODO: array elements need to be freed before this
  g_hash_table_unref (enc->priv->in_idx_hash);
  g_hash_table_unref (enc->priv->in_buf_hash);
  g_array_free (enc->priv->out_xrt_bufs, FALSE);
  g_array_free (enc->priv->qp_xrt_bufs, FALSE);

  if (enc->input_state) {
    gst_video_codec_state_unref (enc->input_state);
    enc->input_state = NULL;
  }

  if (enc->xclbin_path)
    g_free (enc->xclbin_path);
  if (enc->sk_name)
    g_free (enc->sk_name);
  if (enc->sk_lib_path)
    g_free (enc->sk_lib_path);
  if (enc->kernel_name) {
    g_free(enc->kernel_name);
    enc->kernel_name = NULL;
  }
}

#if 0
static void
gst_ivas_xvcuenc_class_init (GstIvasXVCUEncClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoEncoderClass *enc_class = GST_VIDEO_ENCODER_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &sink_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Xilinx VCU H264/H265 encoder", "Encoder/Video",
      "Xilinx H264/H265 Encoder", "Xilinx Inc., https://www.xilinx.com");

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_get_property);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_finalize);
  enc_class->open = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_open);
  enc_class->close = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_close);
  enc_class->stop = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_stop);
  enc_class->set_format = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_set_format);
  enc_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_propose_allocation);
  enc_class->finish = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_finish);
  enc_class->handle_frame = GST_DEBUG_FUNCPTR (gst_ivas_xvcuenc_handle_frame);

  g_object_class_install_property (gobject_class, PROP_XCLBIN_LOCATION,
      g_param_spec_string ("xclbin-location", "xclbin file location",
          "Location of the xclbin to program device", NULL,
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

  /* softkernel current index = sk-start-idx + instance_number */
  g_object_class_install_property (gobject_class, PROP_SK_CURRENT_INDEX,
      g_param_spec_int ("sk-cur-idx", "Current softkernel index",
          "Current softkernel index", 0, 63, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("dev-idx", "Device index",
          "FPGA Device index", 0, 31, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifdef ENABLE_XRM_SUPPORT
  g_object_class_install_property (gobject_class, PROP_RESERVATION_ID,
      g_param_spec_int ("reservation-id", "XRM reservation id",
          "Resource Pool Reservation id", 0, 1024, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  /*******************************************************
   * VCU Encoder specific parameters
   *******************************************************/

  /* VCU AspectRatio */
  g_object_class_install_property (gobject_class, PROP_ASPECT_RATIO,
      g_param_spec_enum ("aspect-ratio", "Aspect ratio",
          "Display aspect ratio of the video sequence to be written in SPS/VUI",
          GST_TYPE_IVAS_VIDEO_ENC_ASPECT_RATIO,
          GST_IVAS_VIDEO_ENC_ASPECT_RATIO_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU Gop.NumB */
  g_object_class_install_property (gobject_class, PROP_B_FRAMES,
      g_param_spec_uint ("b-frames", "Number of B-frames",
          "Number of B-frames between two consecutive P-frames",
          0, G_MAXUINT, GST_IVAS_VIDEO_ENC_B_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* VCU ConstrainedIntraPred */
  g_object_class_install_property (gobject_class,
      PROP_CONSTRAINED_INTRA_PREDICTION,
      g_param_spec_boolean ("constrained-intra-prediction",
          "Constrained Intra Prediction",
          "If enabled, prediction only uses residual data and decoded samples "
          "from neighbouring coding blocks coded using intra prediction modes",
          GST_IVAS_VIDEO_ENC_CONSTRAINED_INTRA_PREDICTION_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU RateCtrlMode */
  g_object_class_install_property (gobject_class, PROP_CONTROL_RATE,
      g_param_spec_enum ("control-rate", "Control Rate",
          "Bitrate control method", GST_TYPE_IVAS_VIDEO_ENC_CONTROL_RATE,
          GST_IVAS_VIDEO_ENC_CONTROL_RATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU RateCtrlMode */
  g_object_class_install_property (gobject_class, PROP_CPB_SIZE,
      g_param_spec_uint ("cpb-size", "CPB size",
          "Coded Picture Buffer as specified in the HRD model in msec. "
          "Not used when control-rate=disable",
          0, G_MAXUINT, GST_IVAS_VIDEO_ENC_CPB_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU EntropyMode (only for H.264) */
  g_object_class_install_property (gobject_class, PROP_ENTROPY_MODE,
      g_param_spec_enum ("entropy-mode", "H264 Entropy Mode",
          "Entropy mode for encoding process (only in H264)",
          GST_TYPE_IVAS_ENC_ENTROPY_MODE,
          GST_IVAS_VIDEO_ENC_ENTROPY_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU EnableFillerData */
  g_object_class_install_property (gobject_class, PROP_FILLER_DATA,
      g_param_spec_boolean ("filler-data", "Filler Data",
          "Enable/Disable Filler Data NAL units for CBR rate control",
          GST_IVAS_VIDEO_ENC_FILLER_DATA_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU Gop.GdrMode */
  g_object_class_install_property (gobject_class, PROP_GDR_MODE,
      g_param_spec_enum ("gdr-mode", "GDR mode",
          "Gradual Decoder Refresh scheme mode. Only used if gop-mode=low-delay-p",
          GST_TYPE_IVAS_VIDEO_ENC_GDR_MODE,
          GST_IVAS_VIDEO_ENC_GDR_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU Gop.Length */
  g_object_class_install_property (gobject_class, PROP_GOP_LENGTH,
      g_param_spec_uint ("gop-length", "Gop Length",
          "Number of all frames in 1 GOP, Must be in multiple of (b-frames+1), "
          "Distance between two consecutive I frames", 0, 1000,
          GST_IVAS_VIDEO_ENC_GOP_LENGTH_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  /* VCU GopCtrlMode */
  g_object_class_install_property (gobject_class, PROP_GOP_MODE,
      g_param_spec_enum ("gop-mode", "GOP mode",
          "Group Of Pictures mode",
          GST_TYPE_IVAS_VIDEO_ENC_GOP_MODE,
          GST_IVAS_VIDEO_ENC_GOP_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU GopCtrlMode */
  g_object_class_install_property (gobject_class, PROP_INITIAL_DELAY,
      g_param_spec_uint ("initial-delay", "Initial Delay",
          "The initial removal delay as specified in the HRD model in msec. "
          "Not used when control-rate=disable",
          0, G_MAXUINT, GST_IVAS_VIDEO_ENC_INITIAL_DELAY_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU LoopFilter */
  g_object_class_install_property (gobject_class, PROP_LOOP_FILTER_MODE,
      g_param_spec_enum ("loop-filter-mode", "Loop Filter mode",
          "Enable or disable the deblocking filter",
          GST_TYPE_IVAS_ENC_LOOP_FILTER_MODE,
          GST_IVAS_VIDEO_ENC_LOOP_FILTER_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU MaxBitRate */
  g_object_class_install_property (gobject_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate",
          "Max bitrate in Kbps, only used if control-rate=variable",
          0, G_MAXUINT, GST_IVAS_VIDEO_ENC_MAX_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU MaxQP */
  g_object_class_install_property (gobject_class, PROP_MAX_QP,
      g_param_spec_uint ("max-qp", "max Quantization value",
          "Maximum QP value allowed for the rate control",
          0, 51, GST_IVAS_VIDEO_ENC_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU MinQP */
  g_object_class_install_property (gobject_class, PROP_MIN_QP,
      g_param_spec_uint ("min-qp", "min Quantization value",
          "Minimum QP value allowed for the rate control",
          0, 51, GST_IVAS_VIDEO_ENC_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU NumSlices */
  g_object_class_install_property (gobject_class, PROP_NUM_SLICES,
      g_param_spec_uint ("num-slices", "Number of slices",
          "Number of slices produced for each frame. Each slice contains one or more complete macroblock/CTU row(s). "
          "Slices are distributed over the frame as regularly as possible. If slice-size is defined as well more slices "
          "may be produced to fit the slice-size requirement. "
          "In low-latency mode  H.264(AVC): 32,  H.265 (HEVC): 22 and "
          "In normal latency-mode H.264(AVC): picture_height/16, H.265(HEVC): "
          "minimum of picture_height/32",
          1, G_MAXUINT, GST_IVAS_VIDEO_ENC_NUM_SLICES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU Gop.FreqIDR */
  g_object_class_install_property (gobject_class, PROP_IDR_PERIODICITY,
      g_param_spec_uint ("periodicity-idr", "IDR periodicity",
          "Periodicity of IDR frames",
          0, G_MAXUINT, GST_IVAS_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU CacheLevel2 */
  g_object_class_install_property (gobject_class, PROP_PREFETCH_BUFFER,
      g_param_spec_boolean ("prefetch-buffer", "L2Cache buffer",
          "Enable/Disable L2Cache buffer in encoding process",
          GST_IVAS_VIDEO_ENC_PREFETCH_BUFFER_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU QpCtrlMode */
  g_object_class_install_property (gobject_class, PROP_QP_MODE,
      g_param_spec_enum ("qp-mode", "QP mode",
          "QP control mode used by the VCU encoder",
          GST_TYPE_IVAS_VIDEO_ENC_QP_MODE, GST_IVAS_VIDEO_ENC_QP_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU ScalingList */
  g_object_class_install_property (gobject_class, PROP_SCALING_LIST,
      g_param_spec_enum ("scaling-list", "Scaling List",
          "Scaling list mode",
          GST_TYPE_IVAS_VIDEO_ENC_SCALING_LIST,
          GST_IVAS_VIDEO_ENC_SCALING_LIST_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU SliceQP */
  g_object_class_install_property (gobject_class, PROP_SLICE_QP,
      g_param_spec_int ("slice-qp", "Quantization parameter",
          "When RateCtrlMode = CONST_QP the specified QP is applied to all "
          "slices. When RateCtrlMode = CBR the specified QP is used as initial QP",
          -1, 51, GST_IVAS_VIDEO_ENC_SLICE_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU SliceSize */
  g_object_class_install_property (gobject_class, PROP_SLICE_SIZE,
      g_param_spec_uint ("slice-size", "Target slice size",
          "Target slice size (in bytes) that the encoder uses to automatically "
          " split the bitstream into approximately equally-sized slices",
          0, 65535, GST_IVAS_VIDEO_ENC_SLICE_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /* VCU BitRate */
  g_object_class_install_property (gobject_class, PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate", "Target Bitrate",
          "Target bitrate in Kbps (64Kbps = component default)",
          0, G_MAXUINT, GST_IVAS_VIDEO_ENC_TARGET_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  /* VCU Encoder NumCore */
  g_object_class_install_property (gobject_class, PROP_NUM_CORES,
      g_param_spec_uint ("num-cores", "Number of cores",
          "Number of Encoder Cores to be used for current Stream. There are 4 "
          "Encoder cores. Value  0 => AUTO, VCU Encoder will autometically decide the"
          " number of cores for the current stream."
          " Value 1 to 4 => number of cores to be used",
          0, 4, 0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* VCU Enable RateControl Mode */
  g_object_class_install_property (gobject_class, PROP_RATE_CONTROL_MODE,
      g_param_spec_boolean ("rate-control-mode", "Rate Control mode",
          "VCU Custom rate control mode",
          GST_IVAS_VIDEO_ENC_RC_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_KERNEL_NAME,
      g_param_spec_string ("kernel-name", "VCU Decoder kernel name",
          "VCU Decoder kernel name", GST_IVAS_VIDEO_ENC_KERNEL_NAME_DEFAULT,
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY));

  GST_DEBUG_CATEGORY_INIT (gst_ivas_xvcuenc_debug_category, "ivas_xvcuenc", 0,
      "debug category for vcu h264/h265 encoder element");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");
}

#endif

static void
gst_ivas_xvcuenc_init (GstIvasXVCUEnc * enc)
{
  GstIvasXVCUEncPrivate *priv = GST_IVAS_XVCUENC_PRIVATE (enc);
  enc->priv = priv;
  enc->sk_start_idx = -1;
  enc->codec_type = XLNX_CODEC_INVALID;
  enc->dev_index = 0;           /* default dev index is 0 */
  enc->kernel_name = g_strdup(GST_IVAS_VIDEO_ENC_KERNEL_NAME_DEFAULT);

  enc->control_rate = GST_IVAS_VIDEO_ENC_CONTROL_RATE_DEFAULT;
  enc->target_bitrate = GST_IVAS_VIDEO_ENC_TARGET_BITRATE_DEFAULT;
  enc->qp_mode = GST_IVAS_VIDEO_ENC_QP_MODE_DEFAULT;
  enc->min_qp = GST_IVAS_VIDEO_ENC_MIN_QP_DEFAULT;
  enc->max_qp = GST_IVAS_VIDEO_ENC_MAX_QP_DEFAULT;
  enc->gop_mode = GST_IVAS_VIDEO_ENC_GOP_MODE_DEFAULT;
  enc->gdr_mode = GST_IVAS_VIDEO_ENC_GDR_MODE_DEFAULT;
  enc->initial_delay = GST_IVAS_VIDEO_ENC_INITIAL_DELAY_DEFAULT;
  enc->cpb_size = GST_IVAS_VIDEO_ENC_CPB_SIZE_DEFAULT;
  enc->scaling_list = GST_IVAS_VIDEO_ENC_SCALING_LIST_DEFAULT;
  enc->max_bitrate = GST_IVAS_VIDEO_ENC_MAX_BITRATE_DEFAULT;
  enc->aspect_ratio = GST_IVAS_VIDEO_ENC_ASPECT_RATIO_DEFAULT;
  enc->filler_data = GST_IVAS_VIDEO_ENC_FILLER_DATA_DEFAULT;
  enc->num_slices = GST_IVAS_VIDEO_ENC_NUM_SLICES_DEFAULT;
  enc->slice_size = GST_IVAS_VIDEO_ENC_SLICE_SIZE_DEFAULT;
  enc->prefetch_buffer = GST_IVAS_VIDEO_ENC_PREFETCH_BUFFER_DEFAULT;
  enc->periodicity_idr = GST_IVAS_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT;
  enc->b_frames = GST_IVAS_VIDEO_ENC_B_FRAMES_DEFAULT;
  enc->gop_length = GST_IVAS_VIDEO_ENC_GOP_LENGTH_DEFAULT;
  enc->entropy_mode = GST_IVAS_VIDEO_ENC_ENTROPY_MODE_DEFAULT;
  enc->slice_qp = GST_IVAS_VIDEO_ENC_SLICE_QP_DEFAULT;
  enc->constrained_intra_prediction =
      GST_IVAS_VIDEO_ENC_CONSTRAINED_INTRA_PREDICTION_DEFAULT;
  enc->loop_filter_mode = GST_IVAS_VIDEO_ENC_LOOP_FILTER_MODE_DEFAULT;
  enc->level = "5.1";
  enc->tier = "MAIN_TIER";
  enc->num_cores = 0;
  enc->rc_mode = GST_IVAS_VIDEO_ENC_RC_MODE_DEFAULT;
  priv->use_inpool = FALSE;
  priv->validate_import = TRUE;
  priv->in_idx_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->in_buf_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (void (*)(void *)) gst_buffer_unref);
  priv->out_xrt_bufs = g_array_new (FALSE, TRUE, sizeof (xrt_buffer *));
  priv->qp_xrt_bufs = g_array_new (FALSE, TRUE, sizeof (xrt_buffer *));
  priv->num_in_idx = 0;
  priv->init_done = FALSE;
  priv->deinit_done = FALSE;
  priv->flush_done = FALSE;
  priv->read_oidx_list = NULL;
  priv->cur_qp_idx = 0;
  priv->qpbuf_count = 0;
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

  /* Create local memory for main encoder structure */
  enc = (XrtIvas_XVCUEnc *) calloc (1, sizeof (XrtIvas_XVCUEnc));
  if (!enc) {
    ERROR_PRINT ("failed to allocate encoder memory\n");
    goto error;
  }

  /* Get the input buffer array from the file */
  in_buffer = (uint8_t *)&input_frame;
  in_size = sizeof (input_frame);
  enc->input_buf_size = in_size;

  /* Initialize xrt and open device */
  iret = xvcuenc (enc, xclbin_path, sk_idx, dev_idx);
  if (iret == FALSE) {
    ERROR_PRINT ("xrt VCU encoder initialization failed!!\n");
    goto error;
  }

  /* Initialize and Configure encoder device */
  iret = xvcuenc_set_format (enc);
  if (iret == FALSE) {
    ERROR_PRINT ("xrt VCU encoder configuration failed!!\n");
    goto error;
  }

  DEBUG_PRINT ("Decoder initialization is done Succesfully\n");

  /* Prepare and send the input frame buffer */
  fret = gstivas_xvcuenc_handle_frame (enc, (uint8_t *)in_buffer, in_size);
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
  fret = gstivas_xvcuenc_finish(enc, (uint8_t *)out_buffer, &out_size);
  if (fret != XRT_FLOW_OK) {
    ERROR_PRINT ("VCU receive command failed!!\n");
    goto error;
  }

  /* Response Received. Stop the device and cleanup */
  iret = gstivas_xvcuenc_stop(enc);
  if (iret == FALSE) {
    ERROR_PRINT ("VCU encoder stop failed!!\n");
    goto error;
  }

  gstivas_xvcuenc_close(enc);

  /* Validate Results */
  iret = xrt_validate_output(out_buffer, out_size);
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



#if 0
static gboolean
vcu_enc_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "ivas_xvcuenc", GST_RANK_NONE,
      GST_TYPE_IVAS_XVCUENC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ivas_xvcuenc,
    "Xilinx VCU H264/H264 Encoder plugin",
    vcu_enc_init, "0.1", GST_LICENSE_UNKNOWN, "GStreamer Xilinx",
    "http://xilinx.com/")

#endif
