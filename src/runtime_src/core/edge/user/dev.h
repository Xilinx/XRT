// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#ifndef DEV_H_
#define DEV_H_

#include "core/common/device.h"
#include <memory>
#include <string>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <filesystem>
#include <regex>

namespace xrt_core::edge {

class dev //Base class for edge type of devices
{
public:
   dev(const std::string& sysfs_base = "") : sysfs_root(sysfs_base)	{}
   void sysfs_get(const std::string& entry, std::string& err_msg,
        				std::vector<std::string>& sv);
   void sysfs_get(const std::string& entry, std::string& err_msg,
        				std::vector<uint64_t>& iv);
   void sysfs_get(const std::string& entry, std::string& err_msg,
        				std::string& s);
   void sysfs_get(const std::string& entry, std::string& err_msg,
        				std::vector<char>& buf);
   template <typename T>
   void sysfs_get(const std::string& entry, std::string& err_msg, T& i, T def) {
     std::vector<uint64_t> iv;
     sysfs_get(entry, err_msg, iv);
     if (!iv.empty())
       i = static_cast<T>(iv[0]);
     else
       i = def; // user defined default value
   }

   void sysfs_put(const std::string& entry, std::string& err_msg,
        				const std::string& input);
   void sysfs_put(const std::string& entry, std::string& err_msg,
        				const std::vector<char>& buf);
   std::string get_sysfs_path(const std::string& entry);

   virtual std::shared_ptr<device>
   create_device(device::handle_type handle, device::id_type id) const = 0;

   virtual device::handle_type
   create_shim(device::id_type id) const = 0;

   virtual ~dev() {}
private:
   std::fstream sysfs_open(const std::string& entry, std::string& err,
     					bool write = false, bool binary = false);
   std::string sysfs_root;
};

} //namespace xrt_core::edge

std::string get_render_devname();

#endif
