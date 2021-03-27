#ifndef _XRT_XVCUDEC_H_
#define _XRT_XVCUDEC_H_

#include <stdint.h>
#include "plugin_common.h"
#include "xrt_utils.h"

extern unsigned char decoder_input_buf[];
extern unsigned int decoder_input_buf_len;
extern unsigned char decoder_output_buf[];
extern unsigned int decoder_output_buf_len;

typedef struct _XlnxOutputBuffer
{
  uint32_t idx;
  xrt_buffer xrt_buf;
} XlnxOutputBuffer;

typedef struct _vcu_dec_usermeta
{
  int64_t pts;
} vcu_dec_usermeta;

typedef struct _out_buf_info
{
  uint64_t freed_obuf_paddr;
  size_t freed_obuf_size;
  uint32_t freed_obuf_index;
} out_buf_info;

typedef struct dec_params
{
  uint32_t bitdepth;
  uint32_t codec_type;
  uint32_t low_latency;
  uint32_t entropy_buffers_count;
  uint32_t frame_rate;
  uint32_t clk_ratio;
  uint32_t profile;
  uint32_t level;
  uint32_t height;
  uint32_t width;
  uint32_t chroma_mode;
  uint32_t scan_type;
} dec_params_t;

typedef struct _sk_payload_data
{
  uint32_t cmd_id;
  uint32_t cmd_rsp;
  uint32_t obuff_size;
  uint32_t obuff_num;
  uint32_t obuff_index[FRM_BUF_POOL_SIZE];
  uint32_t ibuff_valid_size;
  uint32_t host_to_dev_ibuf_idx;
  uint32_t dev_to_host_ibuf_idx;
  bool last_ibuf_copied;
  bool resolution_found;
  vcu_dec_usermeta ibuff_meta;
  vcu_dec_usermeta obuff_meta[FRM_BUF_POOL_SIZE];
  bool end_decoding;
  uint32_t free_index_cnt;
  int valid_oidxs;
  out_buf_info obuf_info[MAX_OUT_INFOS];
} sk_payload_data;

typedef struct _XrtIvas_XVCUDecPrivate
{
  xclDeviceHandle xcl_handle;
  uuid_t xclbinId;
  xrt_buffer **out_bufs_arr;
  xrt_buffer *ert_cmd_buf;
  xrt_buffer *sk_payload_buf;
  xrt_buffer *in_xrt_bufs[MAX_IBUFFS];  /* input encoded stream will be copied to this */
  xrt_buffer *dec_cfg_buf;
  xrt_buffer *dec_out_bufs_handle;
  uint64_t timestamp; /* get current time when sending PREINIT command */
  int init_done;
  int flush_done;          /* to make sure FLUSH cmd issued to softkernel while exiting */
  int deinit_done;
  uint32_t num_out_bufs;
  size_t out_buf_size;
  uint32_t max_ibuf_size;
  uint32_t host_to_dev_ibuf_idx;
  sk_payload_data last_rcvd_payload;
  uint32_t last_rcvd_oidx;
} XrtIvas_XVCUDecPrivate;

typedef struct _XrtIvas_XVCUDec
{
  XrtIvas_XVCUDecPrivate *priv;
  uint32_t input_buf_size;

  /* properties */
  char *xclbin_path;
  char *sk_name;
  char *sk_lib_path;
  int low_latency;
  uint32_t num_entropy_bufs;
  uint32_t bit_depth;
  int sk_start_idx;
  int sk_cur_idx;
  int dev_index;
} XrtIvas_XVCUDec;

#endif
