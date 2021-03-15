#ifndef __PLUGIN_COMMON_H__
#define __PLUGIN_COMMON_H__

extern unsigned char encoder_output_buf[];
extern unsigned int encoder_output_buf_len;;
extern unsigned char decoder_output_buf[];
extern unsigned int decoder_output_buf_len;

static const int ERT_CMD_SIZE = 4096;
#define TRUE 0
#define FALSE -1
#define NOTSUPP 1

#define RETRY_COUNT 100

#define MEM_BANK 0
#define OUT_MEM_SIZE 3342336 // GST Specific

#define ERT_CMD_DATA_LEN 1024
#define CMD_EXEC_TIMEOUT 1000
#define FRM_BUF_POOL_SIZE 50
#define MAX_OUT_INFOS 25
#define MIN_POOL_BUFFERS 1
#define X_MAXUINT -1
#define MAX_IBUFFS 2
#define ENABLE_DMABUF 0
#define WIDTH_ALIGN 256
#define HEIGHT_ALIGN 64
#define ALIGN(size,align) (((size) + (align) - 1) & ~((align) - 1))
#define MAX_OUT_BUFF_COUNT 50

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

typedef enum _xlnx_codec_type{
  XLNX_CODEC_H264,
  XLNX_CODEC_H265,
  XLNX_CODEC_INVALID,
} XlnxCodecType;


//#define DEBUG_VCU
#undef DEBUG_VCU
#define ERROR_PRINT(...) {\
  do {\
    printf("[%s:%d] ERROR : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}

#ifdef DEBUG_VCU
#define INFO_PRINT(...) {\
  do {\
    printf("[%s:%d] INFO : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}

#define DEBUG_PRINT(...) {\
  do {\
    printf("[%s:%d] DEBUG : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}
#else
#define DEBUG_PRINT(...) ((void)0)
#define INFO_PRINT(...) ((void)0)
#endif

int vcu_enc_test(const char *xclbin_path, int sk_idx, int dev_idx);
int vcu_dec_test(const char *xclbin_path, int sk_idx, int dev_idx);

#endif
