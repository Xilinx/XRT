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

/* Update of this file by the user is not encouraged */
#include <assert.h>
#include "xrt_utils.h"

#define ERROR_PRINT(...) {\
  do {\
    printf("[%s:%d] ERROR : ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}

#undef DEBUG_XRT_UTILS

#ifdef DEBUG_XRT_UTILS
#define DEBUG_PRINT(...) {\
  do {\
    printf("[%s:%d] ",__func__, __LINE__);\
    printf(__VA_ARGS__);\
    printf("\n");\
  } while(0);\
}
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

struct sk_device_info
{
  int pid;
  void *device_handle;
  uint32_t dev_index;
};

#define MAX_DEVICES   (32)
#define P2ROUNDUP(x, align) (-(-(x) & -(align)))
#define WAIT_TIMEOUT 1000       // 1sec
#define ERT_START_CMD_PAYLOAD_SIZE ((1024 * sizeof(unsigned int)) - 2)  // ERT cmd can carry max 4088 bytes
#define MEM_BANK 0


int
alloc_xrt_buffer (xclDeviceHandle handle, unsigned int size,
    enum xclBOKind bo_kind, unsigned flags, xrt_buffer * buffer)
{
  if (handle == NULL || size == 0 || buffer == NULL) {
    ERROR_PRINT ("invalid arguments");
    return -1;
  }

  buffer->bo = xclAllocBO (handle, size, bo_kind, flags);
  if (buffer->bo == NULLBO) {
    ERROR_PRINT ("failed to allocate Device BO...");
    return -1;
  }

  buffer->user_ptr = xclMapBO (handle, buffer->bo, true);
  if (buffer->user_ptr == NULL) {
    ERROR_PRINT ("failed to map BO...");
    xclFreeBO (handle, buffer->bo);
    return -1;
  }

  if (bo_kind != XCL_BO_SHARED_VIRTUAL) {
    struct xclBOProperties p;
    if (xclGetBOProperties (handle, buffer->bo, &p)) {
      ERROR_PRINT ("failed to get physical address...");
      xclUnmapBO (handle, buffer->bo, buffer->user_ptr);
      buffer->user_ptr = NULL;
      xclFreeBO (handle, buffer->bo);
      return -1;
    }
    buffer->phy_addr = p.paddr;
  }
  buffer->size = size;
  return 0;
}

void
free_xrt_buffer (xclDeviceHandle handle, xrt_buffer * buffer)
{
  if (handle == NULL || buffer == NULL) {
    ERROR_PRINT ("invalid arguments");
    return;
  }
  if (buffer->user_ptr && buffer->size)
    xclUnmapBO (handle, buffer->bo, buffer->user_ptr);
  if (handle && buffer->bo > 0)
    xclFreeBO (handle, buffer->bo);
  memset (buffer, 0x00, sizeof (xrt_buffer));
}


static const struct axlf_section_header *
get_axlf_section2 (const struct axlf *top, enum axlf_section_kind kind)
{
  uint32_t i = 0;
  DEBUG_PRINT ("Finding section header for axlf section %d", kind);
  for (i = 0; i < top->m_header.m_numSections; i++) {
    DEBUG_PRINT ("Section is %d", top->m_sections[i].m_sectionKind);
    if (top->m_sections[i].m_sectionKind == kind) {
      DEBUG_PRINT ("Found section header for kind %d", kind);
      return &top->m_sections[i];
    }
  }
  ERROR_PRINT ("Section header for axlf section %d not found", kind);
  return NULL;
}

int
download_xclbin (const char *bit, unsigned deviceIndex, const char *halLog,
    xclDeviceHandle * handle, uuid_t * xclbinId)
{
  struct xclDeviceInfo2 deviceInfo;
  FILE *fptr = NULL;
  size_t size = 0;
  char *header = NULL;
  const xclBin *blob = NULL;
  const struct axlf *top = NULL;
  const struct axlf_section_header *ip = NULL;
  struct ip_layout *layout = NULL;
  int i;

  if (deviceIndex >= xclProbe ()) {
    ERROR_PRINT ("Device index not found");
    return -1;
  }

  *handle = xclOpen (deviceIndex, halLog, XCL_INFO);

  if (xclGetDeviceInfo2 (*handle, &deviceInfo)) {
    ERROR_PRINT ("Device information not found");
    goto error;
  }

  DEBUG_PRINT ("DSA = %s", deviceInfo.mName);
  DEBUG_PRINT ("Index = %d", deviceIndex);
  DEBUG_PRINT ("PCIe = GEN%ux%u", deviceInfo.mPCIeLinkSpeed,
      deviceInfo.mPCIeLinkWidth);
  DEBUG_PRINT ("OCL Frequency = %u", deviceInfo.mOCLFrequency[0]);
  DEBUG_PRINT ("DDR Bank = %u", deviceInfo.mDDRBankCount);
  DEBUG_PRINT ("Device Temp = %u", deviceInfo.mOnChipTemp);
  DEBUG_PRINT ("MIG Calibration = %u", deviceInfo.mMigCalib);

  if (!bit || !strlen (bit))
    return 0;

  fptr = fopen (bit, "rb");
  if (!fptr) {
    ERROR_PRINT ("fopen() with %s failed due to %s", bit, strerror (errno));
    goto error;
  }

  if (fseek (fptr, 0, SEEK_END) != 0) {
    ERROR_PRINT ("fseek() failed to SEEK_END with %s", strerror (errno));
    goto error;
  }

  size = ftell (fptr);
  if (size == (size_t) (-1)) {
    ERROR_PRINT ("ftell failed with %s", strerror (errno));
    goto error;
  }

  if (fseek (fptr, 0, SEEK_SET) != 0) {
    ERROR_PRINT ("fseek() failed to SEEK_SET with %s", strerror (errno));
    goto error;
  }

  header = (char *) calloc (1, size);
  if (header == NULL) {
    ERROR_PRINT ("failed to allocate memory");
    return -1;
  }

  if (fread (header, sizeof (char), size, fptr) != size) {
    ERROR_PRINT ("fread failed with %s", strerror (errno));
    goto error;
  }

  if (strncmp (header, "xclbin2", 8)) {
    ERROR_PRINT ("Invalid bitstream xclbin2 tag not present");
    goto error;
  }

  blob = (const xclBin *) header;
  if (xclLoadXclBin (*handle, blob)) {
    ERROR_PRINT ("Bitstream download failed");
    goto error;
  }
  DEBUG_PRINT ("Finished downloading bitstream %s ", bit);

  top = (const struct axlf *) header;
  ip = get_axlf_section2 (top, IP_LAYOUT);
  layout = (struct ip_layout *) (header + ip->m_sectionOffset);

  for (i = 0; i < layout->m_count; ++i) {
    if (layout->m_ip_data[i].m_type != IP_KERNEL)
      continue;
    DEBUG_PRINT ("index = %d, kernel name = %s, base_addr = %lx", i,
        layout->m_ip_data[i].m_name, layout->m_ip_data[i].m_base_address);
  }

  uuid_copy (*xclbinId, top->m_header.uuid);
  free (header);
  fclose (fptr);
  return 0;

error:
  if (header)
    free (header);
  if (fptr)
    fclose (fptr);
  xclClose (*handle);
  return -1;
}

int
send_softkernel_command (xclDeviceHandle handle, xrt_buffer * sk_ert_buf,
    unsigned int *payload, unsigned int num_idx, unsigned int cu_mask,
    int timeout)
{
  struct ert_start_kernel_cmd *ert_cmd =
      (struct ert_start_kernel_cmd *) (sk_ert_buf->user_ptr);
  int ret = 0;
  int retry_cnt = 0;

  if (NULL == sk_ert_buf || NULL == payload
      || (num_idx * sizeof (unsigned int)) > ERT_START_CMD_PAYLOAD_SIZE
      || !num_idx) {
    ERROR_PRINT
        ("invalid arguments. sk_buf = %p, payload = %p and num idx = %d",
        sk_ert_buf, payload, num_idx);
    return -1;
  }

  ert_cmd->state = ERT_CMD_STATE_NEW;
  ert_cmd->opcode = ERT_SK_START;

  ert_cmd->extra_cu_masks = 3;

  if (cu_mask > 31) {
    ert_cmd->cu_mask = 0;
    if (cu_mask > 63) {
      ert_cmd->data[0] = 0;
      if (cu_mask > 96) {
        ert_cmd->data[1] = 0;
        ert_cmd->data[2] = (1 << (cu_mask - 96));
      } else {
        ert_cmd->data[1] = (1 << (cu_mask - 64));
        ert_cmd->data[2] = 0;
      }
    } else {
      ert_cmd->data[0] = (1 << (cu_mask - 32));
    }
  } else {
    ert_cmd->cu_mask = (1 << cu_mask);
    ert_cmd->data[0] = 0;
    ert_cmd->data[1] = 0;
    ert_cmd->data[2] = 0;
  }


  ert_cmd->count = num_idx + 4;
  memcpy (&ert_cmd->data[3], payload, num_idx * sizeof (unsigned int));

  ret = xclExecBuf (handle, sk_ert_buf->bo);
  if (ret < 0) {
    ERROR_PRINT ("[handle %p & bo %d] ExecBuf failed with ret = %d. reason : %s", handle, sk_ert_buf->bo, ret, strerror (errno));
    return ret;
  }

  do {
    ret = xclExecWait (handle, timeout);
    if (ret < 0) {
      ERROR_PRINT ("ExecWait ret = %d. reason : %s", ret, strerror (errno));
      return ret;
    } else if (!ret) {
      if (retry_cnt++ >= 10) {
        ERROR_PRINT ("[handle %p] ExecWait ret = %d. reason : %s", handle, ret, strerror (errno));
	      return -1;
      }
    }
  } while (ert_cmd->state != ERT_CMD_STATE_COMPLETED);

  return 0;
}
