// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDEV_LINUX_H_
#define _XCL_PCIDEV_LINUX_H_

#include "pcidrv.h"
#include "device_linux.h"
#include "core/common/device_factory.h"
#include <string>

#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

// Supported vendors
#define XILINX_ID       0x10ee
#define ADVANTECH_ID    0x13fe
#define AWS_ID          0x1d0f
#define ARISTA_ID       0x3475
#define INVALID_ID      0xffff

#define FDT_BEGIN_NODE  0x1
#define FDT_END_NODE    0x2
#define FDT_PROP        0x3
#define FDT_NOP         0x4
#define FDT_END         0x9
#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

enum p2p_config {
  P2P_CONFIG_DISABLED,
  P2P_CONFIG_ENABLED,
  P2P_CONFIG_REBOOT,
  P2P_CONFIG_NOT_SUPP,
  P2P_CONFIG_ERROR,
};

struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

namespace xrt_core { namespace pci {
  class pcidev_linux : public device_factory
  {
  public:
    // Fundamental and static information for this device are defined as class
    // members and initialized during object construction.
    //
    // The rest of information related to the device shall be obtained
    // dynamically via sysfs APIs below.

    uint32_t m_instance = INVALID_ID;
    std::string m_sysfs_name;     // dir name under /sys/bus/pci/devices
    int m_user_bar = 0;  // BAR mapped in by tools, default is BAR0
    size_t m_user_bar_size = 0;
    uint16_t m_domain = INVALID_ID;
    uint16_t m_bus = INVALID_ID;
    uint16_t m_dev = INVALID_ID;
    uint16_t m_func = INVALID_ID;

    pcidev_linux(const std::string& sysfs, bool isuser);
    virtual
      ~pcidev_linux();
    pcidev_linux() = delete;

    virtual void
    sysfs_get(const std::string& subdev, const std::string& entry,
      std::string& err, std::vector<std::string>& sv);
    virtual void
    sysfs_get(const std::string& subdev, const std::string& entry,
      std::string& err, std::vector<uint64_t>& iv);
    virtual void
    sysfs_get(const std::string& subdev, const std::string& entry,
      std::string& err, std::string& s);
    virtual void
    sysfs_get(const std::string& subdev, const std::string& entry,
      std::string& err, std::vector<char>& buf);
    template <typename T>
    void
    sysfs_get(const std::string& subdev, const std::string& entry,
      std::string& err, T& i, const T& default_val)
    {
      std::vector<uint64_t> iv;
      sysfs_get(subdev, entry, err, iv);
      if (!iv.empty())
        i = static_cast<T>(iv[0]);
      else
        i = static_cast<T>(default_val); // default value
    }

    virtual void
    sysfs_get_sensor(const std::string& subdev, const std::string& entry, uint32_t& i)
    {
      std::string err;
      sysfs_get<uint32_t>(subdev, entry, err, i, 0);
    }

    virtual void
    sysfs_put(const std::string& subdev, const std::string& entry,
      std::string& err, const std::string& input);
    virtual void
    sysfs_put(const std::string& subdev, const std::string& entry,
      std::string& err, const std::vector<char>& buf);

    virtual void
    sysfs_put(const std::string& subdev, const std::string& entry,
      std::string& err, const unsigned int& buf);

    virtual std::string
    get_sysfs_path(const std::string& subdev, const std::string& entry);

    virtual std::string
    get_subdev_path(const std::string& subdev, uint32_t idx) const;

    virtual int
    pcieBarRead(uint64_t offset, void* buf, uint64_t len);

    virtual int
    pcieBarWrite(uint64_t offset, const void* buf, uint64_t len);

    virtual int
    open(const std::string& subdev, int flag) const;

    virtual int
    open(const std::string& subdev, uint32_t idx, int flag) const;

    virtual void
    close(int devhdl) const;

    virtual int
    ioctl(int devhdl, unsigned long cmd, void* arg = nullptr) const;

    virtual int
    poll(int devhdl, short events, int timeout_ms);

    virtual void
    * mmap(int devhdl, size_t len, int prot, int flags, off_t offset);

    virtual int
    munmap(int devhdl, void* addr, size_t len);

    virtual int
    flock(int devhdl, int op);

    virtual int
    get_partinfo(std::vector<std::string>& info, void* blob = nullptr);

    virtual std::shared_ptr<pcidev_linux>
    lookup_peer_dev();

    // Hand out a "device" instance that is specific to this type of device.
    // Caller will use this device to access device specific implementation of ishim.
    virtual std::shared_ptr<device>
    create_device(device::handle_type handle, device::id_type id) const override;

    // Hand out an opaque "shim" handle that is specific to this type of device.
    // On legacy Alveo device, this handle can be used to lookup a device instance and
    // make xcl HAL API calls.
    // On new platforms, this handle can only be used to look up a device. HAL API calls
    // through it are not supported any more.
    virtual device::handle_type
    create_shim(device::id_type id) const override;

    virtual std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
    get_bdf_info() const override
    {
      return std::make_tuple(m_domain, m_bus, m_dev, m_func);
    }

  private:
    int
    map_usr_bar();

    std::mutex m_lock;
    char* m_user_bar_map = reinterpret_cast<char*>(MAP_FAILED);

  };
  
  int
  shutdown(std::shared_ptr<xrt_core::pci::pcidev_linux> mgmt_dev, bool remove_user = false, bool remove_mgmt = false);

  int
  check_p2p_config(const std::shared_ptr<xrt_core::pci::pcidev_linux>& dev, std::string& err);

}} // namespace xrt_core :: pci

#endif
