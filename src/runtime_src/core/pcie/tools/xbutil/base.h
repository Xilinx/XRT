/**
 * Copyright (C) 2019 Xilinx, Inc
 * Author: Sonal Santan
 * xbutil/xbmgmt helpers to dump system information
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

#ifndef XBUTIL_BASE_H
#define XBUTIL_BASE_H

#include "core/common/time.h"

#include <sys/utsname.h>
#include <gnu/libc-version.h>
#include <unistd.h>

#include <boost/property_tree/ini_parser.hpp>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>


namespace xcldev {

static std::string getMachineModel()
{
#if defined(__aarch64__) || defined(__arm__) || defined(__mips__)
    const char node[] = "/proc/device-tree/model";
#elif defined(__PPC64__)
    const char node[] = "/proc/device-tree/model-name";
    // /proc/device-tree/system-id may be 000000
    // /proc/device-tree/model may be 00000
#elif defined (__x86_64__)
    const char node[] = "/sys/devices/virtual/dmi/id/product_name";
#else
    #error "Unsupported platform"
    const char node[] = "";
#endif
    std::string model("unknown");
    std::ifstream stream(node);
    std::getline(stream, model);
    stream.close();
    return model;
}

static std::string driver_version(std::string driver)
{
    std::string line("unknown");
    std::string path("/sys/bus/pci/drivers/");
    path += driver;
    path += "/module/version";
    std::ifstream ver(path);
    if (ver.is_open())
        getline(ver, line);
    return line;
}

void xrtInfo(boost::property_tree::ptree &pt)
{
    pt.put("build.version",   xrt_build_version);
    pt.put("build.hash",      xrt_build_version_hash);
    pt.put("build.date",      xrt_build_version_date);
    pt.put("build.branch",    xrt_build_version_branch);
    pt.put("build.xocl",      driver_version("xocl"));
    pt.put("build.xclmgmt",   driver_version("xclmgmt"));
}

void osInfo(boost::property_tree::ptree &pt)
{
    struct utsname sysinfo;
    if (!uname(&sysinfo)) {
        pt.put("sysname",   sysinfo.sysname);
        pt.put("release",   sysinfo.release);
        pt.put("version",   sysinfo.version);
        pt.put("machine",   sysinfo.machine);
    }
    pt.put("glibc", gnu_get_libc_version());

    // The file is a requirement as per latest Linux standards
    // https://www.freedesktop.org/software/systemd/man/os-release.html
    std::ifstream ifs("/etc/os-release");
    if (ifs.good()) {
        boost::property_tree::ptree opt;
        boost::property_tree::ini_parser::read_ini(ifs, opt);
        std::string val = opt.get<std::string>("PRETTY_NAME", "");
        if (val.length()) {
            if ((val.front() == '"') && (val.back() == '"')) {
                val.erase(0, 1);
                val.erase(val.size()-1);
            }
            pt.put("linux", val);
        }
        ifs.close();
    }

    pt.put("cores", std::thread::hardware_concurrency());
    pt.put("memory", sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE) / 0x100000);
    pt.put("model", getMachineModel());
    pt.put("now", xrt_core::timestamp());
}

void baseInit()
{
    try {
        boost::property_tree::ptree os_pt;
        boost::property_tree::ptree xrt_pt;
        osInfo(os_pt);
        xrtInfo(xrt_pt);
        sensor_tree::put("version", "1.1.0"); // json schema version
        sensor_tree::add_child("system", os_pt);
        sensor_tree::add_child("runtime", xrt_pt);
    } catch (const boost::property_tree::ptree_error &e) {
        std::cout << e.what() << std::endl;
    }
}

void baseDump(std::ostream &ostr)
{
    std::ios::fmtflags f( ostr.flags() );
    ostr << std::left;
    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "System Configuration"
         << "\nOS name:\t"      << sensor_tree::get<std::string>("system.sysname", "N/A")
         << "\nRelease:\t"      << sensor_tree::get<std::string>("system.release", "N/A")
         << "\nVersion:\t"      << sensor_tree::get<std::string>("system.version", "N/A")
         << "\nMachine:\t"      << sensor_tree::get<std::string>("system.machine", "N/A")
         << "\nModel:\t\t"      << sensor_tree::get<std::string>("system.model", "N/A")
         << "\nCPU cores:\t"    << sensor_tree::get<std::string>("system.cores", "N/A")
         << "\nMemory:\t\t"     << sensor_tree::get<std::string>("system.memory", "N/A") << " MB"
         << "\nGlibc:\t\t"      << sensor_tree::get<std::string>("system.glibc", "N/A")
         << "\nDistribution:\t" << sensor_tree::get<std::string>("system.linux", "N/A")
         << "\nNow:\t\t"        << sensor_tree::get<std::string>("system.now", "N/A")
	 << std::endl;

    ostr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    ostr << "XRT Information"
	 << "\nVersion:\t"      << sensor_tree::get<std::string>( "runtime.build.version", "N/A" )
         << "\nGit Hash:\t"     << sensor_tree::get<std::string>( "runtime.build.hash", "N/A" )
         << "\nGit Branch:\t"   << sensor_tree::get<std::string>( "runtime.build.branch", "N/A" )
         << "\nBuild Date:\t"   << sensor_tree::get<std::string>( "runtime.build.date", "N/A" )
         << "\nXOCL:\t\t"       << sensor_tree::get<std::string>( "runtime.build.xocl", "N/A" )
         << "\nXCLMGMT:\t"      << sensor_tree::get<std::string>( "runtime.build.xclmgmt", "N/A" )
         << std::endl;
    ostr.flags(f);
}

} // xcldev

#endif
