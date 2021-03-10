/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "app/xma_utils.hpp"
#include "lib/xma_utils.hpp"
#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "app/xmaparam.h"
#include "lib/xmaapi.h"
#include "lib/xmalimits_lib.h"
#include "ert.h"
#include "core/common/config_reader.h"
#include "core/pcie/linux/scan.h"
#include "core/common/utils.h"
#include <dlfcn.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <iostream>
#include <sstream>
#include <bitset>

#define XMAUTILS_MOD "xmautils"

extern XmaSingleton *g_xma_singleton;

namespace xma_core {
    static const std::map<XmaSessionType, const std::string> sessionMap = {
        { XmaSessionType::XMA_SCALER, "scaler"},
        { XmaSessionType::XMA_ENCODER, "encoder"},
        { XmaSessionType::XMA_DECODER, "decoder"},
        { XmaSessionType::XMA_FILTER, "filter"},
        { XmaSessionType::XMA_KERNEL, "kernel"},
        { XmaSessionType::XMA_ADMIN, "admin"},
        { XmaSessionType::XMA_INVALID, "invalid"}
    };

    static const std::map<ert_cmd_state, const std::string> cu_cmdMap = {
        { ert_cmd_state::ERT_CMD_STATE_NEW, "NEW"},
        { ert_cmd_state::ERT_CMD_STATE_QUEUED, "QUEUED"},
        { ert_cmd_state::ERT_CMD_STATE_RUNNING, "RUNNING"},
        { ert_cmd_state::ERT_CMD_STATE_COMPLETED, "COMPLETED"},
        { ert_cmd_state::ERT_CMD_STATE_ERROR, "ERROR"},
        { ert_cmd_state::ERT_CMD_STATE_ABORT, "ABORT"},
        { ert_cmd_state::ERT_CMD_STATE_SUBMITTED, "SUBMITTED"},
        { ert_cmd_state::ERT_CMD_STATE_TIMEOUT, "TIMEOUT"},
        { ert_cmd_state::ERT_CMD_STATE_NORESPONSE, "NORESPONSE"},
        { ert_cmd_state::ERT_CMD_STATE_SKERROR, "SKERROR"},
        { ert_cmd_state::ERT_CMD_STATE_SKCRASHED, "SKCRASHED"},
        { ert_cmd_state::ERT_CMD_STATE_MAX, "MAX"}
    };

    std::string get_cu_cmd_state(ert_start_kernel_cmd *cu_cmd) {
        auto it = cu_cmdMap.find((ert_cmd_state)cu_cmd->state);
        if (it == cu_cmdMap.end()) {
            return std::string("invalid");
        }
        return it->second;
    }

    std::string get_session_name(XmaSessionType eSessionType) {
        auto it = sessionMap.find(eSessionType);
        if (it == sessionMap.end()) {
            return std::string("invalid");
        }
        return it->second;
    }

    int32_t finalize_ddr_index(XmaHwKernel* kernel_info, int32_t req_ddr_index, int32_t& ddr_index, const std::string& prefix) {
        ddr_index = INVALID_M1;
        if (kernel_info->soft_kernel) {
            if (req_ddr_index != 0) {
                xma_logmsg(XMA_WARNING_LOG, prefix.c_str(), "XMA session with soft_kernel only allows ddr bank of zero");
            }
            //Only allow ddr_bank == 0;
            ddr_index = 0;
            xma_logmsg(XMA_DEBUG_LOG, prefix.c_str(), "XMA session with soft_kernel default ddr_bank: %d", ddr_index);
            return XMA_SUCCESS;
        }
        if (req_ddr_index < 0) {
            ddr_index = kernel_info->default_ddr_bank;
            xma_logmsg(XMA_DEBUG_LOG, prefix.c_str(), "XMA session default ddr_bank: %d", ddr_index);
            return XMA_SUCCESS;
        }
        std::bitset<MAX_DDR_MAP> tmp_bset;
        tmp_bset = kernel_info->ip_ddr_mapping;
        if (tmp_bset[req_ddr_index]) {
            ddr_index = req_ddr_index;
            xma_logmsg(XMA_DEBUG_LOG, prefix.c_str(), "Using user supplied default ddr_bank. XMA session default ddr_bank: %d", ddr_index);
            return XMA_SUCCESS;
        }
        xma_logmsg(XMA_ERROR_LOG, prefix.c_str(),
            "User supplied default ddr_bank is invalid. Valid ddr_bank mapping for this CU: %s", tmp_bset.to_string().c_str());
        return XMA_ERROR;
    }

    int32_t create_session_execbo(XmaHwSessionPrivate *priv, int32_t count, const std::string& prefix) {
        for (int32_t d = 0; d < count; d++) {
            xclBufferHandle  bo_handle = 0;
            int       execBO_size = MAX_EXECBO_BUFF_SIZE;
            //uint32_t  execBO_flags = (1<<31);
            char     *bo_data;
            bo_handle = xclAllocBO(priv->dev_handle, 
                                    execBO_size, 
                                    0, 
                                    XCL_BO_FLAGS_EXECBUF);
            if (!bo_handle || bo_handle == NULLBO) 
            {
                xma_logmsg(XMA_ERROR_LOG, prefix.c_str(), "Initalization of plugin failed. Failed to alloc execbo");
                return XMA_ERROR;
            }
            bo_data = (char*)xclMapBO(priv->dev_handle, bo_handle, true);
            memset((void*)bo_data, 0x0, execBO_size);

            priv->kernel_execbos.emplace_back(XmaHwExecBO{});
            XmaHwExecBO& dev_execbo = priv->kernel_execbos.back();
            dev_execbo.handle = bo_handle;
            dev_execbo.data = bo_data;
        }
        return XMA_SUCCESS;
    }

    int32_t check_plugin_version(int32_t plugin_main_ver, int32_t plugin_sub_ver) {
        if ((plugin_main_ver == XMA_LIB_MAIN_VER && plugin_sub_ver < XMA_LIB_SUB_VER) || plugin_main_ver < XMA_LIB_MAIN_VER) {
            return -1;
        }
        if ((plugin_main_ver == XMA_LIB_MAIN_VER && plugin_sub_ver > XMA_LIB_SUB_VER) || plugin_main_ver > XMA_LIB_MAIN_VER) {
            return -2;
        }
        return XMA_SUCCESS;
    }
}

namespace xma_core { namespace utils {

constexpr std::uint64_t cu_base_min = 0x1800000;
namespace bfs = boost::filesystem;


static const char*
emptyOrValue(const char* cstr)
{
  return cstr ? cstr : "";
}

int32_t
directoryOrError(const bfs::path& path)
{
  if (!bfs::is_directory(path))
      return XMA_ERROR;

   return XMA_SUCCESS;
}

static boost::filesystem::path&
dllExt()
{
  static boost::filesystem::path sDllExt(".so");
  return sDllExt;
}

inline bool
isDLL(const bfs::path& path)
{
  return (bfs::exists(path)
          && bfs::is_regular_file(path)
          && path.extension()==dllExt());
}

static bool
isEmulationMode()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
}

int32_t
load_libxrt()
{
    dlerror();    /* Clear any existing error */

    // xrt
    bfs::path xrt(emptyOrValue(std::getenv("XILINX_XRT")));
    if (xrt.empty()) {
        std::cout << "XMA INFO: XILINX_XRT env variable not set. Trying default /opt/xilinx/xrt" << std::endl;
        xrt = bfs::path("/opt/xilinx/xrt");
    }
    if (directoryOrError(xrt) != XMA_SUCCESS) {
        std::cout << "XMA FATAL: XILINX_XRT env variable is not a directory: " << xrt.string() << std::endl;
        return XMA_ERROR;
    }

    // Load the xmaplugin library as it is a dependency for all plugins
    bfs::path xma2plugin_lib(xrt / "lib/libxma2plugin.so");
    if (!isDLL(xma2plugin_lib)) {
        std::cout << "XMA FATAL: xma2plugin lib not found. Lib: " << xma2plugin_lib.string() << std::endl;
        return XMA_ERROR;
    }
    void* xmahandle = dlopen(xma2plugin_lib.string().c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!xmahandle)
    {
        std::cout << "XMA FATAL: Failed to load xma2plugin library: " << xma2plugin_lib.string() << std::endl;
        std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
        return XMA_ERROR;
    }

    if (!isEmulationMode()) {
        bfs::path p1(xrt / "lib/libxrt_core.so");
        if (isDLL(p1)) {
            void* xrthandle = dlopen(p1.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!xrthandle)
            {
                std::cout << "XMA FATAL: Failed to load XRT library: " << p1.string() << std::endl;
                std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
                return XMA_ERROR;
            }

            return 1;
        }

        bfs::path p2(xrt / "lib/libxrt_aws.so");
        if (isDLL(p2)) {
            void* xrthandle = dlopen(p2.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!xrthandle)
            {
                std::cout << "XMA FATAL: Failed to load XRT library: " << p2.string() << std::endl;
                std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
                return XMA_ERROR;
            }

            return 2;
        }

        std::cout << "XMA FATAL: Failed to load XRT library" << std::endl;
        return XMA_ERROR;
    }
   
    if (is_sw_emulation()) {
        auto sw_em_driver = xrt_core::config::get_sw_em_driver();
        if (sw_em_driver != "null") {
            if (isDLL(sw_em_driver)) {
                void* xrthandle = dlopen(sw_em_driver.c_str(), RTLD_NOW | RTLD_GLOBAL);
                if (!xrthandle)
                {
                    std::cout << "XMA FATAL: Failed to load XRT SWEM library: " << sw_em_driver << std::endl;
                    std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
                    return XMA_ERROR;
                }

                return 4;
            }
            std::cout << "XMA FATAL: Failed to load XRT SWEM library: " << sw_em_driver << std::endl;
            return XMA_ERROR;
        }

        bfs::path p2(xrt / "lib/libxrt_swemu.so");
        sw_em_driver = p2.string();

        if (isDLL(sw_em_driver)) {
            void* xrthandle = dlopen(sw_em_driver.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!xrthandle)
            {
                std::cout << "XMA FATAL: Failed to load XRT SWEM library: " << sw_em_driver << std::endl;
                std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
                return XMA_ERROR;
            }

            return 6;
        }
        std::cout << "XMA FATAL: Failed to load XRT SWEM library: libxrt_swemu.so" << std::endl;
        return XMA_ERROR;
    }

    auto hw_em_driver = xrt_core::config::get_hw_em_driver();
    if (hw_em_driver != "null") {
        if (isDLL(hw_em_driver)) {
            void* xrthandle = dlopen(hw_em_driver.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!xrthandle)
            {
                std::cout << "XMA FATAL: Failed to load XRT HWEM library: " << hw_em_driver << std::endl;
                std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
                return XMA_ERROR;
            }

            return 3;
        }
        std::cout << "XMA FATAL: Failed to load XRT HWEM library: " << hw_em_driver << std::endl;
        return XMA_ERROR;
    }

    bfs::path p1(xrt / "lib/libxrt_hwemu.so");
    hw_em_driver = p1.string();

    if (isDLL(hw_em_driver)) {
        void* xrthandle = dlopen(hw_em_driver.c_str(), RTLD_NOW | RTLD_GLOBAL);
        if (!xrthandle)
        {
            std::cout << "XMA FATAL: Failed to load XRT HWEM library: " << hw_em_driver << std::endl;
            std::cout << "XMA FATAL: DLL open error: " << std::string(dlerror()) << std::endl;
            return XMA_ERROR;
        }

        return 5;
    }

    std::cout << "XMA FATAL: Failed to load XRT HWEM library: libxrt_hwemu.so" << std::endl;
    return XMA_ERROR;
}

void get_system_info() {
    static auto verbosity = xrt_core::config::get_verbosity();
    XmaLogLevelType level = (XmaLogLevelType) std::min({(uint32_t)XMA_INFO_LOG, (uint32_t)verbosity});
    xma_logmsg(level, "XMA-System-Info", "======= START =============");
    for (unsigned j = 0; j < pcidev::get_dev_total(); j++) {
        auto dev = pcidev::get_dev(j);
        xma_logmsg(level, "XMA-System-Info", "dev index = %d; %s", j, dev->sysfs_name.c_str());
        if (dev->is_ready) {
            /* Let's keep this function as generic
            for more detials customers should use xbutil
            uint32_t hwcfg_dev_index = 0;
            bool xma_dev_found = false;
            for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
                if (hw_device.dev_index == (uint32_t)j) {
                    xma_dev_found = true;
                    break;
                }
                hwcfg_dev_index++;
            }
            if (xma_dev_found) {
            }
            */
            std::string errmsg, str1;
            str1 = std::string("None");
            dev->sysfs_get( "rom", "VBNV", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "DSA: %s", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("rom", "ddr_bank_count_max", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "DDR banks: %s", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("rom", "ddr_bank_size", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "DDR bank size: %s GB", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("", "link_speed", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "PCIe Speed: GEN %s", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("", "link_width", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "PCIe Width: x%s", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("", "xclbinuuid", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "xclbin uuid: %s", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("firewall", "detected_status", errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "Firewall Status: %s", str1.c_str());
            str1 = std::string("None");
            dev->sysfs_get("firewall", "detected_level",  errmsg, str1);
            xma_logmsg(level, "XMA-System-Info", "Firewall Value: %s", str1.c_str());
            /* For more details use xbutil
            std::vector<std::string> custat;
            dev->sysfs_get("mb_scheduler", "kds_custat", errmsg, custat);
            char delim = ':';
            for (auto cu_str: custat) {
                std::stringstream str2(cu_str);
                std::string addr = cu_str.substr(4, cu_str.find("]"));
                if (std::stoull(addr, 0, 16) >= cu_base_min) {
                    std::string item;
                    std::getline(str2, item, delim);
                    std::getline(str2, item, delim);
                    cu_str += " : ";
                    cu_str += parseCUStatus(std::stoi(item));
                    xma_logmsg(level, "XMA-System-Info", cu_str.c_str());
                }
            }
            */
        } else {
            xma_logmsg(level, "XMA-System-Info", " Device is not ready. May need to load/flash DSA");
        }
        xma_logmsg(level, "XMA-System-Info", " ");//Gap for next device
    }
    xma_logmsg(level, "XMA-System-Info", "======= END =============");

    bool expected = false;
    bool desired = true;
    while (!g_xma_singleton->log_msg_list_locked.compare_exchange_weak(expected, desired)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        expected = false;
    }
    //log msg list lock acquired

    while (!g_xma_singleton->log_msg_list.empty()) {
        auto itr1 = g_xma_singleton->log_msg_list.begin();
        xclLogMsg(NULL, (xrtLogMsgLevel)itr1->level, "XMA", itr1->msg.c_str());
        g_xma_singleton->log_msg_list.pop_front();
    }

    //Release log msg list lock
    g_xma_singleton->log_msg_list_locked = false;
}

void get_session_cmd_load() {
    static auto verbosity = xrt_core::config::get_verbosity();
    XmaLogLevelType level = (XmaLogLevelType) std::min({(uint32_t)XMA_INFO_LOG, (uint32_t)verbosity});
    //static const std::string logger =  xrt_core::config::get_logging();
    //std::cout << "XMA-Log-Info: To: " << logger << std::endl;
    xma_logmsg(level, "XMA-Session-Stats", "=== Session CU Command Relative Loads: ===");
    for (auto& itr1: g_xma_singleton->all_sessions_vec) {
        xma_logmsg(level, "XMA-Session-Stats", "--------");
        XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) itr1.hw_session.private_do_not_use;
        float avg_cmds = 0;
        if (priv1->num_cu_cmds_avg != 0) {
            avg_cmds = priv1->num_cu_cmds_avg / STATS_WINDOW;
        } else if (priv1->num_samples > 0) {
            avg_cmds = priv1->num_cu_cmds_avg_tmp / ((float)priv1->num_samples);
        }
        xma_logmsg(level, "XMA-Session-Stats", "Session id: %d, type: %s, avg cu cmds: %.2f, busy vs idle: %d vs %d", itr1.session_id, 
            xma_core::get_session_name(itr1.session_type).c_str(), avg_cmds, (uint32_t)priv1->cmd_busy, (uint32_t)priv1->cmd_idle);

        xma_logmsg(level, "XMA-Session-Stats", "Session id: %d, max busy vs idle ticks: %d vs %d, relative cu load: %d", itr1.session_id, (uint32_t)priv1->cmd_busy_ticks, (uint32_t)priv1->cmd_idle_ticks, (uint32_t)priv1->kernel_complete_total);
        XmaHwKernel* kernel_info = priv1->kernel_info;
        if (kernel_info == NULL) {
            continue;
        }
        if (!kernel_info->is_shared) {
            continue;
        }
        if (kernel_info->num_cu_cmds_avg != 0) {
            avg_cmds = kernel_info->num_cu_cmds_avg / STATS_WINDOW;
        } else if (kernel_info->num_samples > 0) {
            avg_cmds = kernel_info->num_cu_cmds_avg_tmp / ((float)kernel_info->num_samples);
        }
        xma_logmsg(level, "XMA-Session-Stats", "Session id: %d, cu: %s, avg cmds: %.2f, busy vs idle: %d vs %d", itr1.session_id, kernel_info->name, avg_cmds, (uint32_t)kernel_info->cu_busy, (uint32_t)kernel_info->cu_idle);
    }
    xma_logmsg(level, "XMA-Session-Stats", "--------");
    xma_logmsg(level, "XMA-Session-Stats", "Num of Decoders: %d", (uint32_t)g_xma_singleton->num_decoders);
    xma_logmsg(level, "XMA-Session-Stats", "Num of Scalers: %d", (uint32_t)g_xma_singleton->num_scalers);
    xma_logmsg(level, "XMA-Session-Stats", "Num of Encoders: %d", (uint32_t)g_xma_singleton->num_encoders);
    xma_logmsg(level, "XMA-Session-Stats", "Num of Filters: %d", (uint32_t)g_xma_singleton->num_filters);
    xma_logmsg(level, "XMA-Session-Stats", "Num of Kernels: %d", (uint32_t)g_xma_singleton->num_kernels);
    xma_logmsg(level, "XMA-Session-Stats", "Num of Admins: %d", (uint32_t)g_xma_singleton->num_admins);
    xma_logmsg(level, "XMA-Session-Stats", "--------\n");
}

int32_t get_cu_index(int32_t dev_index, char* cu_name1) {
    //singleton should be locked before calling this function
    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                   "dev_index not found\n");
        return -1;
    }

    uint32_t hwcfg_dev_index = 0;
    bool found = false;
    for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
        if (hw_device.dev_index == (uint32_t)dev_index) {
            found = true;
            break;
        }
        hwcfg_dev_index++;
    }
    if (!found) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                   "dev_index %d not loaded with xclbin", dev_index);
        return -1;
    }
    if (cu_name1 == nullptr) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                   "cu_name is null");
        return -1;
    }
    std::string cu_name = std::string(cu_name1);
    for (XmaHwKernel& kernel: g_xma_singleton->hwcfg.devices[hwcfg_dev_index].kernels) {
        if (std::string((char*)kernel.name) == cu_name) {
            return kernel.cu_index;
        }
    }
    xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                "cu_name %s not found", cu_name.c_str());
    return -1;
}

int32_t get_default_ddr_index(int32_t dev_index, int32_t cu_index) {
    //singleton should be locked before calling this function
    XmaHwCfg *hwcfg = &g_xma_singleton->hwcfg;
    if (dev_index >= hwcfg->num_devices || dev_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                   "dev_index not found");
        return -1;
    }

    uint32_t hwcfg_dev_index = 0;
    bool found = false;
    for (XmaHwDevice& hw_device: g_xma_singleton->hwcfg.devices) {
        if (hw_device.dev_index == (uint32_t)dev_index) {
            found = true;
            break;
        }
        hwcfg_dev_index++;
    }
    if (!found) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                   "dev_index %d not loaded with xclbin", dev_index);
        return -1;
    }
    if ((cu_index > 0 && (uint32_t)cu_index >= hwcfg->devices[hwcfg_dev_index].number_of_cus) || cu_index < 0) {
        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD,
                   "Invalid cu_index = %d", cu_index);
        return -1;
    }

    XmaHwKernel* kernel_info = &hwcfg->devices[hwcfg_dev_index].kernels[cu_index];

    if (hwcfg->devices[hwcfg_dev_index].kernels[cu_index].soft_kernel) {
        //Only allow ddr_bank == 0;
        return 0;
    } else {
        return kernel_info->default_ddr_bank;
    }
}

int32_t check_all_execbo(XmaSession s_handle) {
    //NOTE: execbo lock must be already obtained
    //Check only for commands in-progress in this sessions else too much checking will waste CPU cycles

    XmaHwSessionPrivate *priv1 = (XmaHwSessionPrivate*) s_handle.hw_session.private_do_not_use;

    if (g_xma_singleton->cpu_mode == XMA_CPU_MODE2) {
        bool notify_execbo_is_free = false;
        if (priv1->num_cu_cmds != 0) {
            int32_t i;
            int32_t num_execbo = priv1->num_execbo_allocated;
            for (i = 0; i < num_execbo; i++) {
                auto& ebo = priv1->kernel_execbos[i];
                if (ebo.in_use) {
                    ert_start_kernel_cmd *cu_cmd = 
                        (ert_start_kernel_cmd*)ebo.data;
                    if (cu_cmd->state == ERT_CMD_STATE_COMPLETED) {
                        if (s_handle.session_type < XMA_ADMIN) {
                            priv1->kernel_complete_count++;
                            priv1->kernel_complete_total++;
                        }
                        notify_execbo_is_free = true;
                        ebo.in_use = false;
                        cu_cmd->state = ERT_CMD_STATE_MAX;
                        priv1->CU_cmds.erase(ebo.cu_cmd_id1);
                        priv1->num_cu_cmds--;
                    } else if (cu_cmd->state == ERT_CMD_STATE_ERROR ||
                    	       cu_cmd->state == ERT_CMD_STATE_ABORT ||
                    	       cu_cmd->state == ERT_CMD_STATE_TIMEOUT ||
                    	       cu_cmd->state >= ERT_CMD_STATE_NORESPONSE) {
                        //Check for invalid/error execo state
                        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "Session id: %d, type: %s, Unexpected ERT_CMD_STATE. state=%s", s_handle.session_id, xma_core::get_session_name(s_handle.session_type).c_str(), xma_core::get_cu_cmd_state(cu_cmd).c_str());
                        std::string err_tmp("XMA FATAL: Session id: ");
                        err_tmp.append(std::to_string(s_handle.session_id));
                        err_tmp.append(", type: ");
                        err_tmp.append(xma_core::get_session_name(s_handle.session_type));
                        err_tmp.append("; Unexpected ERT_CMD_STATE. state=");
                        err_tmp.append(xma_core::get_cu_cmd_state(cu_cmd));
                        throw std::runtime_error(err_tmp);
                    }
                }
            }
        } else {
            notify_execbo_is_free = true;
        }
        //In this mode schedule_work_item still waits for this
        if (notify_execbo_is_free) {
            priv1->execbo_is_free.notify_all();
        }
    } else {
        if (priv1->num_cu_cmds != 0) {
            bool notify_work_item_done_1plus = false;
            bool notify_execbo_is_free = false;

            for (auto ebo_it = priv1->execbo_to_check.begin(); ebo_it != priv1->execbo_to_check.end(); /*incr inside*/) {
                int32_t val = *ebo_it;
                auto& ebo = priv1->kernel_execbos[val];
                if (ebo.in_use) {
                    ert_start_kernel_cmd *cu_cmd = 
                        (ert_start_kernel_cmd*)ebo.data;
                    if (cu_cmd->state == ERT_CMD_STATE_COMPLETED) {
                        if (s_handle.session_type < XMA_ADMIN) {
                            priv1->kernel_complete_count++;
                            priv1->kernel_complete_total++;
                        }
                        ebo.in_use = false;
                        cu_cmd->state = ERT_CMD_STATE_MAX;
                        notify_work_item_done_1plus = true;
                        notify_execbo_is_free = true;
                        priv1->CU_cmds.erase(ebo.cu_cmd_id1);
                        priv1->num_cu_cmds--;
                        //priv1->execbo_lru.emplace_back(val);Let's not reuse execbo immediately after completion
                        ebo_it = priv1->execbo_to_check.erase(ebo_it);
                    } else if (cu_cmd->state == ERT_CMD_STATE_ERROR ||
                    	       cu_cmd->state == ERT_CMD_STATE_ABORT ||
                    	       cu_cmd->state == ERT_CMD_STATE_TIMEOUT ||
                    	       cu_cmd->state >= ERT_CMD_STATE_NORESPONSE) {
                        //Check for invalid/error execo state
                        xma_logmsg(XMA_ERROR_LOG, XMAUTILS_MOD, "Session id: %d, type: %s, Unexpected ERT_CMD_STATE. state=%s", s_handle.session_id, xma_core::get_session_name(s_handle.session_type).c_str(), xma_core::get_cu_cmd_state(cu_cmd).c_str());
                        std::string err_tmp("XMA FATAL: Session id: ");
                        err_tmp.append(std::to_string(s_handle.session_id));
                        err_tmp.append(", type: ");
                        err_tmp.append(xma_core::get_session_name(s_handle.session_type));
                        err_tmp.append("; Unexpected ERT_CMD_STATE. state=");
                        err_tmp.append(xma_core::get_cu_cmd_state(cu_cmd));
                        throw std::runtime_error(err_tmp);
                    } else {
                        ebo_it++;
                    }
                } else {
                    notify_execbo_is_free = true;
                    ebo_it++;
                }
            }

            if (notify_execbo_is_free) {
                priv1->execbo_is_free.notify_all();
            }
            if (notify_work_item_done_1plus) {
                priv1->work_item_done_1plus.notify_one();//Unblock one thread;Though only one is used anyway
                priv1->kernel_done_or_free.notify_all();
                if (priv1->slowest_element) {
                    std::this_thread::yield();
                }
            } else if (priv1->kernel_complete_count != 0) {
                priv1->work_item_done_1plus.notify_one();//Unblock one thread;Though only one is used anyway
                if (priv1->slowest_element) {
                    std::this_thread::yield();
                }
            }
        } else {
            priv1->work_item_done_1plus.notify_one();//Unblock one thread;Though only one is used anyway
            priv1->kernel_done_or_free.notify_all();
            priv1->execbo_is_free.notify_all();
        }
    }

    return XMA_SUCCESS;
}

void logmsg(XmaLogLevelType level, const std::string& tag, const std::string& msg) {
    //TODO
    return;
}

} // namespace utils
} // namespace xma_core
