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

#ifndef _XRT_XVCUENC_H_
#define _XRT_XVCUENC_H_

#include <stdint.h>
#include "plugin_common.h"
#include "xrt_utils.h"

extern unsigned char encoder_output_buf[];
extern unsigned int encoder_output_buf_len;;
extern unsigned char decoder_output_buf[];
extern unsigned int decoder_output_buf_len;

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
  uint32_t la_depth;
} sk_payload_data;

typedef struct _XrtIvas_XVCUEncPrivate
{
  int use_inpool;
  int validate_import;
  xclDeviceHandle xcl_handle;
  uuid_t xclbinId;
  int cu_idx;
  xrt_buffer *in_xrt_bufs;
  xrt_buffer *ert_cmd_buf;
  xrt_buffer *sk_payload_buf;
  xrt_buffer *static_cfg_buf;
  xrt_buffer *dyn_cfg_buf;
  xrt_buffer **out_xrt_bufs;
  xrt_buffer *out_bufs_handle;
  xrt_buffer *qp_bufs_handle;
  int num_out_bufs;
  int num_in_idx;
  int init_done;
  int flush_done;          /* to make sure FLUSH cmd issued to softkernel while exiting */
  int deinit_done;         // TODO: instead maintain state of softkernel
  uint32_t min_num_inbufs;
  uint32_t in_buf_size;
  uint32_t cur_qp_idx;
  uint32_t qpbuf_count;
  sk_payload_data last_rcvd_payload;
  uint32_t last_rcvd_oidx;
  uint64_t timestamp; /* get current time when sending PREINIT command */
} XrtIvas_XVCUEncPrivate;

typedef struct _XrtIvas_XVCUEnc
{
  XrtIvas_XVCUEncPrivate *priv;
  XlnxCodecType codec_type;
  const char *profile;
  const char *level;
  /*only for H.265*/
  const char *tier;

  /* properties */
  char *xclbin_path;
  char *sk_name;
  char *sk_lib_path;
  int sk_start_idx;
  int sk_cur_idx;
  int dev_index;
  int input_buf_size;
  uint32_t control_rate;
  uint32_t target_bitrate;
  int32_t slice_qp;
  uint32_t qp_mode;
  uint32_t min_qp;
  uint32_t max_qp;
  uint32_t gop_mode;
  uint32_t gdr_mode;
  uint32_t initial_delay;
  uint32_t cpb_size;
  uint32_t scaling_list;
  uint32_t max_bitrate;
  uint32_t aspect_ratio;
  bool filler_data;
  uint32_t num_slices;
  uint32_t slice_size;
  bool prefetch_buffer;
  uint32_t periodicity_idr;
  uint32_t b_frames;
  bool constrained_intra_prediction;
  uint32_t loop_filter_mode;
  uint32_t gop_length;
  /*only for H.264*/
  uint32_t entropy_mode;
  uint32_t num_cores;
  bool rc_mode;
  char *kernel_name;
} XrtIvas_XVCUEnc;

#endif /* _GST_IVAS_XVCUENC_H_ */
