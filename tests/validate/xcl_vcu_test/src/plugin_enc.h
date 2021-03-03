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

#ifndef _GST_IVAS_XVCUENC_H_
#define _GST_IVAS_XVCUENC_H_

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

G_BEGIN_DECLS

#define GST_TYPE_IVAS_XVCUENC          (gst_ivas_xvcuenc_get_type())
#define GST_IVAS_XVCUENC(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IVAS_XVCUENC,GstIvasXVCUEnc))
#define GST_IVAS_XVCUENC_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IVAS_XVCUENC,GstIvasXVCUEncClass))
#define GST_IS_IVAS_XVCUENC(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IVAS_XVCUENC))
#define GST_IS_IVAS_XVCUENC_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IVAS_XVCUENC))

typedef struct _GstIvasXVCUEnc GstIvasXVCUEnc;
typedef struct _GstIvasXVCUEncClass GstIvasXVCUEncClass;
typedef struct _GstIvasXVCUEncPrivate GstIvasXVCUEncPrivate;

typedef enum _xlnx_codec_type{
  XLNX_CODEC_H264,
  XLNX_CODEC_H265,
  XLNX_CODEC_INVALID,
} XlnxCodecType;

struct _GstIvasXVCUEnc
{
  GstVideoEncoder parent;
  GstIvasXVCUEncPrivate *priv;
  XlnxCodecType codec_type;
  GstVideoCodecState *input_state;
  GstVideoInfo out_vinfo;
  const gchar *profile;
  const gchar *level;
  /*only for H.265*/
  const gchar *tier;

  /* properties */
  gchar *xclbin_path;
  gchar *sk_name;
  gchar *sk_lib_path;
  gint sk_start_idx;
  gint sk_cur_idx;
  gint dev_index;
  guint32 control_rate;
  guint32 target_bitrate;
  gint32 slice_qp;
  guint32 qp_mode;
  guint32 min_qp;
  guint32 max_qp;
  guint32 gop_mode;
  guint32 gdr_mode;
  guint32 initial_delay;
  guint32 cpb_size;
  guint32 scaling_list;
  guint32 max_bitrate;
  guint32 aspect_ratio;
  gboolean filler_data;
  guint32 num_slices;
  guint32 slice_size;
  gboolean prefetch_buffer;
  guint32 periodicity_idr;
  guint32 b_frames;
  gboolean constrained_intra_prediction;
  guint32 loop_filter_mode;
  guint32 gop_length;
  /*only for H.264*/
  guint32 entropy_mode;
  guint32 num_cores;
  gboolean rc_mode;
  gchar *kernel_name;
};

struct _GstIvasXVCUEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_ivas_xvcuenc_get_type(void);

G_BEGIN_DECLS
#endif /* _GST_IVAS_XVCUENC_H_ */
