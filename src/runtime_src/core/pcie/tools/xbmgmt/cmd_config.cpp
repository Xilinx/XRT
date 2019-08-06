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
    "--device [--card bdf] --security level\n"
    "--show [--daemon | --device [--card bdf] ]";

static struct config {
    std::string host;
} config;

static const std::string configFile("/etc/msd.conf");

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
        std::cout << "Can't open config file for writing" << std::endl;
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
    std::cout << "Daemons:" << std::endl;
    writeConf(std::cout, config);
}

static void showDevConf(std::shared_ptr<pcidev::pci_device>& dev)
{
    std::string errmsg;
    int lvl = 0;

    dev->sysfs_get("icap", "sec_level", errmsg, lvl);
    if (!errmsg.empty()) {
        std::cout << "can't read security level from " << dev->sysfs_name <<
            " : " << errmsg << std::endl;
        return;
    }
    std::cout << dev->sysfs_name << ":" << std::endl;
    std::cout << "\t" << "security level: " << lvl << std::endl;
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

    if (!device)
        return 0;

    if (index != UINT_MAX) {
        auto dev = pcidev::get_dev(index, false);
        showDevConf(dev);
        return 0;
    }

    for (unsigned i = 0; i < pcidev::get_dev_total(false); i++) {
        auto dev = pcidev::get_dev(i, false);
        showDevConf(dev);
    }

    return 0;
}

static void updateDevConf(std::shared_ptr<pcidev::pci_device>& dev,
    std::string lvl)
{
    std::string errmsg;
    dev->sysfs_put("icap", "sec_level", errmsg, lvl);
    if (!errmsg.empty()) {
        std::cout << "can't set security level for " << dev->sysfs_name << " : "
            << errmsg << std::endl;
    }
}

static int device(int argc, char *argv[])
{
    unsigned int index = UINT_MAX;
    std::string lvl;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "security", required_argument, nullptr, '1' },
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
            break;
        default:
            return -EINVAL;
        }
    }

    if (lvl.empty())
        return -EINVAL;

    if (index != UINT_MAX) {
        auto dev = pcidev::get_dev(index, false);
        updateDevConf(dev, lvl);
        return 0;
    }

    for (unsigned i = 0; i < pcidev::get_dev_total(false); i++) {
        auto dev = pcidev::get_dev(i, false);
        updateDevConf(dev, lvl);
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
    return -EINVAL;
}
