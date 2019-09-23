#include "xallocator.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include "core/edge/include/zynq_ioctl.h"
#include <map>
#include <mutex>
#include <iostream>

#define ZOCL_DRIVER_PATH "/dev/dri/renderD128"
 
std::mutex mBOMapLock;
std::map<uint64_t, drm_zocl_info_bo> mBoMap; 

static int FileHandle(void)
{
    static int FileFd = open(ZOCL_DRIVER_PATH, O_RDWR);
    return FileFd;
}

void* xallocate(size_t len) {
    std::lock_guard<std::mutex> l(mBOMapLock);
    uint32_t flag = DRM_ZOCL_BO_FLAGS_ALLOCATOR |DRM_ZOCL_BO_FLAGS_COHERENT | DRM_ZOCL_BO_FLAGS_CMA;
    drm_zocl_create_bo createBoInfo = { len, 0xffffffff, flag };
    int result = ioctl(FileHandle(), DRM_IOCTL_ZOCL_CREATE_BO, &createBoInfo);

#ifdef VERBOSE
    std::cout  << "xclAllocBO result = " << result << std::endl;
    std::cout << "Handle " << createBoInfo.handle << std::endl;
#endif

    if (result)
      return nullptr;

    drm_zocl_info_bo boInfo = { createBoInfo.handle, 0, 0 };
    result = ioctl(FileHandle(), DRM_IOCTL_ZOCL_INFO_BO, &boInfo);

    drm_zocl_map_bo mapInfo = { createBoInfo.handle, 0, 0 };
    result = ioctl(FileHandle(), DRM_IOCTL_ZOCL_MAP_BO, &mapInfo);
    if (result)
      return nullptr;

    void *ptr = mmap(0, boInfo.size, (PROT_READ|PROT_WRITE),
            MAP_SHARED, FileHandle(), mapInfo.offset);

    if (ptr == MAP_FAILED) {
      std::cout << "Failed to map buffer" << std::endl;
      return nullptr;
    }
    mBoMap.insert(std::pair<uint64_t, drm_zocl_info_bo>(reinterpret_cast<uint64_t>(ptr), boInfo));
    return ptr;
}

void xdeallocate(void *buf) {
    std::lock_guard<std::mutex> l(mBOMapLock);
    auto it = mBoMap.find(reinterpret_cast<uint64_t>(buf));
    if (it == mBoMap.end()) {
      std::cout << "xdeallocate failed to find buffer" << std::endl;
      return;
    }

    drm_gem_close closeInfo = {it->second.handle, 0};
    munmap(buf, it->second.size);
    ioctl(FileHandle(), DRM_IOCTL_GEM_CLOSE, &closeInfo);
}
