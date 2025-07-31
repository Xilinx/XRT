// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#include "drv_zocl.h"
#include "dev_zocl.h"
#include "dev.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <drm/drm.h>
#include <iostream>
#include "core/common/message.h"

constexpr size_t version_desc_len{512};
constexpr size_t version_date_len{128};
constexpr size_t version_name_len{128};

namespace xrt_core::edge {

namespace fs = std::filesystem;

void
drv_zocl::
scan_devices(std::vector<std::shared_ptr<dev>>& dev_list)
{
   auto match = [](const std::string& dir_path, const std::regex& filter) {
     if (!fs::exists(dir_path)) {
       throw std::runtime_error("Device search path: " + dir_path + " doesn't exist\n");
     }

     fs::directory_iterator end_itr;
     for (fs::directory_iterator itr{dir_path}; itr != end_itr; ++itr) {
       if (std::regex_match(itr->path().filename().string(), filter)) {
         return fs::read_symlink(itr->path()).filename().string();
       }
     }

     throw std::runtime_error("Device node symlink cannot be found\n");
   };

   static const std::regex zocl_filter{"platform.*zyxclmm_drm-render"};
   const std::string ver_name = "zocl";

   try {
     const std::string dev_path{"/dev/dri/"};
     const std::string render_dev_sym_dir{"/dev/dri/by-path/"};

     auto drm_dev_name = dev_path + match(render_dev_sym_dir, zocl_filter);
     if (!fs::exists(drm_dev_name))
       throw std::runtime_error(drm_dev_name + " device node doesn't exist");

     auto file_d = open(drm_dev_name.c_str(), O_RDWR); // NOLINT
     // lambda for closing fd
     auto fd_close = [](int* fd){
    	if (fd && *fd >= 0) {
    		close(*fd);
        }
     };
     auto fd = std::unique_ptr<int, decltype(fd_close)>(&file_d, fd_close);
     if (*fd < 0)
       throw std::runtime_error("Failed to open device file " + drm_dev_name);

     // validate DRM version name
     std::vector<char> name(version_name_len,0);
     std::vector<char> desc(version_desc_len,0);
     std::vector<char> date(version_date_len,0);
     drm_version version{};
     std::memset(&version, 0, sizeof(version));
     version.name = name.data();
     version.name_len = version_name_len;
     version.desc = desc.data();
     version.desc_len = version_desc_len;
     version.date = date.data();
     version.date_len = version_date_len;

     if (ioctl(*fd, DRM_IOCTL_VERSION, &version) != 0) // NOLINT
       throw std::runtime_error("Failed to get DRM version for device file " + drm_dev_name);

     if (std::strncmp(version.name, ver_name.c_str(), ver_name.length()) != 0)
       throw std::runtime_error("Driver DRM version check failed for device file " + drm_dev_name);
     dev_list.push_back(create_edev());
   }
   catch (const std::exception& e) {
     xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", e.what());
   }
}

std::shared_ptr<dev>
drv_zocl::
create_edev(const std::string& sysfs) const
{
   return std::make_shared<dev_zocl>("/sys/class/drm/" + get_render_devname() + "/device/");
}

} //namespace xrt_core::edge
