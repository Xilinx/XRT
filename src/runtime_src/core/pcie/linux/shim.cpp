/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 *
 * XRT PCIe library layered on top of xocl kernel driver
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
#include "shim.h"  // This file implements shim.h
#include "xrt.h"   // This file implements xrt.h

#include "ert.h"
#include "scan.h"
#include "system_linux.h"
#include "xclbin.h"

#include "core/include/shim_int.h"
#include "core/include/xdp/fifo.h"
#include "core/include/xdp/trace.h"
#include "core/include/experimental/xrt_hw_context.h"

#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/scheduler.h"
#include "core/common/xclbin_parser.h"
#include "core/common/AlignedAllocator.h"
#include "core/common/api/hw_context_int.h"

#include "plugin/xdp/aie_trace.h"
#include "plugin/xdp/hal_api_interface.h"
#include "plugin/xdp/hal_device_offload.h"
#include "plugin/xdp/hal_profile.h"
#include "plugin/xdp/pl_deadlock.h"

#include "core/pcie/driver/linux/include/mgmt-reg.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

#include <unistd.h>
#include <poll.h>

#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <asm/mman.h>
#include <linux/aio_abi.h>

#if defined(__GNUC__)
#define SHIM_UNUSED __attribute__((unused))
#endif

#define GB(x)           ((size_t) (x) << 30)
#define ARRAY_SIZE(x)   (sizeof (x) / sizeof (x[0]))

#define SHIM_QDMA_AIO_EVT_MAX   1024 * 64

namespace xq = xrt_core::query;
namespace {

template <typename ...Args>
void
xrt_logmsg(xrtLogMsgLevel level, const char* format, Args&&... args)
{
  auto slvl = static_cast<xrt_core::message::severity_level>(level);
  xrt_core::message::send(slvl, "XRT", format, std::forward<Args>(args)...);
}

/*
 * numClocks()
 */
inline unsigned numClocks(const std::string& name)
{
    return name.compare(0, 15, "xilinx_adm-pcie", 15) ? 2 : 1;
}

inline int io_setup(unsigned nr, aio_context_t *ctxp)
{
  return syscall(__NR_io_setup, nr, ctxp);
}

inline int io_destroy(aio_context_t ctx)
{
  return syscall(__NR_io_destroy, ctx);
}

inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp)
{
  return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
                struct io_event *events, struct timespec *timeout)
{
  return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

static
xocl::shim*
get_shim_object(xclDeviceHandle handle)
{
  if (auto shim = xocl::shim::handleCheck(handle))
    return shim;

  throw xrt_core::error("Invalid shim handle");
}

} // namespace

namespace xocl {

/*
 * queue_cb: A per queue control block for a qdma stream queue.
 * queue_cb keeps track of * per-queue i/o related configurations. And it
 * handles the actual interaction with the kernel on the i/o data path.
 *
 * Configuration:
 * - qAioEn:		per queue aio context enabled or not
 * - qAioCtx:		per queue aio context, valid only if qAioEn = true
 * - set_option():	optional configurations for aio context and batching
 *
 * i/o data path:
 * - queue_submit_io(): format & submit i/o to the kernel driver
 * - queue_poll_completion(): polliing for completion event for asynchronously
 * 			submitted i/o requests, valid only if qAioEn = true
 */

class queue_cb {
private:
    struct queued_io {	/* batching: io representation */
       unsigned long flags;		/* flags from the original wr */
       unsigned long priv_data; 	/* priv_data from the original wr */
       unsigned long buf_va;		/* i/o buffer virutal address */
       unsigned long len;		/* i/o buffer length */
       struct iocb cb;			/* used for io_submit */
       struct iovec iov[2];		/* used for io_submit */
       struct xocl_qdma_req_header header;	/* used for io_submit */
    };

    bool qAioEn;		/* per queue aio is enabled */
    bool qAioBatchEn;		/* per queue aio req. batching is enabled */
    bool qExit;			/* queue is being stopped/released */
    bool h2c; 			/* queue is for H2C direction */
    uint64_t qhndl;		/* queue handle */
    unsigned int aio_max_evts;	/* for io_setup() max. event concurrently */

    /* aio batching threshold to flush the queued i/o */
    unsigned int byteThresh;	/* total bytes count */
    unsigned int pktThresh;	/* total buffer/packet count */

    unsigned int byteCnt;	/* total bytes of queued i/o */
    unsigned int bufCnt;	/* total buffer count of queued i/o */
    unsigned int cbSubmitCnt;	/* total # submitted i/o */
    unsigned int cbPollCnt;	/* total # of polled/completed i/o */
    int cbErrCnt;		/* submission error */
    int cbErrCode;		/* submission error code */

    aio_context_t qAioCtx;	/* per queue aio context */

    std::thread qWorker;	/* aio batching thread */
    std::mutex reqLock;		/* lock to protect i/o related info. */
    std::list<struct queued_io> reqList; /* queued up i/o request */
    std::condition_variable cv; /* for wait/wake up the batching thread */

    /* release queued i/o and update the stats */
    int release_request(int count)
    {
        /* calling function should hold the reqLock */
        int i = 0;
        while (i < count && !(reqList.empty())) {
            auto it = reqList.begin();
            auto qio = (*it);

	    i++;
            bufCnt--;
            byteCnt -= qio.len;
            reqList.pop_front();
        }

        return i;
    }

    int queue_up_request(xclQueueRequest *wr)
    {
        std::lock_guard<std::mutex> lk(reqLock);

        /* if there was io submission error, wait till all requests are
         * drained or list full*/
        if (cbErrCnt || (reqList.size() == aio_max_evts))
            return -EAGAIN;

        /* queue up this async i/o request */
        unsigned int bytes = 0;
        for (unsigned int i = 0; i < wr->buf_num; i++) {
             bytes += wr->bufs[i].len;
             reqList.push_back({wr->flag, (uint64_t)wr->priv_data,
				wr->bufs[i].va, wr->bufs[i].len});
        }
        bufCnt += wr->buf_num;
        byteCnt += bytes;

	/* wake up the batch worker thread if threshold is reached */
        if ((pktThresh && bufCnt >= pktThresh) ||
            (byteThresh && byteCnt >= byteThresh))
            cv.notify_one();

        return wr->buf_num;
    }

    int check_io_submission_error(int nr_comps, struct xclReqCompletion *comps)
    {
        std::lock_guard<std::mutex> lk(reqLock);

        /* no io submission error or more pending i/o requests */
        if (!cbErrCnt || (cbSubmitCnt != cbPollCnt))
            return 0;

        int num_evt = (cbErrCnt <= nr_comps) ? cbErrCnt : nr_comps;
        cbErrCnt -= num_evt;

        int i = 0;
        auto end = reqList.end();
        for (auto it = reqList.begin(); it != end && i < num_evt; ++it, i++) {
            comps[i].nbytes = 0;
            comps[i].err_code = cbErrCode;
            comps[i].priv_data = (void *)(*it).priv_data;
        }

        release_request(i);

        if (!cbErrCnt)
            cbErrCode = 0;

        return num_evt;
    }

    /* prepare io submission structures */
    void prepare_io(struct iocb *cb, struct iovec *iov,
                    struct xocl_qdma_req_header *header,
		    unsigned long buf_va, unsigned long buf_len,
                    uint64_t priv_data)
    {
        iov[0].iov_base = header;
        iov[0].iov_len = sizeof(struct xocl_qdma_req_header);
        iov[1].iov_base = (void *)buf_va;
        iov[1].iov_len = buf_len;

        if (cb) {
            memset(cb, 0, sizeof(struct iocb));
            cb->aio_fildes = (int)qhndl;
            cb->aio_lio_opcode = h2c ? IOCB_CMD_PWRITEV : IOCB_CMD_PREADV;
            cb->aio_buf = (uint64_t)iov;
            cb->aio_offset = 0;
            cb->aio_nbytes = 2;
            cb->aio_data = priv_data;
        }
    }

    /* submit all of the queued i/o, calling function should hold the lock */
    int queue_flush_aio_request(void)
    {
        /* if there is submission error, wait till all requests are drained */
        if (cbErrCnt)
            return -EAGAIN;

        /* submit all queued requests */
        unsigned int cb_max = bufCnt;
        unsigned int cb_cnt = 0;
        std::vector <struct iocb *> cbpp(cb_max);

        auto end = reqList.end();
        for (auto it = reqList.begin(); it != end && cb_cnt < cb_max;
             ++it, cb_cnt++) {

            it->header.flags = it->flags;
            prepare_io(&it->cb, it->iov, &it->header, it->buf_va, it->len,
			it->priv_data);
            cbpp[cb_cnt] = &it->cb;
        }

        int submitted = io_submit(qAioCtx, cb_cnt, cbpp.data());
        if (submitted < 0) {
            if (submitted != -EAGAIN) {
		/* something went wrong, flush all of the queued request with
                 * this error */
                cbErrCnt = reqList.size();
                cbErrCode= submitted;
            }
            return submitted;
        }

        if (submitted > 0) {
            release_request(submitted);
            cbSubmitCnt += submitted;
        }

        return 0;
    }

    /* worker thread for aio batching */
    void queue_aio_worker(void)
    {
        byteCnt = bufCnt = 0;

        do {
            std::unique_lock<std::mutex> lck(reqLock);
	    while (reqList.empty() && !qExit)
                cv.wait(lck);
            queue_flush_aio_request();
        } while(!qExit);

        qAioBatchEn = false;
    }

    void stop_aio_worker(void)
    {
        if (qAioBatchEn) {
            std::lock_guard<std::mutex> lk(reqLock);
            /* stop the worker thread */
            qExit = true;
            cv.notify_one();
        }
        if (qWorker.joinable())
            qWorker.join();
    }

    void queue_aio_batch_disable_check(void)
    {
        if (!byteThresh && !pktThresh)
            stop_aio_worker();
    }

    void queue_aio_batch_enable_check(void)
    {
        /* to enable aio batching, the stream needs to have its own context
         * and any of the
         */
        if (qAioEn && (byteThresh || pktThresh) && !qAioBatchEn) {
            std::lock_guard<std::mutex> lk(reqLock);
            qExit = false;
            qWorker = std::thread(&queue_cb::queue_aio_worker, this);
            qAioBatchEn = true;
        }
    }

public:
    queue_cb(struct xocl_qdma_ioc_create_queue *qinfo)
        : qAioEn{false}, qAioBatchEn{false}, qExit{false},
          h2c{qinfo->write ? true : false}, qhndl{qinfo->handle},
          aio_max_evts{0}, byteThresh{0}, pktThresh{0}, byteCnt{0}, bufCnt{0},
          cbSubmitCnt{0}, cbPollCnt{0}, cbErrCnt{0}, cbErrCode{0}
    {
       memset(&qAioCtx, 0, sizeof(qAioCtx));
    }

    ~queue_cb()
    {
        if (!qAioEn)
           return;

        stop_aio_worker();
        io_destroy(qAioCtx);
    }

    const bool queue_aio_ctx_enabled(void) { return qAioEn; }
    const bool queue_is_h2c(void) { return h2c; }
    const int queue_get_handle(void) { return qhndl; }

    // optional configurations
    int set_option(int type, uint32_t val)
    {
        switch(type) {
        case STREAM_OPT_AIO_MAX_EVENT:
	    if (!qAioEn) {
                if (!val)
                    val = SHIM_QDMA_AIO_EVT_MAX;
		auto rc = io_setup(val, &qAioCtx);
		if (!rc) {
		     qAioEn = true;
                     aio_max_evts = val;
                }
                return rc;
	    }
            return -EINVAL;
            /* for i/o batching */
        case STREAM_OPT_AIO_BATCH_THRESH_BYTES:
	    byteThresh = val;
            if (val && qAioEn && !qAioBatchEn)
                queue_aio_batch_enable_check();
            else if (!val && qAioBatchEn)
                queue_aio_batch_disable_check();
            return 0;
        case STREAM_OPT_AIO_BATCH_THRESH_PKTS:
	    pktThresh = val;
            if (val && qAioEn && !qAioBatchEn)
                queue_aio_batch_enable_check();
            else if (!val && qAioBatchEn)
                queue_aio_batch_disable_check();
            return 0;
        default:
            return -EINVAL;
        }
    }

    // get aio completion event of the queue
    int queue_poll_completion(int min_compl, int max_compl,
                              struct xclReqCompletion *comps, int* actual,
                              int timeout /*ms*/)
    {
        struct timespec time, *ptime = nullptr;
        int num_evt;

        *actual = 0;

        if (timeout > 0) {
            memset(&time, 0, sizeof(time));
            time.tv_sec = timeout / 1000;
            time.tv_nsec = (timeout % 1000) * 1000000;
            ptime = &time;
        }

        int rc = io_getevents(qAioCtx, min_compl, max_compl,
				(struct io_event *)comps, ptime);
        if (rc < 0)
            return rc;

        cbPollCnt += rc;
        num_evt = rc;
        for (int i = num_evt - 1; i >= 0; i--) {
            comps[i].priv_data = (void *)((struct io_event *)comps)[i].data;
            if (((struct io_event *)comps)[i].res < 0){
                /* error returned by AIO framework */
                comps[i].nbytes = 0;
                comps[i].err_code = ((struct io_event *)comps)[i].res;
            } else {
                comps[i].nbytes = ((struct io_event *)comps)[i].res;
                comps[i].err_code = ((struct io_event *)comps)[i].res2;
            }
        }

        if (rc < min_compl && qAioBatchEn) {
             /* timeout happened, check if there is any io submission errors */
             rc = check_io_submission_error(max_compl - num_evt, comps + num_evt);
             num_evt += rc;
        }

        *actual = num_evt;
        return 0;
    }

    // submit the read/write i/o to the queue
    ssize_t queue_submit_io(xclQueueRequest *wr, aio_context_t *mAioCtx)
    {
        ssize_t rc = 0;
        int error = 0;
        bool aio = (wr->flag & XCL_QUEUE_REQ_NONBLOCKING) ? true : false;

        if (qAioBatchEn) {
            /* queue up this async i/o request */
            if (aio) {
                return queue_up_request(wr);
            } else {
                /* this is synchronous i/o, flush all of the queued requests
                 * first to maintain the order */
                std::lock_guard<std::mutex> lk(reqLock);
                while (!reqList.empty()) {
   		    rc = queue_flush_aio_request();
                    if (rc < 0)
                        return rc;
                }

                /* fall through to process this request */
                rc = 0;
            }
        }

        /* synchronous i/o or no batching configured on the queue, submit the
         * i/o right away */
        struct xocl_qdma_req_header header;
        header.flags = wr->flag;
        if (aio) {
            aio_context_t *aio_ctx = qAioEn ? &qAioCtx : mAioCtx;
            for (unsigned int i = 0; i < wr->buf_num; i++) {
                struct iovec iov[2];
		struct iocb cb;
		struct iocb *cbs[1] = {&cb};

                prepare_io(&cb, iov, &header, wr->bufs[i].va, wr->bufs[i].len,
			 (uint64_t)wr->priv_data);
                error = io_submit(*aio_ctx, 1, cbs);
                if (error <= 0)
                    break;
                rc += wr->bufs[i].len;
            }
            std::lock_guard<std::mutex> lk(reqLock);
            cbSubmitCnt += wr->buf_num;
        } else {
            for (unsigned int i = 0; i < wr->buf_num; i++) {
                struct iovec iov[2];
		ssize_t rv;

                prepare_io(nullptr, iov, &header, wr->bufs[i].va, wr->bufs[i].len, 0);
                if (h2c)
                    rv = writev((int)qhndl, iov, 2);
                else
                    rv = readv((int)qhndl, iov, 2);

                if (rv < 0) {
			error = rv;
			break;
		}
		rc += rv;
            }
        }
        return (rc > 0) ? rc : error;
    }

}; /* queue_cb */

/*
 * shim()
 */
shim::
shim(unsigned index)
  : mCoreDevice(xrt_core::pcie_linux::get_userpf_device(this, index))
  , mUserHandle(-1)
  , mStreamHandle(-1)
  , mBoardNumber(index)
  , mOffsets{0x0, 0x0, OCL_CTLR_BASE, 0x0, 0x0}
  , mMemoryProfilingNumberSlots(0)
  , mAccelProfilingNumberSlots(0)
  , mStallProfilingNumberSlots(0)
  , mStreamProfilingNumberSlots(0)
  , mCmdBOCache(nullptr)
  , mCuMaps{128, {nullptr, 0, 0, 0}}
{
  init(index);
}

int shim::dev_init()
{
    auto dev = pcidev::get_dev(mBoardNumber);
    if(dev == nullptr) {
        xrt_logmsg(XRT_ERROR, "%s: Card [%d] not found", __func__, mBoardNumber);
        return -ENOENT;
    }

    drm_version version;
    const std::unique_ptr<char[]> name(new char[128]);
    const std::unique_ptr<char[]> desc(new char[512]);
    const std::unique_ptr<char[]> date(new char[128]);
    std::memset(&version, 0, sizeof(version));
    version.name = name.get();
    version.name_len = 128;
    version.desc = desc.get();
    version.desc_len = 512;
    version.date = date.get();
    version.date_len = 128;

    mUserHandle = dev->open("", O_RDWR);
    if (mUserHandle == -1)
        return -errno;

    int result = dev->ioctl(mUserHandle, DRM_IOCTL_VERSION, &version);
    if (result) {
        dev->close(mUserHandle);
        return -errno;
    }

    // We're good now.
    mDev = dev;
    (void) xclGetDeviceInfo2(&mDeviceInfo);
    mCmdBOCache = std::make_unique<xrt_core::bo_cache>(this, xrt_core::config::get_cmdbo_cache());

    mStreamHandle = mDev->open("dma.qdma", O_RDWR | O_SYNC);
    memset(&mAioContext, 0, sizeof(mAioContext));
    mAioEnabled = (io_setup(SHIM_QDMA_AIO_EVT_MAX, &mAioContext) == 0);

    return 0;
}

void shim::dev_fini()
{
    if (mStreamHandle > 0) {
        close(mStreamHandle);
        mStreamHandle = 0;
    }

    if (mAioEnabled) {
        io_destroy(mAioContext);
            mAioEnabled = false;
    }

    if (mUserHandle != -1)
        mDev->close(mUserHandle);
}

/*
 * init()
 */
void
shim::
init(unsigned int index)
{
    xrt_logmsg(XRT_INFO, "%s", __func__);

    int ret = dev_init();
    if (ret) {
        xrt_logmsg(XRT_WARNING, "dev_init failed: %d", ret);
        return;
    }

    // Profiling - defaults
    // Class-level defaults: mIsDebugIpLayoutRead = mIsDeviceProfiling = false
    mDevUserName = mDev->sysfs_name;
    mMemoryProfilingNumberSlots = 0;
}

/*
 * ~shim()
 */
shim::~shim()
{
    xrt_logmsg(XRT_INFO, "%s", __func__);
    // flush aie trace and write outputs
    xdp::aie::finish_flush_device(this) ;

    // The BO cache unmaps and releases all execbo, but this must
    // be done before the device is closed.
    mCmdBOCache.reset(nullptr);

    dev_fini();

    for (const auto& p : mCuMaps) {
        if (p.addr)
            (void) munmap(p.addr, p.size);
    }
}

/*
 * xclWrite()
 */
size_t shim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
    switch (space) {
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            if (mDev->pcieBarWrite(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
            const unsigned *reg = static_cast<const unsigned *>(hostBuf);
            size_t regSize = size / 4;
            if (regSize > 32)
            regSize = 32;
            for (unsigned i = 0; i < regSize; i++) {
                xrt_logmsg(XRT_INFO, "%s: space: %d, offset:0x%x, reg:%d",
                        __func__, space, offset+i, reg[i]);
            }
            if (mDev->pcieBarWrite(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_SPACE_DEVICE_CHECKER:
        default:
        {
            return -EPERM;
        }
    }
}

/*
 * xclRead()
 */
size_t shim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
    xrt_logmsg(XRT_INFO, "%s, space: %d, offset: %d, hostBuf: %s, size: %d",
            __func__, space, offset, hostBuf, size);

    switch (space) {
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            //offset += mOffsets[XCL_ADDR_SPACE_DEVICE_PERFMON];
            if (mDev->pcieBarRead(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
            int result = mDev->pcieBarRead(offset, hostBuf, size);
            const unsigned *reg = static_cast<const unsigned *>(hostBuf);
            size_t regSize = size / 4;
            if (regSize > 4)
            regSize = 4;
            for (unsigned i = 0; i < regSize; i++) {
                xrt_logmsg(XRT_INFO, "%s: space: %d, offset:0x%x, reg:%d",
                    __func__, space, offset+i, reg[i]);
            }
            return !result ? size : 0;
        }
        case XCL_ADDR_SPACE_DEVICE_CHECKER:
        {
            if (mDev->pcieBarRead(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        default:
        {
            return -EPERM;
        }
    }
}

/*
 * xclAllocBO()
 *
 * Assume that the memory is always created for the device ddr for now. Ignoring the flags as well.
 */
unsigned int shim::xclAllocBO(size_t size, int unused, unsigned flags)
{
    drm_xocl_create_bo info = {size, mNullBO, flags};
    unsigned int bo = mNullBO;
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_CREATE_BO, &info);
    if (result)
        errno = result;
    else
        bo = info.handle;
    return bo;
}

/*
 * xclAllocUserPtrBO()
 */
unsigned int shim::xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags)
{
    drm_xocl_userptr_bo user =
        {reinterpret_cast<uint64_t>(userptr), size, mNullBO, flags};
    unsigned int bo = mNullBO;
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_USERPTR_BO, &user);
    if (result)
        errno = result;
    else
        bo = user.handle;
    return bo;
}

/*
 * xclFreeBO()
 */
void shim::xclFreeBO(unsigned int boHandle)
{
    drm_gem_close closeInfo = {boHandle, 0};
    (void) mDev->ioctl(mUserHandle, DRM_IOCTL_GEM_CLOSE, &closeInfo);
}

/*
 * xclWriteBO()
 */
int shim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
    int ret;
    drm_xocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo);
    return ret ? -errno : ret;
}

/*
 * xclReadBO()
 */
int shim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
    int ret;
    drm_xocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo);
    return ret ? -errno : ret;
}

/*
 * xclMapBO()
 */
void *shim::xclMapBO(unsigned int boHandle, bool write)
{
    drm_xocl_info_bo info = { boHandle, 0, 0 };
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    if (result) {
        return nullptr;
    }

    drm_xocl_map_bo mapInfo = { boHandle, 0, 0 };
    result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_MAP_BO, &mapInfo);
    if (result) {
        return nullptr;
    }

    auto val = mDev->mmap(mUserHandle, info.size,
        (write ? (PROT_READ | PROT_WRITE) : PROT_READ), MAP_SHARED,
        mapInfo.offset);

    return val == reinterpret_cast<void*>(-1)
      ? nullptr
      : val;
}

/*
 * xclMapBO()
 */
int shim::xclUnmapBO(unsigned int boHandle, void* addr)
{
    drm_xocl_info_bo info = { boHandle, 0, 0 };
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    if (ret)
      return -errno;

    return mDev->munmap(mUserHandle, addr, info.size);
}

/*
 * xclSyncBO()
 */
int shim::xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
    drm_xocl_sync_bo_dir drm_dir = (dir == XCL_BO_SYNC_BO_TO_DEVICE) ?
            DRM_XOCL_SYNC_BO_TO_DEVICE :
            DRM_XOCL_SYNC_BO_FROM_DEVICE;
    drm_xocl_sync_bo syncInfo = {boHandle, 0, size, offset, drm_dir};
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    return ret ? -errno : ret;
}

int shim::execbufCopyBO(unsigned int dst_bo_handle,
    unsigned int src_bo_handle, size_t size, size_t dst_offset,
    size_t src_offset)
{
    auto bo = mCmdBOCache->alloc<ert_start_copybo_cmd>();
    ert_fill_copybo_cmd(bo.second, src_bo_handle, dst_bo_handle,
                        src_offset, dst_offset, size);

    int ret = xclExecBuf(bo.first);
    if (ret) {
        mCmdBOCache->release<ert_start_copybo_cmd>(bo);
        return ret;
    }

    do {
        ret = xclExecWait(1000);
        if (ret == -1)
            break;
    }
    while (bo.second->state < ERT_CMD_STATE_COMPLETED);

    ret = (ret == -1) ? -errno : 0;
    if (!ret && (bo.second->state != ERT_CMD_STATE_COMPLETED))
        ret = -EINVAL;

    mCmdBOCache->release<ert_start_copybo_cmd>(bo);
    return ret;
}

int shim::m2mCopyBO(unsigned int dst_bo_handle,
    unsigned int src_bo_handle, size_t size, size_t dst_offset,
    size_t src_offset)
{
    drm_xocl_copy_bo m2m = {
	    .dst_handle = dst_bo_handle,
	    .src_handle = src_bo_handle,
	    .size = size,
	    .dst_offset = dst_offset,
	    .src_offset = src_offset,
    };

    return mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_COPY_BO, &m2m) ? -errno : 0;
}

/*
 * xclCopyBO()
 */
int shim::xclCopyBO(unsigned int dst_bo_handle,
    unsigned int src_bo_handle, size_t size, size_t dst_offset,
    size_t src_offset)
{
    return (!mDev->get_sysfs_path("m2m", "").empty()) ?
        m2mCopyBO(dst_bo_handle, src_bo_handle, size, dst_offset, src_offset) :
        execbufCopyBO(dst_bo_handle, src_bo_handle, size, dst_offset, src_offset);
}

int shim::xclUpdateSchedulerStat()
{
    auto bo = mCmdBOCache->alloc<ert_packet>();
    bo.second->opcode = ERT_CU_STAT;
    bo.second->type = ERT_CTRL;

    int ret = xclExecBuf(bo.first);
    if (ret) {
        mCmdBOCache->release(bo);
        return ret;
    }

    do {
        ret = xclExecWait(1000);
        if (ret == -1)
            break;
    } while (bo.second->state < ERT_CMD_STATE_COMPLETED);

    ret = (ret == -1) ? -errno : 0;
    if (!ret && (bo.second->state != ERT_CMD_STATE_COMPLETED))
        ret = -EINVAL;
    mCmdBOCache->release<ert_packet>(bo);
    return ret;
}

/*
 * xclSysfsGetErrorStatus()
 */
void shim::xclSysfsGetErrorStatus(xclErrorStatus& stat)
{
    std::string errmsg;
    unsigned int status;
    unsigned int level;
    unsigned long time;

    mDev->sysfs_get<unsigned int>("firewall", "detected_status", errmsg, status, static_cast<unsigned int>(-1));
    mDev->sysfs_get<unsigned int>("firewall", "detected_level", errmsg, level, static_cast<unsigned int>(-1));
    mDev->sysfs_get<unsigned long>("firewall", "detected_time", errmsg, time, static_cast<unsigned long>(-1));

    stat.mNumFirewalls = XCL_FW_MAX_LEVEL;
    if (level < XCL_FW_MAX_LEVEL)
        stat.mFirewallLevel = level;
    for (unsigned i = 0; i < stat.mNumFirewalls; i++) {
        stat.mAXIErrorStatus[i].mErrFirewallID = static_cast<xclFirewallID>(i);
    }

    if (status && (level < ARRAY_SIZE(stat.mAXIErrorStatus))) {
        stat.mAXIErrorStatus[level].mErrFirewallStatus = status;
        stat.mAXIErrorStatus[level].mErrFirewallTime = time;
    }
}

/*
 * xclGetErrorStatus()
 */
int shim::xclGetErrorStatus(xclErrorStatus *info)
{
    xclSysfsGetErrorStatus(*info);
    return 0;
}

/*
 * xclSysfsGetDeviceInfo()
 */
void shim::xclSysfsGetDeviceInfo(xclDeviceInfo2 *info)
{
    std::string s;
    std::string errmsg;

    mDev->sysfs_get<unsigned short>("", "vendor", errmsg, info->mVendorId, static_cast<unsigned short>(-1));
    mDev->sysfs_get<unsigned short>("", "device", errmsg, info->mDeviceId, static_cast<unsigned short>(-1));
    mDev->sysfs_get<unsigned short>("", "subsystem_device", errmsg, info->mSubsystemId, static_cast<unsigned short>(-1));
    info->mDeviceVersion = info->mSubsystemId & 0xff;
    mDev->sysfs_get<unsigned short>("", "subsystem_vendor", errmsg, info->mSubsystemVendorId, static_cast<unsigned short>(-1));
    info->mDataAlignment = getpagesize();
    mDev->sysfs_get<size_t>("rom", "ddr_bank_size", errmsg, info->mDDRSize, static_cast<unsigned short>(-1));
    info->mDDRSize = GB(info->mDDRSize);

    mDev->sysfs_get("rom", "VBNV", errmsg, s);
    snprintf(info->mName, sizeof (info->mName), "%s", s.c_str());
    mDev->sysfs_get("rom", "FPGA", errmsg, s);
    snprintf(info->mFpga, sizeof (info->mFpga), "%s", s.c_str());
    mDev->sysfs_get<unsigned long long>("rom", "timestamp", errmsg, info->mTimeStamp, static_cast<unsigned long long>(-1));
    mDev->sysfs_get<unsigned short>("rom", "ddr_bank_count_max", errmsg, info->mDDRBankCount, static_cast<unsigned short>(-1));
    info->mDDRSize *= info->mDDRBankCount;
    info->mPciSlot = (mDev->domain<<16) + (mDev->bus<<8) + (mDev->dev<<3) + mDev->func;
    info->mNumClocks = numClocks(info->mName);
    info->mNumCDMA = xrt_core::device_query<xrt_core::query::kds_numcdmas>(mCoreDevice);

    mDev->sysfs_get("", "link_width", errmsg, info->mPCIeLinkWidth, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "link_speed", errmsg, info->mPCIeLinkSpeed, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "link_speed_max", errmsg, info->mPCIeLinkSpeedMax, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "link_width_max", errmsg, info->mPCIeLinkWidthMax, static_cast<unsigned short>(-1));

    //dont try to get any information which needs mailbox communication when device is not ready.
    if(!mDev->is_mgmt() && !mDev->is_ready)
        return;

    //get sensors
    unsigned int m12VPex, m12VAux, mPexCurr, mAuxCurr, mDimmTemp_0, mDimmTemp_1, mDimmTemp_2,
        mDimmTemp_3, mSE98Temp_0, mSE98Temp_1, mSE98Temp_2, mFanTemp, mFanRpm, m3v3Pex, m3v3Aux,
        mDDRVppBottom, mDDRVppTop, mSys5v5, m1v2Top, m1v8Top, m0v85, mMgt0v9, m12vSW, mMgtVtt,
        m1v2Bottom, mVccIntVol,mOnChipTemp;
    mDev->sysfs_get_sensor("xmc", "xmc_12v_pex_vol", m12VPex);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_aux_vol", m12VAux);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_pex_curr", mPexCurr);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_aux_curr", mAuxCurr);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp0", mDimmTemp_0);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp1", mDimmTemp_1);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp2", mDimmTemp_2);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp3", mDimmTemp_3);
    mDev->sysfs_get_sensor("xmc", "xmc_se98_temp0", mSE98Temp_0);
    mDev->sysfs_get_sensor("xmc", "xmc_se98_temp1", mSE98Temp_1);
    mDev->sysfs_get_sensor("xmc", "xmc_se98_temp2", mSE98Temp_2);
    mDev->sysfs_get_sensor("xmc", "xmc_fan_temp", mFanTemp);
    mDev->sysfs_get_sensor("xmc", "xmc_fan_rpm", mFanRpm);
    mDev->sysfs_get_sensor("xmc", "xmc_3v3_pex_vol", m3v3Pex);
    mDev->sysfs_get_sensor("xmc", "xmc_3v3_aux_vol", m3v3Aux);
    mDev->sysfs_get_sensor("xmc", "xmc_ddr_vpp_btm", mDDRVppBottom);
    mDev->sysfs_get_sensor("xmc", "xmc_ddr_vpp_top", mDDRVppTop);
    mDev->sysfs_get_sensor("xmc", "xmc_sys_5v5", mSys5v5);
    mDev->sysfs_get_sensor("xmc", "xmc_1v2_top", m1v2Top);
    mDev->sysfs_get_sensor("xmc", "xmc_1v8", m1v8Top);
    mDev->sysfs_get_sensor("xmc", "xmc_0v85", m0v85);
    mDev->sysfs_get_sensor("xmc", "xmc_mgt0v9avcc", mMgt0v9);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_sw", m12vSW);
    mDev->sysfs_get_sensor("xmc", "xmc_mgtavtt", mMgtVtt);
    mDev->sysfs_get_sensor("xmc", "xmc_vcc1v2_btm", m1v2Bottom);
    mDev->sysfs_get_sensor("xmc", "xmc_vccint_vol", mVccIntVol);
    mDev->sysfs_get_sensor("xmc", "xmc_fpga_temp", mOnChipTemp);
    info->m12VPex = m12VPex;
    info->m12VAux = m12VAux;
    info->mPexCurr = mPexCurr;
    info->mAuxCurr = mAuxCurr;
    info->mDimmTemp[0] = mDimmTemp_0;
    info->mDimmTemp[1] = mDimmTemp_1;
    info->mDimmTemp[2] = mDimmTemp_2;
    info->mDimmTemp[3] = mDimmTemp_3;
    info->mSE98Temp[0] = mSE98Temp_0;
    info->mSE98Temp[1] = mSE98Temp_1;
    info->mSE98Temp[2] = mSE98Temp_2;
    info->mFanTemp = mFanTemp;
    info->mFanRpm = mFanRpm;
    info->m3v3Pex = m3v3Pex;
    info->m3v3Aux = m3v3Aux;
    info->mDDRVppBottom = mDDRVppBottom;
    info->mDDRVppTop = mDDRVppTop;
    info->mSys5v5 = mSys5v5;
    info->m1v2Top = m1v2Top;
    info->m1v8Top = m1v8Top;
    info->m0v85 = m0v85;
    info->mMgt0v9 = mMgt0v9;
    info->m12vSW = m12vSW;
    info->mMgtVtt = mMgtVtt;
    info->m1v2Bottom = m1v2Bottom;
    info->mVccIntVol = mVccIntVol;
    info->mOnChipTemp = mOnChipTemp;

    //get sensors end

    mDev->sysfs_get("", "mig_calibration", errmsg, info->mMigCalib, false);
    std::vector<uint64_t> freqs;
    mDev->sysfs_get("icap", "clock_freqs", errmsg, freqs);
    for (unsigned i = 0;
        i < std::min(freqs.size(), ARRAY_SIZE(info->mOCLFrequency));
        i++) {
        info->mOCLFrequency[i] = freqs[i];
    }
}

/*
 * xclGetDeviceInfo2()
 */
int shim::xclGetDeviceInfo2(xclDeviceInfo2 *info)
{
    std::memset(info, 0, sizeof(xclDeviceInfo2));
    info->mMagic = 0X586C0C6C;
    info->mHALMajorVersion = XCLHAL_MAJOR_VER;
    info->mHALMinorVersion = XCLHAL_MINOR_VER;
    info->mMinTransferSize = DDR_BUFFER_ALIGNMENT;
    std::string errmsg;
    std::vector<std::string> dmaStatStrs;
    mDev->sysfs_get("dma", "channel_stat_raw", errmsg, dmaStatStrs);
    info->mDMAThreads = dmaStatStrs.size();
    xclSysfsGetDeviceInfo(info);
    return 0;
}

/*
 * resetDevice()
 */
int shim::resetDevice(xclResetKind kind)
{
    // Only XCL_USER_RESET is supported on user pf.
    if (kind != XCL_USER_RESET)
        return -EINVAL;

    std::string err;
    int dev_offline = 1;
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_HOT_RESET);
    if (ret)
        return -errno;

    dev_fini();

    // This loop used to wait for device come online and ready to use. It reads "dev_offline"
    // sysfs node to retrieve latest status of device in the interval of 500 milli sec.
    // In certain testing environement, reset never complete and waits indefinitely
    // in this loop for dev_offline status to be false. To overcome this infinite loop issue,
    // added loop_timer (default value set to 120 sec) which is used to exit the loop.
    unsigned int loop_timer = xrt_core::config::get_device_offline_timer();
    auto start = std::chrono::system_clock::now();
    while (dev_offline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        pcidev::get_dev(mBoardNumber)->sysfs_get<int>("",
            "dev_offline", err, dev_offline, -1);
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        if (elapsed_seconds.count() > loop_timer) {
            xrt_logmsg(XRT_WARNING, "%s: device unable to come online during reset, try again", __func__);
            ret = -EAGAIN;
        }
    }

    dev_init();

    return ret;
}

int shim::p2pEnable(bool enable, bool force)
{
    const std::string input = "1\n";
    std::string err;
    std::vector<std::string> p2p_cfg;
    int ret;

    if (mDev == nullptr)
        return -EINVAL;

    ret = check_p2p_config(mDev, err);
    if (ret == P2P_CONFIG_ENABLED && enable) {
        throw std::runtime_error("P2P is already enabled");
    } else if (ret == P2P_CONFIG_DISABLED && !enable) {
        throw std::runtime_error("P2P is already disabled");
    }

    /* write 0 to config for default bar size */
    if (enable) {
        mDev->sysfs_put("p2p", "p2p_enable", err, "1");
    } else {
        mDev->sysfs_put("p2p", "p2p_enable", err, "0");
    }
    if (!err.empty()) {
        throw std::runtime_error("P2P is not supported");
    }

    if (force) {
        dev_fini();
        /* remove root bus and rescan */
        mDev->sysfs_put("", "root_dev/remove", err, input);

        // initiate rescan "echo 1 > /sys/bus/pci/rescan"
        const std::string rescan_path = "/sys/bus/pci/rescan";
        std::ofstream rescanFile(rescan_path);
        if(!rescanFile.is_open()) {
            perror(rescan_path.c_str());
        } else {
            rescanFile << input;
        }
        dev_init();
    }

    ret = check_p2p_config(mDev, err);
    if (!err.empty()) {
        throw std::runtime_error(err);
    } else if (ret == P2P_CONFIG_DISABLED && enable) {
        throw std::runtime_error("Can not enable P2P");
    } else if (ret == P2P_CONFIG_ENABLED && !enable) {
        throw std::runtime_error("Can not disable P2P");
    }

    return 0;
}

int shim::cmaEnable(bool enable, uint64_t size)
{
    int ret = 0;

    if (enable) {
        /* Once set MAP_HUGETLB, we have to specify bit[26~31] as size in log
         * e.g. We like to get 2M huge page, 2M = 2^21,
         * 21 = 0x15
         * Let's find how many 1GB huge page we have to allocate
         */
        std::string errmsg;
        uint64_t hugepage_flag = 0x1e;
        uint64_t page_sz = 1 << 30;
        uint64_t allocated_size = 0;
        uint32_t page_num = size >> 30;
        drm_xocl_alloc_cma_info cma_info = {0};
        std::vector<uint64_t> user_addr(page_num, 0);

        /* We check the sysfs node host_mem_size first before going forward
         * If the same size of host memory chunk is allocated
         * then return 0 as SUCCESS
         */

        mDev->sysfs_get<uint64_t>("", "host_mem_size", errmsg, allocated_size, 0);
        if (allocated_size == size)
            return ret;

        cma_info.total_size = size;
        cma_info.entry_num = page_num;
        cma_info.user_addr = user_addr.data();

        for (uint32_t i = 0; i < page_num; ++i) {
            void *addr_local = mmap(0x0, page_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | hugepage_flag << MAP_HUGE_SHIFT, 0, 0);

            if (addr_local == MAP_FAILED) {
                ret = -ENOMEM;
                break;
            } else {
                cma_info.user_addr[i] = (uint64_t)addr_local;
            }
        }

        if (!ret) {
            ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_ALLOC_CMA, &cma_info);
            if (ret)
                    ret = -errno;
        }

        for (uint32_t i = 0; i < page_num; ++i) {
            if (!cma_info.user_addr[i])
                continue;

            munmap((void*)cma_info.user_addr[i], page_sz);
        }

        if (ret) {
            cma_info.entry_num = 0;
            ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_ALLOC_CMA, &cma_info);
            if (ret)
                    ret = -errno;
        }

    } else {
        ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_FREE_CMA);
        if (ret)
                ret = -errno;
    }

    return ret;
}
/*
 * xclLockDevice()
 */
bool
shim::
xclLockDevice()
{
  return true;
}

/*
 * xclUnlockDevice()
 */
bool
shim::
xclUnlockDevice()
{
  return true;
}

/*
 * xclReClock2()
 */
int shim::xclReClock2(unsigned short region, const unsigned short *targetFreqMHz)
{
    int ret;
    drm_xocl_reclock_info reClockInfo;
    std::memset(&reClockInfo, 0, sizeof(drm_xocl_reclock_info));
    reClockInfo.region = region;
    reClockInfo.ocl_target_freq[0] = targetFreqMHz[0];
    reClockInfo.ocl_target_freq[1] = targetFreqMHz[1];
    reClockInfo.ocl_target_freq[2] = targetFreqMHz[2];
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_RECLOCK, &reClockInfo);
    return ret ? -errno : ret;
}

/*
 * zeroOutDDR()
 */
bool shim::zeroOutDDR()
{
    // Zero out the DDR so MIG ECC believes we have touched all the bits
    // and it does not complain when we try to read back without explicit
    // write. The latter usually happens as a result of read-modify-write
    // TODO: Try and speed this up.
    // [1] Possibly move to kernel mode driver.
    // [2] Zero out specific buffers when they are allocated

    // TODO: Implement this
    //  static const unsigned long long BLOCK_SIZE = 0x4000000;
    //  void *buf = 0;
    //  if (posix_memalign(&buf, DDR_BUFFER_ALIGNMENT, BLOCK_SIZE))
    //      return false;
    //  memset(buf, 0, BLOCK_SIZE);
    //  mDataMover->pset64(buf, BLOCK_SIZE, 0, mDeviceInfo.mDDRSize/BLOCK_SIZE);
    //  free(buf);
    return true;
}

/*
 * xclLoadXclBin()
 */
int
shim::
xclLoadXclBin(const xclBin *buffer)
{
  xdp::hal::flush_device(this);
  xdp::aie::flush_device(this);
  xdp::pl_deadlock::flush_device(this);

  auto top = reinterpret_cast<const axlf*>(buffer);
  if (auto ret = xclLoadAxlf(top)) {
    // Something wrong, determine what
    if (ret == -EOPNOTSUPP) {
      xrt_logmsg(XRT_ERROR, "Xclbin does not match shell on card.");
      auto xclbin_vbnv = xrt_core::xclbin::get_vbnv(top);
      auto shell_vbnv = xrt_core::device_query<xrt_core::query::rom_vbnv>(mCoreDevice);
      if (xclbin_vbnv != shell_vbnv) {
        xrt_logmsg(XRT_ERROR, "Shell VBNV is '%s'", shell_vbnv.c_str());
        xrt_logmsg(XRT_ERROR, "Xclbin VBNV is '%s'", xclbin_vbnv.c_str());
      }
      xrt_logmsg(XRT_ERROR, "Use 'xbmgmt flash' to update shell.");
    }
    else if (ret == -EBUSY) {
      xrt_logmsg(XRT_ERROR, "Xclbin on card is in use, can't change.");
    }
    else if (ret == -EKEYREJECTED) {
      xrt_logmsg(XRT_ERROR, "Xclbin isn't signed properly");
    }
    else if (ret == -E2BIG) {
      xrt_logmsg(XRT_ERROR, "Not enough host_mem for xclbin");
    }
    else if (ret == -ETIMEDOUT) {
      xrt_logmsg(XRT_ERROR,
                 "Can't reach out to mgmt for xclbin downloading");
      xrt_logmsg(XRT_ERROR,
                 "Is xclmgmt driver loaded? Or is MSD/MPD running?");
    }
    else if (ret == -EDEADLK) {
      xrt_logmsg(XRT_ERROR, "CU was deadlocked? Hardware is not stable");
      xrt_logmsg(XRT_ERROR, "Please reset device with 'xbutil reset'");
    }
    xrt_logmsg(XRT_ERROR, "See dmesg log for details. err = %d", ret);
    return ret;
  }

  // Success
  mCoreDevice->register_axlf(buffer);

  xdp::hal::update_device(this);
  xdp::aie::update_device(this);
  xdp::pl_deadlock::update_device(this);

  START_DEVICE_PROFILING_CB(this);

  return 0;
}

/*
 * xclLoadAxlf()
 */
int shim::xclLoadAxlf(const axlf *buffer)
{
    xrt_logmsg(XRT_INFO, "%s, buffer: %s", __func__, buffer);
    drm_xocl_axlf axlf_obj = {const_cast<axlf *>(buffer), 0};
    unsigned int flags = XOCL_AXLF_BASE;
    int off = 0;

    auto force_program = xrt_core::config::get_force_program_xclbin(); //default value is false
    if(force_program) {
        axlf_obj.flags = flags | XOCL_AXLF_FORCE_PROGRAM;
    }

    auto kernels = xrt_core::xclbin::get_kernels(buffer);
    /* Calculate size of kernels */
    for (auto& kernel : kernels) {
        axlf_obj.ksize += sizeof(kernel_info) + sizeof(argument_info) * kernel.args.size();
    }

    /* To enhance CU subdevice and KDS/ERT, driver needs all details about kernels
     * while load xclbin.
     *
     * Why we extract data from XML metadata?
     *  1. Kernel is NOT a good place to parse xml. It prefers binary.
     *  2. All kernel details are in the xml today.
     *
     * What would happen in the future?
     *  XCLBIN would contain fdt as metadata. At that time, this
     *  could be removed.
     *
     * Binary format:
     * +-----------------------+
     * | Kernel[0]             |
     * |   name[64]            |
     * |   anums               |
     * |   argument[0]         |
     * |   argument[1]         |
     * |   argument[...]       |
     * |-----------------------|
     * | Kernel[1]             |
     * |   name[64]            |
     * |   anums               |
     * |   argument[0]         |
     * |   argument[1]         |
     * |   argument[...]       |
     * |-----------------------|
     * | Kernel[...]           |
     * |   ...                 |
     * +-----------------------+
     */
    std::vector<char> krnl_binary(axlf_obj.ksize);
    axlf_obj.kernels = krnl_binary.data();
    for (auto& kernel : kernels) {
        auto krnl = reinterpret_cast<kernel_info *>(axlf_obj.kernels + off);
        if (kernel.name.size() > sizeof(krnl->name))
            return -EINVAL;
        std::strncpy(krnl->name, kernel.name.c_str(), sizeof(krnl->name)-1);
        krnl->name[sizeof(krnl->name)-1] = '\0';
        krnl->anums = kernel.args.size();
        krnl->range = kernel.range;

        krnl->features = 0;
        if (kernel.sw_reset)
            krnl->features |= KRNL_SW_RESET;

        int ai = 0;
        for (auto& arg : kernel.args) {
            if (arg.name.size() > sizeof(krnl->args[ai].name)) {
                xrt_logmsg(XRT_ERROR, "%s: Argument name length %d>%d", __func__, arg.name.size(), sizeof(krnl->args[ai].name));
                return -EINVAL;
            }
            std::strncpy(krnl->args[ai].name, arg.name.c_str(), sizeof(krnl->args[ai].name)-1);
            krnl->args[ai].name[sizeof(krnl->args[ai].name)-1] = '\0';
            krnl->args[ai].offset = arg.offset;
            krnl->args[ai].size   = arg.size;
            // XCLBIN doesn't define argument direction yet and it only support
            // input arguments.
            // Driver use 1 for input argument and 2 for output.
            // Let's refine this line later.
            krnl->args[ai].dir    = 1;
            ai++;
        }
        off += sizeof(kernel_info) + sizeof(argument_info) * kernel.args.size();
    }

    /* To make download xclbin and configure KDS/ERT as an atomic operation. */
    axlf_obj.kds_cfg.ert = xrt_core::config::get_ert();
    axlf_obj.kds_cfg.polling = xrt_core::config::get_ert_polling();
    axlf_obj.kds_cfg.cu_dma = xrt_core::config::get_ert_cudma();
    axlf_obj.kds_cfg.cu_isr = xrt_core::config::get_ert_cuisr() && xrt_core::xclbin::get_cuisr(buffer);
    axlf_obj.kds_cfg.cq_int = xrt_core::config::get_ert_cqint();
    axlf_obj.kds_cfg.dataflow = xrt_core::config::get_feature_toggle("Runtime.dataflow") || xrt_core::xclbin::get_dataflow(buffer);
    axlf_obj.kds_cfg.rw_shared = xrt_core::config::get_rw_shared();

    /* TODO: In scheduler.cpp init() function, it use get_ert_slots(void) to get slot size.
     * But we cannot do this here, since the xclbin is not registered.
     * Currently, emulation flow use get_ert_slots() as well.
     * We will consider how to better determine slot size in new kds.
     */
    //axlf_obj.kds_cfg.slot_size = mCoreDevice->get_ert_slots().second;
    auto xml_hdr = xrt_core::xclbin::get_axlf_section(buffer, EMBEDDED_METADATA);
    if (!xml_hdr)
        throw std::runtime_error("No xml metadata in xclbin");
    auto xml_size = xml_hdr->m_sectionSize;
    auto xml_data = reinterpret_cast<const char*>(reinterpret_cast<const char*>(buffer) + xml_hdr->m_sectionOffset);
    axlf_obj.kds_cfg.slot_size = mCoreDevice->get_ert_slots(xml_data, xml_size).second;

    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_READ_AXLF, &axlf_obj);
    if (ret && errno == EAGAIN) {
        //special case for aws
        //if EAGAIN is seen, that means a pcie removal&rescan is ongoing, let's just
        //wait and reload 2nd time -- this time the there will be no device id
        //change, hence no pcie removal&rescan, anymore
	//we need to close the device otherwise the removal&rescan (unload driver) will hang
	//we also need to reopen the device once removal&rescan completes
        int dev_hotplug_done = 0;
        std::string err;
        dev_fini();
        std::this_thread::sleep_for(std::chrono::seconds(5));
        while (!dev_hotplug_done) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                pcidev::get_dev(mBoardNumber)->sysfs_get<int>("",
                "dev_hotplug_done", err, dev_hotplug_done, 0);
        }
        dev_init();
        ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_READ_AXLF, &axlf_obj);
    }

    if (ret)
        return -errno;

    // If it is an XPR DSA, zero out the DDR again as downloading the XCLBIN
    // reinitializes the DDR and results in ECC error.
    if(isXPR())
    {
        xrt_logmsg(XRT_INFO, "%s, XPR Device found, zeroing out DDR again..", __func__);

        if (zeroOutDDR() == false)
        {
            xrt_logmsg(XRT_ERROR, "%s, zeroing out DDR again..", __func__);
            return -EIO;
        }
    }
    return ret;
}

/*
 * xclExportBO()
 */
int shim::xclExportBO(unsigned int boHandle)
{
    drm_prime_handle info = {boHandle, DRM_RDWR, -1};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
    if (result) {
        xrt_logmsg(XRT_WARNING, "%s: DRM prime handle to fd failed with DRM_RDWR. Trying default flags.", __func__);
        info.flags = 0;
        result = ioctl(mUserHandle, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
    }

    xrt_logmsg(XRT_DEBUG, "%s: boHandle %d, ioctl return %ld, fd %d", __func__, boHandle, result, info.fd);
    return !result ? info.fd : result;
}

/*
 * xclImportBO()
 */
unsigned int shim::xclImportBO(int fd, unsigned flags)
{
    drm_prime_handle info = {mNullBO, flags, fd};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_PRIME_FD_TO_HANDLE, &info);
    if (result) {
        xrt_logmsg(XRT_ERROR, "%s: FD to handle IOCTL failed", __func__);
    }
    return !result ? info.handle : mNullBO;
}

/*
 * xclGetBOProperties()
 */
int shim::xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
    drm_xocl_info_bo info = {boHandle, 0, mNullBO, mNullAddr};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    properties->handle = info.handle;
    properties->flags  = info.flags;
    properties->size   = info.size;
    properties->paddr  = info.paddr;
    return result ? -errno : result;
}

int shim::xclGetSectionInfo(void* section_info, size_t * section_size,
    enum axlf_section_kind kind, int index)
{
    if(section_info == nullptr || section_size == nullptr)
        return -EINVAL;

    std::string entry;
    if(kind == MEM_TOPOLOGY)
        entry = "mem_topology";
    else if (kind == CONNECTIVITY)
        entry = "connectivity";
    else if (kind == IP_LAYOUT)
        entry = "ip_layout";
    else {
        xrt_logmsg(XRT_ERROR, "%s: Unhandled section found", __func__);
        return -EINVAL;
    }

    std::string err;
    std::vector<char> buf;
    mDev->sysfs_get("icap", entry, err, buf);
    if (!err.empty()) {
        xrt_logmsg(XRT_ERROR, "%s: %s", __func__, err.c_str());
        return -EINVAL;
    }

    char* memblock = buf.data();

    if(kind == MEM_TOPOLOGY) {
        mem_topology* mem = reinterpret_cast<mem_topology *>(memblock);
        if (index > (mem->m_count -1))
            return -EINVAL;
        memcpy(section_info, &mem->m_mem_data[index], sizeof(mem_data));
        *section_size = sizeof (mem_data);
    } else if (kind == CONNECTIVITY) {
        connectivity* con = reinterpret_cast<connectivity *>(memblock);
        if (index > (con->m_count -1))
            return -EINVAL;
        memcpy(section_info, &con->m_connection[index], sizeof(connection));
        *section_size = sizeof (connection);
    } else if(kind == IP_LAYOUT) {
        ip_layout* ip = reinterpret_cast<ip_layout *>(memblock);
        if (index > (ip->m_count -1))
            return -EINVAL;
        memcpy(section_info, &ip->m_ip_data[index], sizeof(ip_data));
        *section_size = sizeof (ip_data);
    }

    return 0;
}

/*
 * xclSysfsGetUsageInfo()
 */
void shim::xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat)
{
    std::string errmsg;
    std::vector<std::string> dmaStatStrs;
    std::vector<std::string> mmStatStrs;
    std::vector<std::string> xmcStatStrs;
    mDev->sysfs_get("dma", "channel_stat_raw", errmsg, dmaStatStrs);
    mDev->sysfs_get("", "memstat_raw", errmsg, mmStatStrs);
    mDev->sysfs_get("microblaze", "version", errmsg, xmcStatStrs);
    if (!dmaStatStrs.empty()) {
        stat.dma_channel_count = dmaStatStrs.size();
        for (unsigned i = 0;
            i < std::min(dmaStatStrs.size(), ARRAY_SIZE(stat.c2h));
            i++) {
            std::stringstream ss(dmaStatStrs[i]);
            ss >> stat.c2h[i] >> stat.h2c[i];
        }
    }

    if (!mmStatStrs.empty()) {
        stat.mm_channel_count = mmStatStrs.size();
        for (unsigned i = 0;
            i < std::min(mmStatStrs.size(), ARRAY_SIZE(stat.mm));
            i++) {
            std::stringstream ss(mmStatStrs[i]);
            ss >> stat.mm[i].memory_usage >> stat.mm[i].bo_count;
        }
    }
}

/*
 * xclGetUsageInfo()
 */
int shim::xclGetUsageInfo(xclDeviceUsage *info)
{
    drm_xocl_usage_stat stat = { 0 };

    xclSysfsGetUsageInfo(stat);
    std::memset(info, 0, sizeof(xclDeviceUsage));
    std::memcpy(info->h2c, stat.h2c, sizeof(size_t) * 8);
    std::memcpy(info->c2h, stat.c2h, sizeof(size_t) * 8);
    for (int i = 0; i < 8; i++) {
        info->ddrMemUsed[i] = stat.mm[i].memory_usage;
        info->ddrBOAllocated[i] = stat.mm[i].bo_count;
    }
    info->dma_channel_cnt = stat.dma_channel_count;
    info->mm_channel_cnt = stat.mm_channel_count;
    return 0;
}

/*
 * isGood()
 */
bool shim::isGood() const
{
    return (mDev != nullptr);
}

/*
 * handleCheck()
 *
 * Returns pointer to valid handle on success, 0 on failure.
 */
xocl::shim*
shim::
handleCheck(void* handle)
{
  if (!handle)
    return 0;

  auto shim = reinterpret_cast<xocl::shim*>(handle);
  if (!shim->isGood() || shim->mUserHandle == -1)
    return 0;

  return shim;
}

/*
 * xclUnmgdPwrite()
 */
ssize_t shim::xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset)
{
    if (flags) {
        return -EINVAL;
    }
    drm_xocl_pwrite_unmgd unmgd = {0, 0, offset, count, reinterpret_cast<uint64_t>(buf)};
    return mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_UNMGD, &unmgd);
}

/*
 * xclUnmgdPread()
 */
ssize_t shim::xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
{
    if (flags) {
        return -EINVAL;
    }
    drm_xocl_pread_unmgd unmgd = {0, 0, offset, count, reinterpret_cast<uint64_t>(buf)};
    return mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_UNMGD, &unmgd);
}

/*
 * xclExecBuf()
 */
int shim::xclExecBuf(unsigned int cmdBO)
{
    int ret;
    xrt_logmsg(XRT_INFO, "%s, cmdBO: %d", __func__, cmdBO);
    drm_xocl_execbuf exec = {0, cmdBO, 0,0,0,0,0,0,0,0};
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
    return ret ? -errno : ret;
}

/*
 * xclExecBuf()
 */
int shim::xclExecBuf(unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    xrt_logmsg(XRT_INFO, "%s, cmdBO: %d, num_bo_in_wait_list: %d, bo_wait_list: %d",
            __func__, cmdBO, num_bo_in_wait_list, bo_wait_list);

    // New KDS does not support xclExecBufWithWaitList().
    xrt_logmsg(XRT_ERROR, "xclExecBufWithWaitList() is no longer supported.");
    return -ENOTSUP;
}

/*
 * xclRegisterEventNotify()
 */
int shim::xclRegisterEventNotify(unsigned int userInterrupt, int fd)
{
    int ret ;
    drm_xocl_user_intr userIntr = {0, fd, (int)userInterrupt};
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_USER_INTR, &userIntr);
    return ret ? -errno : ret;
}

/*
 * xclExecWait()
 */
int shim::xclExecWait(int timeoutMilliSec)
{
    return mDev->poll(mUserHandle, POLLIN, timeoutMilliSec);
}

// xclOpenContext
int
shim::
xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared) const
{
    unsigned int flags = shared ? XOCL_CTX_SHARED : XOCL_CTX_EXCLUSIVE;
    drm_xocl_ctx ctx = {XOCL_CTX_OP_ALLOC_CTX};
    std::memcpy(ctx.xclbin_id, xclbinId, sizeof(uuid_t));
    ctx.cu_index = ipIndex;
    ctx.flags = flags;
    if (mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx))
      throw xrt_core::system_error(errno, "failed to open ip context");

    return 0;
}

/*
 * xclCloseContext
 */
int shim::xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex)
{
    std::lock_guard<std::mutex> l(mCuMapLock);

    if (ipIndex < mCuMaps.size()) {
	    // Make sure no MMIO register space access when CU is released.
        if (auto p = mCuMaps[ipIndex].addr) {
            std::ignore = munmap(p, mCuMaps[ipIndex].size);
            mCuMaps[ipIndex] = {nullptr, 0, 0, 0};
        }
    }

    drm_xocl_ctx ctx = {XOCL_CTX_OP_FREE_CTX};
    std::memcpy(ctx.xclbin_id, xclbinId, sizeof(uuid_t));
    ctx.cu_index = ipIndex;
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx);
    return ret ? -errno : ret;
}

/*
 * xclBootFPGA()
 */
int shim::xclBootFPGA()
{
    return -EOPNOTSUPP;
}

uint32_t shim::xclGetNumLiveProcesses()
{
    std::string errmsg;

    std::vector<std::string> stringVec;
    mDev->sysfs_get("", "kdsstat", errmsg, stringVec);
    // Dependent on message format built in kdsstat_show. Checking number of
    // "context" in kdsstat.
    // kdsstat has "context: <number_of_live_processes>"
    if(stringVec.size() >= 4) {
        auto p = stringVec[3].find_first_of("0123456789");
        auto subStr = stringVec[3].substr(p);
        auto number = std::stoul(subStr);
        return number;
    }
    return 0;
}

int shim::xclGetDebugIPlayoutPath(char* layoutPath, size_t size)
{
  return xclGetSysfsPath("icap", "debug_ip_layout", layoutPath, size);
}


int shim::xclGetSubdevPath(const char* subdev, uint32_t idx, char* path, size_t size)
{
    auto dev = pcidev::get_dev(mBoardNumber);
    std::string subdev_str = std::string(subdev);

    if (mLogStream.is_open()) {
      mLogStream << "Retrieving [devfs root]";
      mLogStream << subdev_str << "/" << idx;
      mLogStream << std::endl;
    }
    std::string sysfsFullPath = dev->get_subdev_path(subdev_str, idx);
    strncpy(path, sysfsFullPath.c_str(), size);
    path[size - 1] = '\0';
    return 0;
}

int shim::xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  uint32_t bytesPerSample = (xdp::TRACE_FIFO_WORD_WIDTH / 8);

  traceBufSz = xdp::MAX_TRACE_NUMBER_SAMPLES_FIFO * bytesPerSample;   /* Buffer size in bytes */
  traceSamples = nSamples;

  return 0;
}

int shim::xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
    // Create trace buffer on host (requires alignment)
    const int traceBufWordSz = traceBufSz / 4;  // traceBufSz is in number of bytes

    uint32_t size = 0;

    wordsPerSample = (xdp::TRACE_FIFO_WORD_WIDTH / 32);
    uint32_t numWords = numSamples * wordsPerSample;

//    alignas is defined in c++11
#if GCC_VERSION >= 40800
    /* Alignment is limited to 16 by PPC64LE : so , should it be
    alignas(16) uint32_t hostbuf[traceBufSzInWords];
    */
    alignas(xdp::IP::FIFO::alignment) uint32_t hostbuf[traceBufWordSz];
#else
    xrt_core::AlignedAllocator<uint32_t> alignedBuffer(xdp::IP::FIFO::alignment, traceBufWordSz);
    uint32_t* hostbuf = alignedBuffer.getBuffer();
#endif

    // Now read trace data
    memset((void *)hostbuf, 0, traceBufSz);

    // Iterate over chunks
    // NOTE: AXI limits this to 4K bytes per transfer
    uint32_t chunkSizeWords = 256 * wordsPerSample;
    if (chunkSizeWords > 1024) chunkSizeWords = 1024;
    uint32_t chunkSizeBytes = 4 * chunkSizeWords;
    uint32_t words=0;

    // Read trace a chunk of bytes at a time
    if (numWords > chunkSizeWords) {
      for (; words < (numWords-chunkSizeWords); words += chunkSizeWords) {
          if(mLogStream.is_open())
            mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
		       << std::hex << (ipBaseAddress + xdp::IP::FIFO::AXI_LITE::RDFD) /*fifoReadAddress[0] or AXI_FIFO_RDFD*/ << " and writing it to 0x"
                          << (void *)(hostbuf + words) << std::dec << std::endl;

	  xclUnmgdPread(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, ipBaseAddress + xdp::IP::FIFO::AXI_LITE::RDFD /*offset : or AXI_FIFO_RDFD*/);

        size += chunkSizeBytes;
      }
    }

    // Read remainder of trace not divisible by chunk size
    if (words < numWords) {
      chunkSizeBytes = 4 * (numWords - words);

      if(mLogStream.is_open()) {
        mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
		   << std::hex << (ipBaseAddress + xdp::IP::FIFO::AXI_LITE::RDFD) /*fifoReadAddress[0]*/ << " and writing it to 0x"
                      << (void *)(hostbuf + words) << std::dec << std::endl;
      }

      xclUnmgdPread(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, ipBaseAddress + xdp::IP::FIFO::AXI_LITE::RDFD /*offset : or AXI_FIFO_RDFD*/);

      size += chunkSizeBytes;
    }

    if(mLogStream.is_open())
        mLogStream << __func__ << ": done reading " << size << " bytes " << std::endl;

    memcpy((char*)traceBuf, (char*)hostbuf, traceBufSz);

    return size;
}

// Get the device clock frequency (in MHz)
double shim::xclGetDeviceClockFreqMHz()
{
  xclGetDeviceInfo2(&mDeviceInfo);
  unsigned short clockFreq = mDeviceInfo.mOCLFrequency[0];
  if (clockFreq == 0)
    clockFreq = 300;

  //if (mLogStream.is_open())
  //  mLogStream << __func__ << ": clock freq = " << clockFreq << std::endl;
  return ((double)clockFreq);
}

// For PCIe gen 3x16 or 4x8:
// Max BW = 16.0 * (128b/130b encoding) = 15.75385 GB/s
double shim::xclGetHostReadMaxBandwidthMBps()
{
  return 15753.85;
}

// For PCIe gen 3x16 or 4x8:
// Max BW = 16.0 * (128b/130b encoding) = 15.75385 GB/s
double shim::xclGetHostWriteMaxBandwidthMBps()
{
  return 15753.85;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double shim::xclGetKernelReadMaxBandwidthMBps()
{
  return 19250.00;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double shim::xclGetKernelWriteMaxBandwidthMBps()
{
  return 19250.00;
}

int shim::xclGetSysfsPath(const char* subdev, const char* entry, char* sysfsPath, size_t size)
{
  auto dev = pcidev::get_dev(mBoardNumber);
  std::string subdev_str = std::string(subdev);
  std::string entry_str = std::string(entry);
  if (mLogStream.is_open()) {
    mLogStream << "Retrieving [sysfs root]";
    mLogStream << subdev_str << "/" << entry_str;
    mLogStream << std::endl;
  }
  std::string sysfsFullPath = dev->get_sysfs_path(subdev_str, entry_str);
  strncpy(sysfsPath, sysfsFullPath.c_str(), size);
  sysfsPath[size - 1] = '\0';
  return 0;
}

int shim::xclRegRW(bool rd, uint32_t ipIndex, uint32_t offset, uint32_t *datap)
{
  std::lock_guard<std::mutex> lk(mCuMapLock);

  if (ipIndex >= mCuMaps.size()) {
    xrt_logmsg(XRT_ERROR, "%s: invalid CU index: %d", __func__, ipIndex);
    return -EINVAL;
  }

  auto& cumap = mCuMaps[ipIndex];  // {base, size, start, end}

  if (cumap.addr == nullptr) {
    auto cu_subdev = "CU[" + std::to_string(ipIndex) + "]";
    auto size = xrt_core::device_query<xq::cu_size>(mCoreDevice, xq::request::modifier::subdev, cu_subdev);
    if (size <= 0 || size > 0x10000) {
      xrt_logmsg(XRT_ERROR, "%s: incorrect cu size %d", __func__, size);
      return -EINVAL;
    }
    auto range_str = xrt_core::device_query<xq::cu_read_range>(mCoreDevice, xq::request::modifier::subdev, cu_subdev);
    auto range = xq::cu_read_range::to_range(range_str);

    void *p = mDev->mmap(mUserHandle, size, PROT_READ | PROT_WRITE,
                         MAP_SHARED, static_cast<off_t>(ipIndex + 1) * getpagesize());
    if (p != MAP_FAILED) {
      cumap.addr = static_cast<uint32_t*>(p);
      cumap.size = size;
      cumap.start = range.start;
      cumap.end = range.end;
    }

    if (cumap.addr == nullptr) {
      xrt_logmsg(XRT_ERROR, "%s: can't map CU: %d", __func__, ipIndex);
      return -EINVAL;
    }
  }

  if ((offset & (sizeof(uint32_t) - 1)) != 0) {
    xrt_logmsg(XRT_ERROR, "%s: offset is not aligned in word: %d", __func__, offset);
    return -EINVAL;
  }

  if (offset >= cumap.size) {
    xrt_logmsg(XRT_ERROR, "%s: invalid CU offset: %d", __func__, offset);
    return -EINVAL;
  }

  if (cumap.start) {
    if (!rd) {
        xrt_logmsg(XRT_ERROR, "%s: read range is set, not allow write", __func__);
        return -EINVAL;
    }

    if ((cumap.start > offset) || (cumap.end < offset)) {
        xrt_logmsg(XRT_ERROR, "%s: CU offset %d out of read range, %d, %d", __func__, offset, cumap.start, cumap.end);
        return -EINVAL;
    }
  }

  if (rd)
    *datap = (cumap.addr)[offset / sizeof(uint32_t)];
  else
    (cumap.addr)[offset / sizeof(uint32_t)] = *datap;

  return 0;
}

int shim::xclIPSetReadRange(uint32_t ipIndex, uint32_t start, uint32_t size)
{
    int ret = 0;
    drm_xocl_set_cu_range range = {ipIndex, start, size};

    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_SET_CU_READONLY_RANGE, &range);
    return ret ? -errno : ret;
}

int shim::xclRegRead(uint32_t ipIndex, uint32_t offset, uint32_t *datap)
{
    return xclRegRW(true, ipIndex, offset, datap);
}

int shim::xclRegWrite(uint32_t ipIndex, uint32_t offset, uint32_t data)
{
    return xclRegRW(false, ipIndex, offset, &data);
}

int shim::xclIPName2Index(const char *name)
{
    // In new kds, driver determines CU index
    try {
      for (auto& stat : xrt_core::device_query<xrt_core::query::kds_cu_info>(mCoreDevice))
        if (stat.name == name)
          return stat.index;

      xrt_logmsg(XRT_ERROR, "%s not found", name);
      return -ENOENT;
    }
    catch (const xrt_core::query::no_such_key&) {
    }

    // Old kds is enabled
    std::string errmsg;
    std::vector<char> buf;
    const uint64_t bad_addr = 0xffffffffffffffff;

    mDev->sysfs_get("icap", "ip_layout", errmsg, buf);
    if (!errmsg.empty()) {
        xrt_logmsg(XRT_ERROR, "can't read ip_layout sysfs node: %s",
            errmsg.c_str());
        return -EINVAL;
    }
    if (buf.empty())
        return -ENOENT;

    const ip_layout *map = (ip_layout *)buf.data();
    if(map->m_count < 0) {
        xrt_logmsg(XRT_ERROR, "invalid ip_layout sysfs node content");
        return -EINVAL;
    }

    uint64_t addr = bad_addr;
    int i;
    for(i = 0; i < map->m_count; i++) {
        if (strncmp((char *)map->m_ip_data[i].m_name, name,
                sizeof(map->m_ip_data[i].m_name)) == 0) {
            addr = map->m_ip_data[i].m_base_address;
            break;
        }
    }
    if (i == map->m_count)
        return -ENOENT;
    if (addr == bad_addr)
        return -EINVAL;

    auto cus = xrt_core::xclbin::get_cus(map);
    auto itr = std::find(cus.begin(), cus.end(), addr);
    if (itr == cus.end())
      return -ENOENT;

    return std::distance(cus.begin(),itr);
}

int shim::xclOpenIPInterruptNotify(uint32_t ipIndex, unsigned int flags)
{
    int ret;

    drm_xocl_ctx ctx;
    ctx.cu_index = ipIndex;
    ctx.flags = flags;
    ctx.op = XOCL_CTX_OP_OPEN_UCU_FD;

    xrt_logmsg(XRT_DEBUG, "%s: IP index %d, flags 0x%x", __func__, ipIndex, flags);
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx);
    return (ret < 0) ? -errno : ret;
}

int shim::xclCloseIPInterruptNotify(int fd)
{
    xrt_logmsg(XRT_DEBUG, "%s: fd %d", __func__, fd);
    close(fd);
    return 0;
}

// open_context() - aka xclOpenContextByName
xrt_core::cuidx_type
shim::
open_cu_context(const xrt::hw_context& hwctx, const std::string& cuname)
{
  // Alveo Linux PCIE does not yet support multiple xclbins.  Call
  // regular flow.  Default access mode to shared unless explicitly
  // exclusive.
  auto shared = (hwctx.get_mode() != xrt::hw_context::access_mode::exclusive);
  auto ctxhdl = static_cast<xcl_hwctx_handle>(hwctx);
  auto cuidx = mCoreDevice->get_cuidx(ctxhdl, cuname);
  xclOpenContext(hwctx.get_xclbin_uuid().get(), cuidx.index, shared);

  return cuidx;
}

void
shim::
close_cu_context(const xrt::hw_context& hwctx, xrt_core::cuidx_type cuidx)
{
  // To-be-implemented
  if (xclCloseContext(hwctx.get_xclbin_uuid().get(), cuidx.index))
    throw xrt_core::system_error(errno, "failed to close cu context (" + std::to_string(cuidx.index) + ")");
}

// Assign xclbin with uuid to hardware resources and return a context id
// The context handle is 1:1 with a slot idx
uint32_t
shim::
create_hw_context(const xrt::uuid& xclbin_uuid,
                  const xrt::hw_context::qos_type& qos,
                  xrt::hw_context::access_mode mode)
{
  // Explicit hardware contexts are not supported in Alveo.
  throw xrt_core::ishim::not_supported_error{__func__};
}

void
shim::
destroy_hw_context(uint32_t ctxhdl)
{
  // Explicit hardware contexts are not supported in Alveo.
  throw xrt_core::ishim::not_supported_error{__func__};
}

// Registers an xclbin, but does not load it.
void
shim::
register_xclbin(const xrt::xclbin&)
{
  // Explicit hardware contexts are not supported in Alveo.
  throw xrt_core::ishim::not_supported_error{__func__};
}

} // namespace xocl

////////////////////////////////////////////////////////////////
// Implementation of internal SHIM APIs
////////////////////////////////////////////////////////////////
namespace xrt::shim_int {

xclDeviceHandle
open_by_bdf(const std::string& bdf)
{
  return xclOpen(xrt_core::pcie_linux::get_device_id_from_bdf(bdf), nullptr, XCL_QUIET);
}

xrt_core::cuidx_type
open_cu_context(xclDeviceHandle handle, const xrt::hw_context& hwctx, const std::string& cuname)
{
  auto shim = get_shim_object(handle);
  return shim->open_cu_context(hwctx, cuname);
}

void
close_cu_context(xclDeviceHandle handle, const xrt::hw_context& hwctx, xrt_core::cuidx_type cuidx)
{
  auto shim = get_shim_object(handle);
  return shim->close_cu_context(hwctx, cuidx);
}

uint32_t // ctxhdl aka slotidx
create_hw_context(xclDeviceHandle handle,
                  const xrt::uuid& xclbin_uuid,
                  const xrt::hw_context::qos_type& qos,
                  const xrt::hw_context::access_mode mode)
{
  auto shim = get_shim_object(handle);
  return shim->create_hw_context(xclbin_uuid, qos, mode);
}

void
destroy_hw_context(xclDeviceHandle handle, uint32_t ctxhdl)
{
  auto shim = get_shim_object(handle);
  shim->destroy_hw_context(ctxhdl);
}

void
register_xclbin(xclDeviceHandle handle, const xrt::xclbin& xclbin)
{
  auto shim = get_shim_object(handle);
  shim->register_xclbin(xclbin);
}


} // xrt::shim_int
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// Implementation of user exposed SHIM APIs
// This are C level functions
////////////////////////////////////////////////////////////////
unsigned int
xclProbe()
{
  return xdp::hal::profiling_wrapper("xclProbe", [] {
    return pcidev::get_dev_ready();
  }) ;
}

xclDeviceHandle
xclOpen(unsigned int deviceIndex, const char*, xclVerbosityLevel)
{
  return xdp::hal::profiling_wrapper("xclOpen", [deviceIndex] {
  try {
    if(pcidev::get_dev_total() <= deviceIndex) {
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
        std::string("Cannot find index " + std::to_string(deviceIndex) + " \n"));
      return static_cast<xclDeviceHandle>(nullptr);
    }

    xocl::shim *handle = new xocl::shim(deviceIndex);

    if (handle->handleCheck(handle) == 0) {
      xrt_core::send_exception_message(strerror(errno) +
        std::string(" Device index ") + std::to_string(deviceIndex));
      return static_cast<xclDeviceHandle>(nullptr);
    }

    return static_cast<xclDeviceHandle>(handle);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }

  return static_cast<xclDeviceHandle>(nullptr);
  }) ;
}

void xclClose(xclDeviceHandle handle)
{
  xdp::hal::profiling_wrapper("xclClose", [handle] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (drv) {
        delete drv;
        return;
    }
  }) ;
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
  return xdp::hal::profiling_wrapper("xclLoadXclBin", [handle, buffer] {
    try {
      auto drv = xocl::shim::handleCheck(handle);
      if (!drv)
        return -EINVAL;

      return drv->xclLoadXclBin(buffer);
    }
    catch (const xrt_core::error& ex) {
      xrt_core::send_exception_message(ex.what());
      return ex.get_code();
    }
    catch (const std::exception& ex) {
      xrt_core::send_exception_message(ex.what());
      return -EINVAL;
    }
  });
}

int xclLogMsg(xclDeviceHandle, xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
    static auto verbosity = xrt_core::config::get_verbosity();
    if (level > verbosity)
      return 0;

    va_list args;
    va_start(args, format);
    xrt_core::message::sendv(static_cast<xrt_core::message::severity_level>(level), tag, format, args);
    va_end(args);
    return 0;
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
  return xdp::hal::profiling_wrapper("xclWrite",
  [handle, space, offset, hostBuf, size] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclWrite(space, offset, hostBuf, size) : -ENODEV;
  }) ;
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
  return xdp::hal::profiling_wrapper("xclRead",
  [handle, space, offset, hostBuf, size] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRead(space, offset, hostBuf, size) : -ENODEV;
  }) ;
}

int xclRegWrite(xclDeviceHandle handle, uint32_t ipIndex, uint32_t offset, uint32_t data)
{
  return xdp::hal::profiling_wrapper("xclRegWrite",
  [handle, ipIndex, offset, data] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRegWrite(ipIndex, offset, data) : -ENODEV;
  }) ;
}

int xclRegRead(xclDeviceHandle handle, uint32_t ipIndex, uint32_t offset, uint32_t *datap)
{
  return xdp::hal::profiling_wrapper("xclRegRead",
  [handle, ipIndex, offset, datap] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRegRead(ipIndex, offset, datap) : -ENODEV;
  }) ;
}

int xclGetErrorStatus(xclDeviceHandle handle, xclErrorStatus *info)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    std::memset(info, 0, sizeof(xclErrorStatus));
    if(!drv)
        return 0;
    return drv->xclGetErrorStatus(info);
}

int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
    xocl::shim *drv = (xocl::shim *) handle;
    return drv ? drv->xclGetDeviceInfo2(info) : -ENODEV;
}

unsigned int xclVersion ()
{
    return 2;
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
  return xdp::hal::profiling_wrapper("xclAllocBO",
                              [handle, size, unused, flags]
                              {
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclAllocBO(size, unused, flags) : -ENODEV;
                              } ) ;


}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
  return xdp::hal::profiling_wrapper("xclAllocUserPtrBO",
    [handle, userptr, size, flags] {
      xocl::shim *drv = xocl::shim::handleCheck(handle);
      return drv ? drv->xclAllocUserPtrBO(userptr, size, flags) : -ENODEV;
    } ) ;
}

void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle) {

  xdp::hal::profiling_wrapper("xclFreeBO",
    [handle, boHandle] {
      xocl::shim *drv = xocl::shim::handleCheck(handle);
      if (!drv) {
          return;
      }
      drv->xclFreeBO(boHandle);
    }) ;
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek)
{
  return xdp::hal::buffer_transfer_profiling_wrapper("xclWriteBO", size, true,
  [handle, boHandle, src, size, seek] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclWriteBO(boHandle, src, size, seek) : -ENODEV;
  });
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip)
{
  return xdp::hal::buffer_transfer_profiling_wrapper("xclReadBO", size, false,
  [handle, boHandle, dst, size, skip] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclReadBO(boHandle, dst, size, skip) : -ENODEV;
  });
}

void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{

  return xdp::hal::profiling_wrapper("xclMapBO", [handle, boHandle, write] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclMapBO(boHandle, write) : nullptr;
  }) ;
}

int xclUnmapBO(xclDeviceHandle handle, unsigned int boHandle, void* addr)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclUnmapBO(boHandle, addr) : -ENODEV;
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  return xdp::hal::buffer_transfer_profiling_wrapper("xclSyncBO", size,
                                              (dir == XCL_BO_SYNC_BO_TO_DEVICE),
  [handle, boHandle, dir, size, offset] {

    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (size == 0) {
      //Nothing to do
      return 0;
    }
    return drv ? drv->xclSyncBO(boHandle, dir, size, offset) : -ENODEV;
   }) ;
}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle,
            unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{

  return xdp::hal::profiling_wrapper("xclCopyBO",
  [handle, dst_boHandle, src_boHandle, size, dst_offset, src_offset] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ?
      drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
  }) ;
}

int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclReClock2(region, targetFreqMHz) : -ENODEV;
}

int xclLockDevice(xclDeviceHandle handle)
{
  return xdp::hal::profiling_wrapper("xclLockDevice", [handle] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv)
        return -ENODEV;
    return drv->xclLockDevice() ? 0 : 1;
  });
}

int xclUnlockDevice(xclDeviceHandle handle)
{
  return xdp::hal::profiling_wrapper("xclUnlockDevice", [handle] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv)
        return -ENODEV;
    return drv->xclUnlockDevice() ? 0 : 1;
  }) ;
}

int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
    return xclInternalResetDevice(handle, kind);
}

int xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
    // NOTE: until xclResetDevice is made completely internal,
    // this wrapper is being used to limit the pragma use to this file
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->resetDevice(kind) : -ENODEV;
}

int xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  try {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->p2pEnable(enable, force) : -ENODEV;
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -ENODEV;
  }
}

int xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t total_size)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->cmaEnable(enable, total_size) : -ENODEV;
}

int xclBootFPGA(xclDeviceHandle handle)
{
    // Not doable from user side. Can be added to xbmgmt later.
    return -EOPNOTSUPP;
}

int xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExportBO(boHandle) : -ENODEV;
}

unsigned int xclImportBO(xclDeviceHandle handle, int fd, unsigned flags)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv) {
        std::cout << __func__ << ", " << std::this_thread::get_id() << ", handle & XOCL Device are bad" << std::endl;
    }
    return drv ? drv->xclImportBO(fd, flags) : -ENODEV;
}

int
xclCloseExportHandle(int fd)
{
  return close(fd) ? -errno : 0;
}

ssize_t xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset)
{
  return xdp::hal::profiling_wrapper("xclUnmgdPwrite",
  [handle, flags, buf, count, offset] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclUnmgdPwrite(flags, buf, count, offset) : -ENODEV;
  }) ;
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
  return xdp::hal::profiling_wrapper("xclUnmgdPread",
  [handle, flags, buf, count, offset] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclUnmgdPread(flags, buf, count, offset) : -ENODEV;
  }) ;
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
  return xdp::hal::profiling_wrapper("xclGetBOProperties",
  [handle, boHandle, properties] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetBOProperties(boHandle, properties) : -ENODEV;
  }) ;
}

int xclGetUsageInfo(xclDeviceHandle handle, xclDeviceUsage *info)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetUsageInfo(info) : -ENODEV;
}

int xclGetSectionInfo(xclDeviceHandle handle, void* section_info, size_t * section_size,
    enum axlf_section_kind kind, int index)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetSectionInfo(section_info, section_size, kind, index) : -ENODEV;
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
  return xdp::hal::profiling_wrapper("xclExecBuf",
  [handle, cmdBO] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExecBuf(cmdBO) : -ENODEV;
  }) ;
}

int xclExecBufWithWaitList(xclDeviceHandle handle, unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExecBuf(cmdBO,num_bo_in_wait_list,bo_wait_list) : -ENODEV;
}

int xclRegisterEventNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRegisterEventNotify(userInterrupt, fd) : -ENODEV;
}

int xclIPSetReadRange(xclDeviceHandle handle, uint32_t ipIndex, uint32_t start, uint32_t size)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclIPSetReadRange(ipIndex, start, size) : -ENODEV;
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  return xdp::hal::profiling_wrapper("xclExecWait",
  [handle, timeoutMilliSec] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExecWait(timeoutMilliSec) : -ENODEV;
  });
}

int
xclOpenContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  return xdp::hal::profiling_wrapper("xclOpenContext",
  [handle, xclbinId, ipIndex, shared] {
    try {
      xocl::shim *drv = xocl::shim::handleCheck(handle);
      return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -ENODEV;
    }
    catch (const xrt_core::error& ex) {
      xrt_core::send_exception_message(ex.what());
      return ex.get_code();
    }
  }) ;
}

int xclCloseContext(xclDeviceHandle handle, const uuid_t xclbinId, unsigned ipIndex)
{
  return xdp::hal::profiling_wrapper("xclCloseContext",
  [handle, xclbinId, ipIndex] {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -ENODEV;
  });
}

const axlf_section_header* wrap_get_axlf_section(const axlf* top, axlf_section_kind kind)
{
    return xclbin::get_axlf_section(top, kind);
}

size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}

uint xclGetNumLiveProcesses(xclDeviceHandle handle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetNumLiveProcesses() : 0;
}

int xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetDebugIPlayoutPath(layoutPath, size) : -ENODEV;
}

int xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclGetTraceBufferInfo(nSamples, traceSamples, traceBufSz) : -ENODEV;
}

int xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclReadTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample) : -ENODEV;
}

int xclCreateProfileResults(xclDeviceHandle handle, ProfileResults** results)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  int status = -1;
  CREATE_PROFILE_RESULTS_CB(handle, results, status);
  return status;
}

int xclGetProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  int status = -1;
  GET_PROFILE_RESULTS_CB(handle, results, status);
  return status;
}

int xclDestroyProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  int status = -1;
  DESTROY_PROFILE_RESULTS_CB(handle, results, status);
  return status;
}

double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetDeviceClockFreqMHz() : 0.0;
}

double xclGetHostReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetHostReadMaxBandwidthMBps() : 0.0;
}

double xclGetHostWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetHostWriteMaxBandwidthMBps() : 0.0;
}

double xclGetKernelReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetKernelReadMaxBandwidthMBps() : 0.0;
}

double xclGetKernelWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetKernelWriteMaxBandwidthMBps() : 0.0;
}

int xclGetSysfsPath(xclDeviceHandle handle, const char* subdev,
                      const char* entry, char* sysfsPath, size_t size)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetSysfsPath(subdev, entry, sysfsPath, size);
}

int xclIPName2Index(xclDeviceHandle handle, const char *name)
{
  try {
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return (drv) ? drv->xclIPName2Index(name) : -ENODEV;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return -ENOENT;
  }
}

int xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclUpdateSchedulerStat() : -ENODEV;
}

int xclOpenIPInterruptNotify(xclDeviceHandle handle, uint32_t ipIndex, unsigned int flags)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return (drv) ? drv->xclOpenIPInterruptNotify(ipIndex, flags) : -EINVAL;
}

int xclCloseIPInterruptNotify(xclDeviceHandle handle, int fd)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return (drv) ? drv->xclCloseIPInterruptNotify(fd) : -EINVAL;
}

int xclGetSubdevPath(xclDeviceHandle handle,  const char* subdev,
                        uint32_t idx, char* path, size_t size)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if (!drv)
    return -1;
  return drv->xclGetSubdevPath(subdev, idx, path, size);
}

void
xclGetDebugIpLayout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret)
{
  if(size_ret)
    *size_ret = 0;
  return;
}
