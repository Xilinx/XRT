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
#include "xbmgmt.h"

const char *subCmdConfigDesc = "Parse or update mailbox configuration";
const char *subCmdConfigUsage =
    "--update [--host ip-or-hostname-for-peer]\n"
    "--show\n"
    "--purge";

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

static int loadConf(struct config& conf)
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

static int update(int argc, char *argv[])
{
    if (argc < 1)
        return -EINVAL;

    const option opts[] = {
        { "host", required_argument, nullptr, '0' },
    };

    // Load current config.
    int ret = loadConf(config);
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

static int purge(int argc, char *argv[])
{
    if (argc != 1)
        return -EINVAL;
    return remove(configFile.c_str());
}

static int list(int argc, char *argv[])
{
    if (argc != 1)
        return -EINVAL;

    int ret = loadConf(config);
    if (ret != 0)
        return ret;

    writeConf(std::cout, config);
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
        return list(argc, argv);
    if (op.compare("--update") == 0)
        return update(argc, argv);
    if (op.compare("--purge") == 0)
        return purge(argc, argv);
    return -EINVAL;
}
