#ifndef __DUMMY_PLUGIN_DEC_H__
#define __DUMMY_PLUGIN_DEC_H__

#include <stdint.h>
#include "xrt_utils.h"

#define XCLBIN_PATH "/usr/local/lib/aws.xclbin"
static const int ERT_CMD_SIZE = 4096;
#define int int 
#define TRUE 0
#define FALSE -1
#define MAX_IBUFFS 2
#define MEM_BANK 0
#define OUT_MEM_SIZE 3342336 // GST Specific

#define ERT_CMD_DATA_LEN 1024
#define CMD_EXEC_TIMEOUT 1000
#define FRM_BUF_POOL_SIZE 50
#define MAX_OUT_INFOS 25

enum cmd_type
{
  VCU_PREINIT = 0,
  VCU_INIT,
  VCU_PUSH,
  VCU_RECEIVE,
  VCU_FLUSH,
  VCU_DEINIT,
};

typedef enum _XrtFlowReturn
{
  XRT_FLOW_OK = 0,
  XRT_FLOW_EOS,
  XRT_FLOW_ERROR,
} XrtFlowReturn;

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

int vcu_dec_test(const char *xclbin_path, int sk_idx, int dev_idx);

#endif
