/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style CMA backed memory manager for ZynQ based OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Min Ma       <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef __ZYNQ_HWCTX_H__
#define __ZYNQ_HWCTX_H__

#ifndef __KERNEL__
#include <stdint.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#else
#include <uapi/drm/drm_mode.h>
#endif /* !__KERNEL__ */

#define CU_NAME_MAX_LEN   64

/**
 * struct drm_zocl_create_hw_ctx - Create a hw context on a slot on device
 * used with DRM_ZOCL_CREATE_HW_CTX ioctl
 *
 * @uuid_ptr:      uuid pointer which need to download
 * @uuid_size:     uuild pointer size that need to download
 * @qos:           QOS information
 * @hw_context:    Returns Context handle
 */
struct drm_zocl_create_hw_ctx {
        struct drm_zocl_axlf	*axlf_obj;
	uint32_t                qos;

        // return context id
        uint32_t                hw_context;
};

/**
 * struct drm_zocl_destroy_hw_ctx - Close/Destroy a hw context on a slot on device
 * used with DRM_ZOCL_DESTROY_HW_CTX ioctl
 *
 * @hw_context:    Context handle which need to close
 */
struct drm_zocl_destroy_hw_ctx {
        uint32_t        hw_context;
};

/**
 * struct drm_zocl_open_cu_ctx - Open a cu context under a hw context on device
 * used with DRM_ZOCL_OPEN_CU_CTX ioctl
 *
 * @hw_context:    Open CU under this hw Context handle
 * @cu_name:       Name of the compute unit in the device image for which
 *                 the open request is being made
 * @flags:         Shared or exclusive context (ZOCL_CTX_SHARED/ZOCL_CTX_EXCLUSIVE)
 * @cu_index:      Return the acquired CU index. This will require for close cu context
 */
struct drm_zocl_open_cu_ctx {
        // Under this hw context id
        uint32_t        hw_context;
        char            cu_name[CU_NAME_MAX_LEN];
        uint32_t        flags;

        // Return the acquired CU index.
        uint32_t        cu_index;
};

/**
 * struct drm_zocl_close_cu_ctx - Open a cu context under a hw context on device
 * used with DRM_ZOCL_CLOSE_CU_CTX ioctl
 *
 * @hw_context:    Open CU under this hw Context handle
 * @cu_index:      Index of the compute unit in the device image for which
 *                 the close request is being made
 */
struct drm_zocl_close_cu_ctx {
        // Under this hw context id
        uint32_t        hw_context;
        uint32_t        cu_index;
};

/**
 * struct drm_zocl_open_aie_ctx - Open a cu context under a hw context on device
 * used with DRM_ZOCL_OPEN_AIE_CTX ioctl
 *
 * @hw_context:    Open AIE under this hw Context handle
 * @cu_name:       Name of the compute unit in the device image for which
 *                 the open request is being made
 * @flags:         Shared or exclusive context (ZOCL_CTX_SHARED/ZOCL_CTX_EXCLUSIVE)
 * @cu_index:      Return the acquired CU index. This will require for close cu context
 */
struct drm_zocl_open_aie_ctx {
        // Under this hw context id
        uint32_t        hw_context;
        uint32_t        flags;

        // Return the acquired CU index.
        uint32_t        cu_index;
};

/**
 * struct drm_zocl_close_aie_ctx - Open a cu context under a hw context on device
 * used with DRM_ZOCL_CLOSE_AIE_CTX ioctl
 *
 * @hw_context:    Open AIE under this hw Context handle
 * @cu_index:      Index of the compute unit in the device image for which
 *                 the close request is being made
 */
struct drm_zocl_close_aie_ctx {
        // Under this hw context id
        uint32_t        hw_context;
        uint32_t        cu_index;
};


/**
 * struct drm_zocl_open_graph_ctx - Open a cu context under a hw context on device
 * used with DRM_ZOCL_OPEN_GRAPH_CTX ioctl
 *
 * @hw_context:    Open GRAPH under this hw Context handle
 * @cu_name:       Name of the compute unit in the device image for which
 *                 the open request is being made
 * @flags:         Shared or exclusive context (ZOCL_CTX_SHARED/ZOCL_CTX_EXCLUSIVE)
 * @cu_index:      Return the acquired CU index. This will require for close cu context
 */
struct drm_zocl_open_graph_ctx {
        // Under this hw context id
        uint32_t        hw_context;
        char            cu_name[CU_NAME_MAX_LEN];
        uint32_t        flags;

        // Return the acquired CU index.
        uint32_t        cu_index;
};

/**
 * struct drm_zocl_close_graph_ctx - Open a cu context under a hw context on device
 * used with DRM_ZOCL_CLOSE_GRAPH_CTX ioctl
 *
 * @hw_context:    Open CU under this hw Context handle
 * @cu_index:      Index of the compute unit in the device image for which
 *                 the close request is being made
 */
struct drm_zocl_close_graph_ctx {
        // Under this hw context id
        uint32_t        hw_context;
        uint32_t        cu_index;
};

/**
 * struct drm_zocl_hw_ctx_execbuf - Submit a command buffer for execution on a compute
 * unit  used with DRM_ZOCL_HW_CTX_EXECBUF ioctl
 *
 * @hw_ctx_id:      Pass the hw context id
 * @exec_bo_handle: BO handle of command buffer formatted as ERT command
 */
struct drm_zocl_hw_ctx_execbuf {
  uint32_t hw_ctx_id;
  uint32_t exec_bo_handle;
};

#endif
