/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include <getopt.h>
#include <unistd.h>
#include <climits>
#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"

const char *subCmdConfigDesc = "Parse or update daemon/device configuration";
const char *subCmdConfigUsage =
    "--daemon --host ip-or-hostname-for-peer\n"
    "--device [--card bdf] [--security level] [--runtime_clk_scale en(dis)able] [--cs_threshold_power_override val] [--cs_reset val]\n"
    "--show [--daemon | --device [--card bdf]\n"
    "--enable_retention [--ddr] [--card bdf]\n"
    "--disable_retention [--ddr] [--card bdf]";

static struct config {
    std::string host;
} config;

static const std::string configFile("/etc/msd.conf");

enum configs {
    CONFIG_SECURITY = 0,
    CONFIG_CLK_SCALING,
    CONFIG_CS_THRESHOLD_POWER_OVERRIDE,
    CONFIG_CS_RESET,
};
typedef configs configType;

static int splitLine(std::string& line, std::string& key, std::string& value)
{
    auto pos = line.find('=', 0);
    if (pos == std::string::npos) {
        std::cout << "Bad config line: " << line << std::endl;
        return -EINVAL;
    }
    key = line.substr(0, pos);
    value = line.substr(pos + 1);
    return 0;
}

std::string getHostname(void)
{
    char buf[512];

    if(gethostname(buf, sizeof(buf)) < 0)
        return std::string("");
    return std::string(buf);
}

static int loadDaemonConf(struct config& conf)
{
    // Load default
    conf.host = getHostname();

    std::ifstream cfile(configFile);
    if (!cfile.good())
        return 0;

    // Load persistent value, may overwrite default one
    for (std::string line; std::getline(cfile, line);) {
        std::string key, value;
        int ret = splitLine(line, key, value);
        if (ret != 0)
            return ret;
        if (key.compare("host") == 0) {
            conf.host = value;
        } else {
            // Ignore unknown keys
            std::cout << "Unknown config key: " << key << std::endl;
        }
    }

    return 0;
}

static void writeConf(std::ostream& ostr, struct config& conf)
{
    ostr << "host=" << conf.host << std::endl;
}

static int daemon(int argc, char *argv[])
{
    if (argc < 1)
        return -EINVAL;

    const option opts[] = {
        { "host", required_argument, nullptr, '0' },
        { nullptr, 0, nullptr, 0 },
    };

    // Load current config.
    int ret = loadDaemonConf(config);
    if (ret != 0)
        return ret;

    // Update config based on input arguements.
    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            config.host = optarg;
            break;
        default:
            return -EINVAL;
        }
    }

    // Write it back.
    std::ofstream cfile(configFile);
    if (!cfile.good()) {
        std::cout << "Error: Can't open config file for writing" << std::endl;
        return -EINVAL;
    }

    writeConf(cfile, config);
    return 0;
}

// Remove daemon config file
static int purge(int argc, char *argv[])
{
    if (argc != 1)
        return -EINVAL;
    return remove(configFile.c_str());
}

static void showDaemonConf(void)
{
    (void) loadDaemonConf(config);
    std::cout << "Daemon:" << std::endl;
    std::cout << "\t";
    writeConf(std::cout, config);
}

static void showDevConf(std::shared_ptr<pcidev::pci_device>& dev)
{
    std::string errmsg, svl;
    int lvl = 0;

    dev->sysfs_get("icap", "sec_level", errmsg, lvl, 0);
    if (!errmsg.empty()) {
        std::cout << "Error: can't read security level from " << dev->sysfs_name
			<< " : " << errmsg << "\n";
    } else {
        std::cout << dev->sysfs_name << ":\n";
        std::cout << "\t" << "security level: " << lvl << "\n";
    }

    lvl = 0;
    errmsg = "";
    dev->sysfs_get("xmc", "scaling_enabled", errmsg, lvl, 0);
    if (!errmsg.empty()) {
        std::cout << "Error: can't read scaling_enabled status from " <<
            dev->sysfs_name << " : " << errmsg << std::endl;
    } else {
        std::cout << "\t" << "Runtime clock scaling enabled status: " <<
            lvl << std::endl;
    }

    errmsg = "";
    dev->sysfs_get("xmc", "scaling_threshold_power_override", errmsg, svl);
    if (!errmsg.empty()) {
        std::cout << "Error: can't read scaling_threshold_power_override from "
			<< dev->sysfs_name << " : " << errmsg << std::endl;
    } else {
        std::cout << "\t" << "scaling_threshold_power_override: " <<
            svl << std::endl;
    }

    dev->sysfs_get("icap", "data_retention", errmsg, lvl, 0);
    if (!errmsg.empty()) {
        std::cout << "Error: can't read data_retention from " << dev->sysfs_name
            << " : " << errmsg << "\n";
            std::cout << "See dmesg log for details" << std::endl;
    } else {
        std::cout << "\tData Retention: " << (lvl ? "Enable" : "Disable") << "\n";
    }
}

static int show(int argc, char *argv[])
{
    unsigned int index = UINT_MAX;
    bool daemon = false;
    bool device = false;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "daemon", no_argument, nullptr, '1' },
        { "device", no_argument, nullptr, '2' },
        { nullptr, 0, nullptr, 0 },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            daemon = true;
            break;
        case '2':
            device = true;
            break;
        default:
            return -EINVAL;
        }
    }

    // User should specify either one or none of them, not both
    if (daemon && device)
        return -EINVAL;

    // Show both daemon and device configs, if none specified
    if (!daemon && !device) {
        daemon = true;
        device = true;
    }

    if (daemon)
        showDaemonConf();

    if (device) {
        if (index != UINT_MAX) {
            auto dev = pcidev::get_dev(index, false);
            showDevConf(dev);
            return 0;
        }

        for (unsigned i = 0; i < pcidev::get_dev_total(false); i++) {
            auto dev = pcidev::get_dev(i, false);
            showDevConf(dev);
        }
    }

    return 0;
}

static void updateDevConf(pcidev::pci_device *dev,
    const std::string lvl, configType config_type)
{
    std::string errmsg;

    switch(config_type) {
    case CONFIG_SECURITY:
        dev->sysfs_put("icap", "sec_level", errmsg, lvl);
        if (!errmsg.empty()) {
            std::cout << "Error: Failed to set security level for " <<
                dev->sysfs_name << "\n";
            std::cout << "See dmesg log for details" << std::endl;
        }
        break;
    case CONFIG_CLK_SCALING:
        dev->sysfs_put("xmc", "scaling_enabled", errmsg, lvl);
        if (!errmsg.empty()) {
            std::cout << "Error: Failed to update clk scaling status for " <<
                dev->sysfs_name << "\n";
            std::cout << "See dmesg log for details" << std::endl;
        }
        break;
    case CONFIG_CS_THRESHOLD_POWER_OVERRIDE:
        dev->sysfs_put("xmc", "scaling_threshold_power_override", errmsg, lvl);
        if (!errmsg.empty()) {
            std::cout << "Error: Failed to update clk scaling power threshold for " <<
                dev->sysfs_name << "\n";
            std::cout << "See dmesg log for details" << std::endl;
        }
        break;
    case CONFIG_CS_RESET:
        dev->sysfs_put("xmc", "scaling_reset", errmsg, lvl);
        if (!errmsg.empty()) {
            std::cout << "Error: Failed to reset clk scaling feature for " <<
                dev->sysfs_name << "\n";
            std::cout << "See dmesg log for details" << std::endl;
        }
        break;
    }
}

static int device(int argc, char *argv[])
{
    unsigned int index = UINT_MAX;
    std::string lvl;
    configType config_type = (configType)-1;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "security", required_argument, nullptr, '1' },
        { "runtime_clk_scale", required_argument, nullptr, '2' },
        { "cs_threshold_power_override", required_argument, nullptr, '3' },
        { "cs_reset", required_argument, nullptr, '4' },
        { nullptr, 0, nullptr, 0 },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            lvl = optarg;
            config_type = CONFIG_SECURITY;
            break;
        case '2':
            lvl = optarg;
            config_type = CONFIG_CLK_SCALING;
            break;
        case '3':
            lvl = optarg;
            config_type = CONFIG_CS_THRESHOLD_POWER_OVERRIDE;
            break;
        case '4':
            lvl = optarg;
            config_type = CONFIG_CS_RESET;
            break;
        default:
            return -EINVAL;
        }
    }

    if (lvl.empty())
        return -EINVAL;

    if (index != UINT_MAX) {
        auto dev = pcidev::get_dev(index, false);
        updateDevConf(dev.get(), lvl, config_type);
        return 0;
    }

    for (unsigned i = 0; i < pcidev::get_dev_total(false); i++) {
        auto dev = pcidev::get_dev(i, false);
        updateDevConf(dev.get(), lvl, config_type);
    }

    return 0;
}

static void memoryRetention(pcidev::pci_device *dev, unsigned int mem_type, bool enable)
{
    std::string errmsg;

    dev->sysfs_put("icap", "data_retention", errmsg, enable);
    if (!errmsg.empty()) {
        std::cout << "Error: Failed to set data_retention for " <<
            dev->sysfs_name << "\n";
        std::cout << "See dmesg log for details" << std::endl;
    } else {
        std::cout << (enable ? "Enable" : "Disable") << " successfully" << std::endl;
    }
}

static int memory(int argc, char *argv[], bool enable)
{
    unsigned int index = UINT_MAX;
    std::string lvl;
    unsigned int mem_type = 2;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "ddr", no_argument, nullptr, '1' },
        { "hbm", no_argument, nullptr, '2' },
        { nullptr, 0, nullptr, 0 },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            mem_type = 0;
            break;
        case '2':
            mem_type = 1;
            break;
        default:
            return -EINVAL;
        }
    }

    if (mem_type & 2)
        return -EINVAL;

    if (index != UINT_MAX) {
        auto dev = pcidev::get_dev(index, false);
        memoryRetention(dev.get(), mem_type, enable);
        return 0;
    }
    for (unsigned i = 0; i < pcidev::get_dev_total(false); i++) {
        auto dev = pcidev::get_dev(i, false);
        memoryRetention(dev.get(), mem_type, enable);
    }
    return 0;
}

int configHandler(int argc, char *argv[])
{
    sudoOrDie();

    if (argc < 2)
        return -EINVAL;

    std::string op = argv[1];
    argc--;
    argv++;

    if (op.compare("--show") == 0)
        return show(argc, argv);
    if (op.compare("--daemon") == 0)
        return daemon(argc, argv);
    if (op.compare("--device") == 0)
        return device(argc, argv);
    if (op.compare("--purge") == 0) // hidden opt to remove daemon config file
        return purge(argc, argv);
    if (op.compare("--enable_retention") == 0)
        return memory(argc, argv, true);
    if (op.compare("--disable_retention") == 0)
        return memory(argc, argv, false);
    return -EINVAL;
}
