/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

#ifndef _XBTESTPFMDEFPARSER_H
#define _XBTESTPFMDEFPARSER_H

#include "xjsonparser.h"

class XbtestPfmDefParser : public XJsonParser
{

private:

    std::atomic<bool> *m_abort;
    Xbtest_Pfm_Def_t m_xbtest_pfm_def;

    const bool RET_FAILURE = true;
    const bool RET_SUCCESS = false;

    const std::string PLATDEF_JSON_VERSION  = "1.0.0";

    const int  DEFAULT_DOWNLOAD_TIME    = -1;
    const int  DEFAULT_LIMIT            = -1;
    const bool DEFAULT_POWERTEST        = true;
    const int  DEFAULT_CALIBRATION      = 0;

    Json_Definition_t m_json_definition;

    const std::set<std::string> MEM_TYPES = {"DDR", "HBM"};

    Json_Parameters_t *m_device_params;

    std::string m_filename;
    std::string m_version;
    std::string m_device;

    const std::string VERSION    = "version";
    const std::string DEVICE     = "device";

    const std::string INFO      = "info";
    const std::string NAME      = "name";
    const std::string CLOCKS    = "clocks";
    const std::string FREQUENCY = "frequency";

    const std::string RUNTIME        = "runtime";
    const std::string DOWNLOAD_TIME  = "download_time";

    const std::string PHYSICAL       = "physical";
    const std::string THERMAL        = "thermal";
    const std::string TEMP_SOURCES   = "temp_sources";
    const std::string LIMIT          = "limit";
    const std::string SOURCE_NAME    = "source_name";
    const std::string A              = "a";
    const std::string B              = "b";
    const std::string C              = "c";
    const std::string T_OFFSET       = "t_offset";

    const std::string POWER          = "power";
    const std::string POWER_TARGET   = "power_target";
    const std::string MIN            = "min";
    const std::string MAX            = "max";
    const std::string POWER_SOURCES  = "power_sources";
    const std::string NAME_CURRENT   = "name_current";
    const std::string NAME_VOLTAGE   = "name_voltage";
    const std::string POWERTEST      = "powertest";
    const std::string CALIBRATION    = "calibration";
    const std::string XPE_LEAKAGE    = "xpe_leakage";

    const std::string MEMORY        = "memory";
    const std::string SIZE          = "size";
    const std::string QUANTITY      = "quantity";
    const std::string DMA_BW        = "dma_bw";
    const std::string CU_BW         = "cu_bw";
    const std::string ALT_WR_RD     = "alt_wr_rd";
    const std::string ONLY_WR       = "only_wr";
    const std::string ONLY_RD       = "only_rd";
    const std::string WRITE         = "write";
    const std::string READ          = "read";
    const std::string HIGH          = "high";
    const std::string LOW           = "low";

public:

    XbtestPfmDefParser( Json_Parameters_t *device_params, Global_Config_t global_config, std::atomic<bool> *abort );
    ~XbtestPfmDefParser();

    bool Parse();

    void PrintPlatformDef();
    void PrintRequiredNotFound( std::vector<std::string> node_title_in );
    Json_Definition_t GetJsonDefinition();

    Xbtest_Pfm_Def_t GetPlatformDef();

};

#endif /* _XBTESTPFMDEFPARSER_H */
