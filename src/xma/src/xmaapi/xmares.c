/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

/**
 * @file xmares.c
 * @brief Resource allocation manager code
 *
 * This library implements resource managment between
 * processes and permits device-level sharing
*/

#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/xmaapi.h"
#include "lib/xmacfg.h"
#include "lib/xmares.h"
#include "xmaplugin.h"

#define XMA_RES_MOD "xmares"

enum XmaKernType {
    xma_res_encoder = 1,
    xma_res_scaler,
    xma_res_decoder,
    xma_res_filter,
    xma_res_kernel
};

typedef struct XmaKernReq {
    enum XmaKernType type; /**< kernel type [internal use] */
    char name[NAME_MAX]; /**< unused */
    /* input request parameters */
    char vendor[NAME_MAX]; /**<requested vendor */
    bool dev_excl; /**<request exclusive access to device */
    union {
        XmaEncoderType enc_type;
        XmaScalerType scal_type;
        XmaDecoderType dec_type;
        XmaFilterType filter_type;
        XmaKernelType kernel_type;
    } kernel_spec;  /**<requested kernel */
    /* output parameters */
    int32_t dev_handle; /**< output param */
    int32_t kern_handle; /**< output param */
    int32_t plugin_handle; /**< output param */
    XmaSession *session; /**< associated session object */
} XmaKernReq;

/**
 * Shared memory database structure
*/
typedef struct XmaKernel {
    char name[NAME_MAX];
    char vendor[NAME_MAX];
    char function[NAME_MAX];
    int32_t plugin_handle;
} XmaKernel;

typedef struct XmaImage {
    char name[NAME_MAX];
    uint32_t kernel_cnt;
    XmaKernel kernels[MAX_KERNEL_CONFIGS];
} XmaImage;

typedef struct XmaKernelChan {
    pthread_t thread_id;
    XmaSession *session;
} XmaKernelChan;

typedef struct XmaKernelInstance {
    uint32_t kernel_id;
    pid_t client_id;
    XmaKernelChan channels[MAX_KERNEL_CHANS];
} XmaKernelInstance;

typedef struct XmaDevice {
    bool configured; /**< Indicates xclbin loaded */
    bool excl; /**< device locked for exclusive use */
    bool exists; /**< device exists within system */
    pid_t client_procs[MAX_KERNEL_CONFIGS]; /**< processes using device */
    uint32_t image_id;
    XmaKernelInstance kernels[MAX_KERNEL_CONFIGS];/**< each entry is a kernel instance */
    uint32_t kernel_cnt;
} XmaDevice;

typedef struct XmaShmRes {
    XmaDevice devices[MAX_XILINX_DEVICES];
    XmaImage images[MAX_IMAGE_CONFIGS];
} XmaShmRes;

typedef struct XmaResConfig {
    XmaShmRes sys_res;
    pthread_mutex_t lock;
    pid_t clients[MAX_XILINX_DEVICES * MAX_KERNEL_CONFIGS];
    uint32_t ref_cnt;
} XmaResConfig;

/**********************************GLOBALS*************************************/
#ifdef XMA_RES_TEST
char *XMA_SHM_FILE = "/tmp/xma_shm_db";
char *XMA_SHM_FILE_SIG = "/tmp/xma_shm_db_ready";
bool xma_shm_filename_set = 0;
#endif

/**********************************PROTOTYPES**********************************/
static void xma_set_shm_filenames(void);

static XmaResConfig *xma_shm_open(char *shm_filename, XmaSystemCfg *xma_shm);

static void xma_shm_close(XmaResConfig *xma_shm, bool rm_shm);

static int xma_init_shm(XmaResConfig *xma_shm, XmaSystemCfg *config,
                        bool shm_locked);

static int xma_shm_lock(XmaResConfig *xma_shm);

static int xma_shm_unlock(XmaResConfig *xma_shm);

static int xma_verify_process_res(pid_t pid);

static int xma_verify_shm_client_procs(XmaResConfig *xma_shm,
                                       XmaSystemCfg *config);

static int xma_alloc_next_dev(XmaResources shm_cfg, int *dev_handle, bool excl);

static int xma_get_next_free_dev(XmaResConfig *xma_shm, int *dev_handle);

static int xma_alloc_dev(XmaResConfig *xma_shm, int dev_handle, bool excl);

static int32_t xma_res_alloc_kernel(XmaResources shm_cfg,
                                    XmaSession *session,
                                    XmaKernReq *kern_props,
                                    enum XmaKernType type);

static int xma_client_thread_kernel_alloc(XmaResources shm_cfg,
                                          XmaDevice *dev,
                                          int dev_kern_idx,
                                          XmaSession *session,
                                          size_t kernel_data_size,
                                          int32_t (*alloc_chan)(XmaSession *p,
                                                                XmaSession **s,
                                                                uint32_t cnt));

static int xma_client_thread_kernel_free(XmaDevice *dev,
                                         pid_t proc_id,
                                         pthread_t thread_id,
                                         int dev_kern_idx,
                                         XmaSession *session);

static XmaKernReq *xma_res_create_kern_req(enum XmaKernType type,
                                                  const char *vendor,
                                                  bool dev_excl);

static int xma_free_dev(XmaResConfig *xma_shm, int32_t dev_handle, pid_t pid);

static void xma_free_all_kernel_chan_res(XmaDevice *dev, pid_t pid);

static void xma_free_all_proc_res(XmaResConfig *xma_shm, pid_t proc_id);

static void xma_dec_ref_shm(XmaResConfig *xma_shm);

static int xma_inc_ref_shm(XmaResConfig *xma_shm);

/********************************IMPLEMENTATION********************************/

XmaResources xma_res_shm_map(XmaSystemCfg *config)
{
    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    xma_set_shm_filenames();
    return (XmaResources)xma_shm_open(XMA_SHM_FILE, config);
}

void xma_res_shm_unmap(XmaResources shm_cfg)
{
    XmaResConfig *xma_shm = (XmaResConfig *)shm_cfg;
    extern XmaSingleton *g_xma_singleton;
    bool rm_shm;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (!xma_shm)
        return;

    if (xma_shm_lock(xma_shm))
        return;

    xma_shm = (XmaResConfig *)g_xma_singleton->shm_res_cfg;
    g_xma_singleton->shm_freed = true;
    xma_dec_ref_shm(xma_shm);
    xma_free_all_proc_res(xma_shm, getpid());
    rm_shm = xma_shm->ref_cnt ? false : true;
    xma_shm_unlock(xma_shm);
    xma_shm_close(xma_shm, rm_shm);
    /* JPM TODO eval changing this to a free(g_xma_singleton) call here */
    g_xma_singleton->shm_res_cfg = NULL;
}

int32_t xma_res_alloc_dev(XmaResources shm_cfg, bool excl)
{
    int dev_handle = -1;
    return xma_alloc_next_dev(shm_cfg, &dev_handle, excl);
}

int32_t xma_res_alloc_next_dev(XmaResources shm_cfg, int dev_handle, bool excl)
{
    return xma_alloc_next_dev(shm_cfg, &dev_handle, excl);
}


static int32_t xma_alloc_next_dev(XmaResources shm_cfg,
                                  int *dev_handle,
                                  bool excl)
{
    XmaResConfig *xma_shm = (XmaResConfig *)shm_cfg;

    while (*dev_handle < MAX_XILINX_DEVICES)
    {
        int ret;
        if (xma_shm_lock(xma_shm))
            return XMA_ERROR;
        ret = xma_get_next_free_dev(xma_shm, dev_handle);
        if (ret < 0) {
            xma_shm_unlock(xma_shm);
            return ret;
        }
        ret = xma_alloc_dev(xma_shm, *dev_handle, excl);
        xma_shm_unlock(xma_shm);
        if (ret < 0)
            continue;

        return *dev_handle;
    }

    return XMA_ERROR;
}

int32_t xma_res_alloc_dec_kernel(XmaResources shm_cfg, XmaDecoderType type,
                                      const char *vendor, XmaSession *session,
                                      bool dev_excl)
{
    XmaKernReq *kern_props;

    kern_props =
        xma_res_create_kern_req(xma_res_decoder, vendor, dev_excl);
    if (!kern_props || !shm_cfg)
        return XMA_ERROR;
    kern_props->kernel_spec.dec_type = type;

    return xma_res_alloc_kernel(shm_cfg, session, kern_props, xma_res_decoder);
}

int32_t xma_res_alloc_filter_kernel(XmaResources shm_cfg, XmaFilterType type,
                                         const char *vendor, XmaSession *session,
                                         bool dev_excl)
{
    XmaKernReq *kern_props;

    kern_props =
        xma_res_create_kern_req(xma_res_filter, vendor, dev_excl);
    if (!kern_props || !shm_cfg)
        return XMA_ERROR;
    kern_props->kernel_spec.filter_type = type;

    return xma_res_alloc_kernel(shm_cfg, session, kern_props, xma_res_filter);
}

int32_t xma_res_alloc_kernel_kernel(XmaResources   shm_cfg,
                                         XmaKernelType  type,
                                         const char    *vendor,
                                         XmaSession    *session,
                                         bool           dev_excl)
{
    XmaKernReq *kern_props;

    kern_props =
        xma_res_create_kern_req(xma_res_kernel, vendor, dev_excl);
    if (!kern_props || !shm_cfg)
        return XMA_ERROR;
    kern_props->kernel_spec.kernel_type = type;

    return xma_res_alloc_kernel(shm_cfg, session,
                                kern_props, xma_res_kernel);
}


int32_t xma_res_alloc_enc_kernel(XmaResources shm_cfg, XmaEncoderType type,
                                      const char *vendor, XmaSession *session,
                                      bool dev_excl)
{
    XmaKernReq *kern_props;

    kern_props =
        xma_res_create_kern_req(xma_res_encoder, vendor, dev_excl);
    if (!kern_props || !shm_cfg)
        return XMA_ERROR;
    kern_props->kernel_spec.enc_type = type;

    return xma_res_alloc_kernel(shm_cfg, session, kern_props, xma_res_encoder);
}

int32_t xma_res_alloc_scal_kernel(XmaResources shm_cfg, XmaScalerType type,
                                       const char *vendor, XmaSession *session,
                                       bool dev_excl)
{
    XmaKernReq *kern_props;

    kern_props =
        xma_res_create_kern_req(xma_res_scaler, vendor, dev_excl);
    if (!kern_props)
        return XMA_ERROR;
    kern_props->kernel_spec.scal_type = type;

    return xma_res_alloc_kernel(shm_cfg, session, kern_props, xma_res_scaler);
}

int32_t xma_res_free_kernel(XmaResources shm_cfg, XmaKernelRes kern_res)
{
    XmaResConfig *xma_shm = (XmaResConfig *)shm_cfg;
    XmaKernReq *kern_req = (XmaKernReq *)kern_res;
    XmaDevice *dev;
    int32_t dev_handle, kern_handle;
    pid_t proc_id = getpid();
    pthread_t thread_id = pthread_self();
    XmaSession *session;
    int ret = 0;

    if (!shm_cfg || !kern_res)
        return XMA_ERROR;

    dev_handle = xma_res_dev_handle_get(kern_res);
    if (dev_handle < 0)
        return XMA_ERROR;

    kern_handle = xma_res_kern_handle_get(kern_res);
    if (kern_handle < 0)
        return XMA_ERROR;

    session = xma_res_session_get(kern_res);
    if (session == NULL)
        return XMA_ERROR;

    dev = &xma_shm->sys_res.devices[dev_handle];
    if (xma_shm_lock(xma_shm))
        return XMA_ERROR;
    ret = xma_client_thread_kernel_free(dev, proc_id, thread_id,
                                        kern_handle, session);
    xma_shm_unlock(xma_shm);
    free(kern_req);
    return ret;
}

int32_t xma_res_free_dev(XmaResources shm_cfg, int32_t dev_handle)
{
    XmaResConfig *xma_shm = (XmaResConfig *)shm_cfg;
    pid_t proc_id = getpid();

    int32_t ret = 0;

    if (!shm_cfg)
        return XMA_ERROR_INVALID;

    if (xma_shm_lock(xma_shm))
        return XMA_ERROR;
    ret = xma_free_dev(xma_shm, dev_handle, proc_id);
    xma_shm_unlock(xma_shm);
    return ret;
}

int32_t xma_res_dev_handle_get(XmaKernelRes *kern_res)
{
    XmaKernReq *kern_req = (XmaKernReq *)kern_res;

    if (!kern_res)
        return XMA_ERROR_INVALID;

    return kern_req->dev_handle;

}

int32_t xma_res_plugin_handle_get(XmaKernelRes *kern_res)
{
    XmaKernReq *kern_req = (XmaKernReq *)kern_res;

    if (!kern_res)
        return XMA_ERROR_INVALID;

    return kern_req->plugin_handle;
}

int32_t xma_res_kern_handle_get(XmaKernelRes *kern_res)
{
    XmaKernReq *kern_req = (XmaKernReq *)kern_res;

    if (!kern_res)
        return XMA_ERROR_INVALID;

    return kern_req->kern_handle;
}

static void xma_set_shm_filenames(void)
{
#ifdef XMA_RES_TEST
    char    *userlogin, *fn, *fn_sig;
    size_t  fn_buff_size;

    if (xma_shm_filename_set)
        return;

    userlogin = getlogin();
    if (!userlogin)
        return;

    fn_buff_size = strlen((const char *)userlogin);
    fn_buff_size += strlen((const char *)XMA_SHM_FILE);
    fn_buff_size += 3;

    fn = malloc(fn_buff_size);
    memset(fn, 0, fn_buff_size);
    strcpy(fn, (const char*)XMA_SHM_FILE);
    strcat(fn, "_");
    strcat(fn, (const char *)userlogin);
    XMA_SHM_FILE = fn;

    fn_buff_size = strlen((const char *)userlogin);
    fn_buff_size += strlen((const char *)XMA_SHM_FILE_SIG);
    fn_buff_size += 3;

    fn_sig = malloc(fn_buff_size);
    memset(fn_sig, 0, fn_buff_size);
    strcpy(fn_sig, (const char*)XMA_SHM_FILE_SIG);
    strcat(fn_sig, "_");
    strcat(fn_sig, (const char *)userlogin);
    XMA_SHM_FILE_SIG = fn_sig;
    xma_shm_filename_set = 1;
#else
    return;
#endif
}

XmaSession *xma_res_session_get(XmaKernelRes *kern_res)
{
    XmaKernReq *kern_req = (XmaKernReq *)kern_res;

    if (!kern_res)
        return NULL;

    return kern_req->session;
}

int32_t xma_res_kern_chan_id_get(XmaKernelRes *kern_res)
{
    if (!kern_res)
        return XMA_ERROR_INVALID;

    return ((XmaKernReq *)kern_res)->session->chan_id;
}

static XmaResConfig *xma_shm_open(char *shm_filename, XmaSystemCfg *config)
{
    extern XmaSingleton *g_xma_singleton;
    int ret, fd;
    XmaResConfig *shm_map;

    pthread_mutexattr_t proc_shared_lock;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    /* JPM TODO consider replacing with shm_open() */
    fd = open(shm_filename, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd < 0 && errno == EEXIST)
        goto eexist;
    else if (fd < 0)
        return NULL;

    fchmod(fd, 0666);
    ret = ftruncate(fd, sizeof(XmaResConfig));
    if (ret)
        return NULL; /*JPM log proper error message */

    pthread_mutexattr_init(&proc_shared_lock);
    pthread_mutexattr_setpshared(&proc_shared_lock, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&proc_shared_lock, PTHREAD_MUTEX_ROBUST);
    shm_map = (XmaResConfig *)mmap(NULL, sizeof(XmaResConfig),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    pthread_mutex_init(&shm_map->lock, &proc_shared_lock);
    ret = xma_init_shm(shm_map, config, false);
    if (ret)
        return NULL;

    return shm_map;

eexist:
    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "Resource database already exists\n");
    /* prevent same process from double-ref counting shm */
    if (g_xma_singleton && g_xma_singleton->shm_res_cfg) {
        xma_logmsg(XMA_INFO_LOG, XMA_RES_MOD,
                   "Resource database already mapped into this process\n");
        return g_xma_singleton->shm_res_cfg;
    }

    fd = open(shm_filename, O_RDWR, 0666);
    if (fd < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_RES_MOD,
                   "Issue opening resource database file: fd = %d\n", fd);
        return NULL;
    }

    shm_map = (XmaResConfig *)mmap(NULL, sizeof(XmaResConfig),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);

    /* verify processes and update ref cnt */
    ret = xma_verify_shm_client_procs(shm_map, config);
    if (ret < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMA_RES_MOD,
                   "Problem verifying clients of shared mem database\n");
        return NULL;
    }

    while (ret != 1 && !xma_res_xma_init_completed()) {
        struct stat stat_buf;

        if (stat(shm_filename, &stat_buf))
            return NULL;
        sched_yield();
    }

    return shm_map;
}

void xma_res_mark_xma_ready(XmaResources shm_cfg)
{
    XmaResConfig *shm_map = (XmaResConfig *)shm_cfg;
    int fd_sig;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    /* File used to signal to other processes that shm is ready */
    fd_sig = open(XMA_SHM_FILE_SIG, O_CREAT | O_EXCL, 0666);
    if (fd_sig < 0 && errno == EEXIST) {
        return;
    } else if (fd_sig < 0) {
        xma_res_shm_unmap(shm_map);
        return;
    }
    fchmod(fd_sig, 0644);
}

bool xma_res_xma_init_completed(void)
{
    struct stat stat_buf;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    return stat(XMA_SHM_FILE_SIG, &stat_buf) == 0 ? true : false;
}

static int xma_init_shm(XmaResConfig *xma_shm,
                        XmaSystemCfg *config, bool shm_locked)
{
    XmaDevice *shm_devices = xma_shm->sys_res.devices;
    XmaImage *shm_images = xma_shm->sys_res.images;
    int i, j, kern_cnt, kern_inst_cnt, tot_kerns;
    uint32_t cfg_dev_ids[MAX_XILINX_DEVICES];
    int32_t dev_cnt, img_cnt, cfg_dev_idx;
    int decoder_idx = 0;
    int encoder_idx = 0;
    int scaler_idx = 0;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    img_cnt = xma_cfg_img_cnt_get();
    dev_cnt = xma_cfg_dev_cnt_get();
    if (img_cnt < 0 || dev_cnt < 0)
        return XMA_ERROR_INVALID;

    xma_cfg_dev_ids_get(cfg_dev_ids);

    if (!shm_locked && (0 != xma_shm_lock(xma_shm)))
        return XMA_ERROR;

    memset(&xma_shm->sys_res, 0, sizeof(XmaShmRes));

    /* init device data */
    for (i = 0, cfg_dev_idx = 0; i < dev_cnt; i++, cfg_dev_idx++) {
        shm_devices[cfg_dev_ids[cfg_dev_idx]].configured = true;
        shm_devices[cfg_dev_ids[cfg_dev_idx]].exists = true;
        shm_devices[cfg_dev_ids[cfg_dev_idx]].excl = false;
    }

    /* init image data */
    for (i = 0; i < img_cnt; i++) {
        strncpy(shm_images[i].name, config->imagecfg[i].xclbin, (NAME_MAX-1));
        shm_images[i].kernel_cnt = config->imagecfg[i].num_kernelcfg_entries;
        /* populate kernelcfg entries for image */
        for(kern_cnt = 0;
            kern_cnt < config->imagecfg[i].num_kernelcfg_entries;
            kern_cnt++)
        {
            XmaKernelCfg *kernelcfg = config->imagecfg[i].kernelcfg;
            strncpy(shm_images[i].kernels[kern_cnt].name,
                    kernelcfg[kern_cnt].name,
                    (MAX_KERNEL_NAME-1));
            strncpy(shm_images[i].kernels[kern_cnt].vendor,
                    kernelcfg[kern_cnt].vendor,
                    (MAX_VENDOR_NAME-1));
            strncpy(shm_images[i].kernels[kern_cnt].function,
                    kernelcfg[kern_cnt].function,
                    (MAX_FUNCTION_NAME-1));

            if (strcmp(config->imagecfg[i].kernelcfg[kern_cnt].function,
                   XMA_CFG_FUNC_NM_SCALE) == 0) {
                    shm_images[i].kernels[kern_cnt].plugin_handle =
                        scaler_idx++;
            } else if (strcmp(config->imagecfg[i].kernelcfg[kern_cnt].function,
                   XMA_CFG_FUNC_NM_ENC) == 0) {
                    shm_images[i].kernels[kern_cnt].plugin_handle =
                        encoder_idx++;
            } else if (strcmp(config->imagecfg[i].kernelcfg[kern_cnt].function,
                   XMA_CFG_FUNC_NM_DEC) == 0) {
                    shm_images[i].kernels[kern_cnt].plugin_handle =
                        decoder_idx++;
            }
        }
        /* map image id to shm device entry */
        for (j = 0; j < config->imagecfg[i].num_devices; j++)
        {
            XmaKernelCfg *kernelcfg = config->imagecfg[i].kernelcfg;
            int32_t dev_id = config->imagecfg[i].device_id_map[j];
            shm_devices[dev_id].image_id = i;
            /* populate kernel map for each device */
            for (kern_cnt = 0, tot_kerns = 0;
                 kern_cnt < config->imagecfg[i].num_kernelcfg_entries;
                 kern_cnt++)
            {
                for(kern_inst_cnt = 0;
                    kern_inst_cnt < kernelcfg[kern_cnt].instances &&
                    tot_kerns < MAX_KERNEL_CONFIGS;
                    kern_inst_cnt++, tot_kerns++)
                    shm_devices[dev_id].kernels[tot_kerns].kernel_id = kern_cnt;
            }
            shm_devices[dev_id].kernel_cnt = tot_kerns;
        }
    }
    xma_inc_ref_shm(xma_shm);
    if (!shm_locked)
        xma_shm_unlock(xma_shm);

    return XMA_SUCCESS;
}

static void xma_shm_close(XmaResConfig *xma_shm, bool rm_shm)
{
    if (!xma_shm)
        return;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    munmap((void*)xma_shm, sizeof(XmaResConfig));

    if (rm_shm) {
        unlink(XMA_SHM_FILE);
        unlink(XMA_SHM_FILE_SIG);
    }
}

static int xma_verify_process_res(pid_t pid)
{
    struct stat stat_buf;
    int ret;
    char procfs_pid[20] = {0};


    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
               "%s() verify pid client %d\n", __func__, pid);

    snprintf(procfs_pid, sizeof(procfs_pid), "/proc/%d", pid);
    ret = stat(procfs_pid, &stat_buf);
    if (ret && errno == ENOENT) {
        xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                   "%s() client %d is not alive\n", __func__, pid);
        return XMA_ERROR;
    }

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
               "%s() client %d is alive\n", __func__, pid);
    return XMA_SUCCESS;
}

static int xma_get_next_free_dev(XmaResConfig *xma_shm, int *dev_handle)
{
    XmaDevice *devices = xma_shm->sys_res.devices;
    pid_t  proc_id = getpid();
    uint32_t dev_id;
    int ret;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    /* start search from next device: *dev_handle + 1 */
    for (dev_id = *dev_handle >= 0 ? *dev_handle + 1 : 0;
         dev_id < MAX_XILINX_DEVICES; dev_id++) {
        if (!devices[dev_id].exists)
            continue;

        if (devices[dev_id].excl) {
            ret = xma_verify_process_res(devices[dev_id].client_procs[0]);
            if (ret) {
                xma_free_all_kernel_chan_res(&devices[dev_id], 0);
                xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                           "Resetting client id for exclusive use device %u\n",
                           dev_id);
                devices[dev_id].excl = false;
                devices[dev_id].client_procs[0] = 0;
                *dev_handle = dev_id;
                return XMA_SUCCESS;
            } else if (devices[dev_id].client_procs[0] == proc_id) {
                xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                           "Found free device id: %u\n", dev_id);
                *dev_handle = dev_id;
                return XMA_SUCCESS;
            }
            continue;
        }
        *dev_handle = dev_id;
        return XMA_SUCCESS;
    }
    return XMA_ERROR_NO_DEV;
}

static int xma_alloc_dev(XmaResConfig *xma_shm, int dev_handle,
                         bool excl)
{
    XmaDevice *devices = xma_shm->sys_res.devices;
    pid_t  proc_id = getpid();
    int pid_idx;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    /* does process already have exclusive access? */
    if (devices[dev_handle].excl)
    {
        if (devices[dev_handle].client_procs[0] == proc_id)
            return XMA_SUCCESS;
        else
            return XMA_ERROR_NO_DEV;
    }

    if (excl) {
        /* is another process already using this as a non-exclusive device? */
        for (pid_idx = 0; pid_idx < MAX_KERNEL_CONFIGS; pid_idx++) {
            if (devices[dev_handle].client_procs[pid_idx] &&
                devices[dev_handle].client_procs[pid_idx] != proc_id) {
                xma_logmsg(XMA_ERROR_LOG, XMA_RES_MOD,
                           "Cannot allocate %d as an exclusive device.\n",
                           dev_handle);
                xma_logmsg(XMA_ERROR_LOG, XMA_RES_MOD,
                           "Already in use by %lu\n",
                           devices[dev_handle].client_procs[pid_idx]);
                return XMA_ERROR_NO_DEV;
            }
        }
        devices[dev_handle].excl = true;
        devices[dev_handle].client_procs[0] = proc_id;
        return XMA_SUCCESS;
    }

    /* process is already using this non-exclusive device? */
    for (pid_idx = 0; pid_idx < MAX_KERNEL_CONFIGS; pid_idx++)
        if (devices[dev_handle].client_procs[pid_idx] == proc_id) {
            xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                       "%s() Returning device already in use by %lu\n",
                       __func__, proc_id);
            return XMA_SUCCESS;
        }

    /* register process as using non-exclusive device */
    for (pid_idx = 0; pid_idx < MAX_KERNEL_CONFIGS; pid_idx++)
        if (!devices[dev_handle].client_procs[pid_idx]) {
            devices[dev_handle].client_procs[pid_idx] = proc_id;
            xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                       "%s() Registering pid %lu with device %lu\n",
                       __func__, proc_id, dev_handle);
            return XMA_SUCCESS;
        }

    return XMA_ERROR_NO_DEV;
}

static int xma_free_dev(XmaResConfig *xma_shm, int32_t dev_handle,
                        pid_t proc_id)
{
    XmaDevice *devices = xma_shm->sys_res.devices;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (!devices[dev_handle].exists)
        return XMA_ERROR_NO_DEV;

    if (devices[dev_handle].excl) {
        devices[dev_handle].excl = false;
        devices[dev_handle].client_procs[0] = 0;
        return XMA_SUCCESS;
    } else {
        int pid_idx;
        for (pid_idx = 0; pid_idx < MAX_KERNEL_CONFIGS; pid_idx++)
            if (devices[dev_handle].client_procs[pid_idx] == proc_id) {
                devices[dev_handle].client_procs[pid_idx] = 0;
                /* JPM need to clear kernel channel data as well */
                return XMA_SUCCESS;
            }
    }
    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
               "Unable to free device %d for process id %lu\n",
               dev_handle, proc_id);
    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "Invalid device handle\n");
    return XMA_ERROR_INVALID;
}

static int32_t xma_res_alloc_kernel(XmaResources shm_cfg,
                                         XmaSession *session,
                                         XmaKernReq *kern_props,
                                         enum XmaKernType type)
{
    int32_t (*plugin_alloc_chan)(XmaSession *pending,
                                 XmaSession **current,
                                 uint32_t sess_cnt);
    XmaResConfig *xma_shm = (XmaResConfig *)shm_cfg;
    pid_t proc_id = getpid();
    extern XmaSingleton *g_xma_singleton;
    int kern_idx, dev_id;
    bool kern_aquired = false;
    size_t kernel_data_size;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (!session)
        return XMA_ERROR_INVALID;

    for (dev_id = -1; !kern_aquired && (dev_id < MAX_XILINX_DEVICES);)
    {
        XmaDevice *dev;
        int ret = 0;
        bool dev_exclusive = kern_props->dev_excl;

        dev_id = xma_res_alloc_next_dev(shm_cfg, dev_id, dev_exclusive);
        if (dev_id < 0)
            break;

        dev = &xma_shm->sys_res.devices[dev_id];
        /* JPM check to see if matching kernel is on allocated device */
        /* if not, free device, increment dev count, re-loop */
        for (kern_idx = 0;
             kern_idx < MAX_KERNEL_CONFIGS &&
             kern_idx < dev->kernel_cnt && !kern_aquired;
             kern_idx++)
        {
            int str_cmp1 = -1, str_cmp2 = -1, type_cmp = false;
            int kern_id = dev->kernels[kern_idx].kernel_id;
            XmaKernel *kernel =
                &xma_shm->sys_res.images[dev->image_id].kernels[kern_id];
            XmaScalerPlugin *scaler;
            XmaDecoderPlugin *decoder;
            XmaEncoderPlugin *encoder;
            XmaFilterPlugin *filter;
            XmaKernelPlugin *kernplg;
            plugin_alloc_chan = NULL;

            str_cmp1 = strcmp(kernel->vendor, kern_props->vendor);
            if (type == xma_res_scaler) {
                scaler = &g_xma_singleton->scalercfg[kernel->plugin_handle];
                str_cmp2 = strcmp(kernel->function, XMA_CFG_FUNC_NM_SCALE);
                type_cmp = scaler->hwscaler_type ==
                           kern_props->kernel_spec.scal_type ? true : false;
                plugin_alloc_chan = scaler->alloc_chan;
                kernel_data_size = 0;
            } else if (type == xma_res_encoder) {
                encoder = &g_xma_singleton->encodercfg[kernel->plugin_handle];
                str_cmp2 = strcmp(kernel->function, XMA_CFG_FUNC_NM_ENC);
                type_cmp = encoder->hwencoder_type ==
                           kern_props->kernel_spec.enc_type ? true : false;
                plugin_alloc_chan = encoder->alloc_chan;
                kernel_data_size = encoder->kernel_data_size;
            } else if (type == xma_res_decoder) {
                decoder = &g_xma_singleton->decodercfg[kernel->plugin_handle];
                str_cmp2 = strcmp(kernel->function, XMA_CFG_FUNC_NM_DEC);
                type_cmp = decoder->hwdecoder_type ==
                           kern_props->kernel_spec.dec_type ? true : false;
                plugin_alloc_chan = decoder->alloc_chan;
                kernel_data_size = 0;
            } else if (type == xma_res_filter) {
                filter = &g_xma_singleton->filtercfg[kernel->plugin_handle];
                str_cmp2 = strcmp(kernel->function, XMA_CFG_FUNC_NM_FILTER);
                type_cmp = filter->hwfilter_type ==
                           kern_props->kernel_spec.filter_type ? true : false;
                plugin_alloc_chan = filter->alloc_chan;
                kernel_data_size = 0;
            } else if (type == xma_res_kernel) {
                kernplg = &g_xma_singleton->kernelcfg[kernel->plugin_handle];
                str_cmp2 = strcmp(kernel->function, XMA_CFG_FUNC_NM_KERNEL);
                type_cmp = kernplg->hwkernel_type ==
                           kern_props->kernel_spec.kernel_type ? true : false;
                kernel_data_size = 0;
            }

            if (str_cmp1 == 0 && str_cmp2 == 0 && type_cmp) {
                /* register client thread id with kernel */
                    ret = xma_client_thread_kernel_alloc(shm_cfg, dev, kern_idx,
                                                         session,
                                                         kernel_data_size,
                                                         plugin_alloc_chan);
                    if (ret)
                        continue;

                    kern_props->dev_handle = dev_id;
                    kern_props->kern_handle = kern_idx;
                    kern_props->plugin_handle = kernel->plugin_handle;
                    kern_props->session = session;
                    kern_aquired = true;
            }
        }

        if (!kern_aquired) {
            xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                       "%s() Unable to locate requested %s kernel type\n",
                        __func__,
                        kern_props->kernel_spec.scal_type ? "scaler" :
                        kern_props->kernel_spec.enc_type ? "encoder" :
                        kern_props->kernel_spec.dec_type ? "decoder" :
                        kern_props->kernel_spec.filter_type ? "filter" :
                        "kernel"
                        );
            xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                       "%s() from vendor %s on device %d\n",
                        __func__, kern_props->vendor, dev_id);
            ret = xma_free_dev(xma_shm, dev_id, proc_id);
        }

        if (ret < 0) {
            return XMA_ERROR;
        }
    }
    if (kern_aquired) {
        session->kern_res = (XmaKernelRes)kern_props;
        return XMA_SUCCESS;
    }

    xma_logmsg(XMA_ERROR_LOG, XMA_RES_MOD, "No available kernels of type '%s' from vendor %s\n",
               kern_props->kernel_spec.scal_type ? "scaler" :
               kern_props->kernel_spec.enc_type ? "encoder" :
               kern_props->kernel_spec.dec_type ? "decoder" :
               kern_props->kernel_spec.filter_type ? "filter" :
               "kernel", kern_props->vendor);
    return XMA_ERROR_NO_KERNEL;

}

static int32_t xma_client_thread_kernel_alloc(XmaResources shm_cfg,
                                              XmaDevice *dev,
                                              int dev_kern_idx,
                                              XmaSession *session,
                                              size_t kernel_data_size,
                                              int32_t (*alloc_chan)
                                                            (XmaSession *p,
                                                             XmaSession **c,
                                                             uint32_t sess_cnt))
{
    XmaKernelInstance *kernel_inst = &dev->kernels[dev_kern_idx];
    XmaResConfig *xma_shm = (XmaResConfig *)shm_cfg;
    XmaSession *sessions[MAX_KERNEL_CHANS];
    pthread_t thread_id = pthread_self();
    pid_t proc_id = getpid();
    int j, ret;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (xma_shm_lock(xma_shm))
        return XMA_ERROR;

    if (kernel_inst->client_id && kernel_inst->client_id != proc_id) {
        xma_shm_unlock(xma_shm);
        return XMA_ERROR_NO_KERNEL; /* some other process has this kernel */
    }

    kernel_inst->client_id = proc_id;

    for (j = 0; kernel_inst->channels[j].thread_id && j < MAX_KERNEL_CHANS; j++)
        sessions[j] = kernel_inst->channels[j].session;

    if (!j) { /* unused kernel */
        xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s() Kernel not in-use\n", __func__);
        if (alloc_chan) {
            xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                       "%s() Kernel supports channels\n", __func__);
            if (kernel_data_size > 0)
                session->kernel_data = calloc(kernel_data_size, sizeof(uint8_t));
            ret = alloc_chan(session, sessions, j);
            if (ret) {
                xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                           "%s() Channel request rejected\n", __func__);
                xma_shm_unlock(xma_shm);
                return ret;
            }
        }
        kernel_inst->channels[j].session = session;
        kernel_inst->channels[j].thread_id = thread_id;
        session->chan_id = session->chan_id >= 0 ? session->chan_id : 0;
        xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                   "%s() Kernel aquired. Channel id %d\n",
                   __func__, session->chan_id);
        xma_shm_unlock(xma_shm);
        return XMA_SUCCESS;
    } else if (j && j < MAX_KERNEL_CHANS && alloc_chan) {
        /* verify it can support another request */
        xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                   "%s() Kernel in-use and supports channels. Channel instance %d\n",
                   __func__, j);
        if (kernel_data_size > 0)
            session->kernel_data = sessions[0]->kernel_data;
        ret = alloc_chan(session, sessions, j);
        if (ret) {
            xma_shm_unlock(xma_shm);
            return ret;
        }
        kernel_inst->channels[j].session = session;
        kernel_inst->channels[j].thread_id = thread_id;
        xma_shm_unlock(xma_shm);
        return XMA_SUCCESS;
    } else if (j && !alloc_chan) {
        /* kernel is in-use and doesn't support channels */
        xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD,
                   "%s() All kernel channels in-use \n", __func__);
        xma_shm_unlock(xma_shm);
        return XMA_ERROR_NO_KERNEL;
    }
    xma_shm_unlock(xma_shm);
    return XMA_ERROR;
}

static int xma_client_thread_kernel_free(XmaDevice *dev,
                                         pid_t proc_id,
                                         pthread_t thread_id,
                                         int dev_kern_idx,
                                         XmaSession *session)
{
    XmaKernelInstance *kernel_inst = &dev->kernels[dev_kern_idx];
    bool last_used_chan = false;
    int i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    /* are we the wrong process? */
    if (kernel_inst->client_id && kernel_inst->client_id != proc_id)
        return XMA_ERROR_INVALID;

    for (i = 0; i < MAX_KERNEL_CHANS; i++) {
        if (kernel_inst->channels[i].thread_id != thread_id)
            continue;
        if (kernel_inst->channels[i].session != session)
            continue;
        kernel_inst->channels[i].thread_id = 0;
        kernel_inst->channels[i].session = NULL;
        /* eliminate fragmentation in list of used channels after free */
        for (; i < MAX_KERNEL_CHANS-1 &&
               kernel_inst->channels[i+1].thread_id &&
               kernel_inst->channels[i+1].session; i++) {
            kernel_inst->channels[i].thread_id =
                                     kernel_inst->channels[i+1].thread_id;
            kernel_inst->channels[i].session =
                                     kernel_inst->channels[i+1].session;
        }
        last_used_chan = !i ? true : false;
        /* ensure last entry is cleared if not otherwise */
        if (!last_used_chan) {
            kernel_inst->channels[i].thread_id = 0;
            kernel_inst->channels[i].session = NULL;
            return XMA_SUCCESS;
        } else {
            kernel_inst->client_id = 0;
            if (session->kernel_data)
                free(session->kernel_data);
            return XMA_SUCCESS;
        }
    }
    return XMA_ERROR;
}

static XmaKernReq *xma_res_create_kern_req(enum XmaKernType type,
                                           const char *vendor,
                                           bool dev_excl)
{
    XmaKernReq *req = malloc(sizeof(XmaKernReq));

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (!req || !vendor) {
        if (req)
            free(req);
        return NULL;
    }

    memset(req, 0, sizeof(XmaKernReq));
    req->type = type;
    strncpy(req->vendor, vendor, (NAME_MAX-1));
    req->dev_excl = dev_excl;
    req->dev_handle = -1;
    req->kern_handle = -1;
    req->plugin_handle = -1;

    return req;
}

static int xma_shm_lock(XmaResConfig *xma_shm)
{
    extern XmaSingleton *g_xma_singleton;
    int ret;

    if (g_xma_singleton->shm_freed || !xma_shm)
        return XMA_ERROR_INVALID;

    ret = pthread_mutex_lock(&xma_shm->lock);
    if (ret == EOWNERDEAD) {
        pthread_mutex_consistent(&xma_shm->lock);
        return XMA_SUCCESS;
    }
    return ret;
}

static int xma_shm_unlock(XmaResConfig *xma_shm)
{
    if (!xma_shm)
        return XMA_ERROR_INVALID;
    return pthread_mutex_unlock(&xma_shm->lock);
}

static void xma_free_all_kernel_chan_res(XmaDevice *dev, pid_t proc_id)
{
    int i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    for (i = 0; i < MAX_KERNEL_CONFIGS && i < dev->kernel_cnt; i++)
    {
        XmaKernelInstance *kernel = &dev->kernels[i];
        int j;

        if (proc_id && kernel->client_id != proc_id)
            continue;

        kernel->client_id = 0;
        for (j = 0; j < MAX_KERNEL_CHANS && kernel->channels[j].session; j++)
        {
            kernel->channels[j].thread_id = 0;
            kernel->channels[j].session = NULL;
        }
    }
}

static int xma_verify_shm_client_procs(XmaResConfig *xma_shm,
                                       XmaSystemCfg *config)
{
    int i, ret, max_refs;
    bool shm_reinit = false;

    max_refs = MAX_XILINX_DEVICES * MAX_KERNEL_CONFIGS;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (xma_shm_lock(xma_shm))
        return XMA_ERROR;

    for (i = xma_shm->ref_cnt - 1; i >= 0; i--)
    {
        pid_t dead_proc = xma_shm->clients[i];

        ret = xma_verify_process_res(dead_proc);
        if (ret) {
            int j;

            xma_shm->clients[i] = 0;
            xma_shm->ref_cnt--;

            /* free all resources associated with pid */
            xma_free_all_proc_res(xma_shm, dead_proc);

            /* defragment process list */
            for (j = i; j < max_refs && xma_shm->clients[j + 1]; j++)
                xma_shm->clients[j] = xma_shm->clients[j + 1];

            /* if we had to defragment, clean up duplicate last entry */
            if (j != i)
                xma_shm->clients[j] = 0;
        }

    }

    if (xma_shm->ref_cnt == 0) {
        ret = xma_init_shm(xma_shm, config, true);
        if (ret)
            return ret;

        unlink(XMA_SHM_FILE_SIG);
        shm_reinit = true;
    }

    if (!shm_reinit && xma_inc_ref_shm(xma_shm)) {
        xma_shm_unlock(xma_shm);
        return XMA_ERROR;
    }
    xma_shm_unlock(xma_shm);

    return shm_reinit ? 1 : XMA_SUCCESS;
}

/* call while holding lock */
static void xma_dec_ref_shm(XmaResConfig *xma_shm)
{
    pid_t curr_proc = getpid();
    int max_refs = MAX_XILINX_DEVICES * MAX_KERNEL_CONFIGS;
    int i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, __func__);
    for (i = 0; i < xma_shm->ref_cnt; i++)
    {
        int j;

        if (curr_proc != xma_shm->clients[i])
            continue;

        xma_shm->clients[i] = 0;
        xma_shm->ref_cnt--;

        /* defragment process list */
        for (j = i; j < max_refs && xma_shm->clients[j + 1]; j++)
            xma_shm->clients[j] = xma_shm->clients[j + 1];

        /* if we had to defragment, clean up duplicate last entry */
        if (j != i)
            xma_shm->clients[j] = 0;
    }
}

/* call while holding lock */
static int xma_inc_ref_shm(XmaResConfig *xma_shm)
{
    pid_t curr_proc = getpid();
    int max_refs = MAX_XILINX_DEVICES * MAX_KERNEL_CONFIGS;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    if (xma_shm->ref_cnt + 1 > max_refs)
        return XMA_ERROR_NO_KERNEL;

    xma_shm->clients[xma_shm->ref_cnt] = curr_proc;
    xma_shm->ref_cnt++;
    return XMA_SUCCESS;
}

/* call while holding lock */
static void xma_free_all_proc_res(XmaResConfig *xma_shm, pid_t proc_id)
{
    int i;

    xma_logmsg(XMA_DEBUG_LOG, XMA_RES_MOD, "%s()\n", __func__);
    for (i = 0; i < MAX_XILINX_DEVICES; i++)
    {
        xma_free_dev(xma_shm, i, proc_id);
        xma_free_all_kernel_chan_res(&xma_shm->sys_res.devices[i], proc_id);
    }
    return;
}
