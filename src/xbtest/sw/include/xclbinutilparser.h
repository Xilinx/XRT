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

#ifndef _XCLBINUTILPARSER_H
#define _XCLBINUTILPARSER_H

#include "xjsonparser.h"

class XclbinUtilParser: public XJsonParser
{

private:
    std::string m_device_name;
    uint        m_device_idx;
    std::string m_xclbin;
    std::string m_xclbin_uuid;
    std::atomic<bool> *m_abort;

    std::string CONNECTIVITY        = "connectivity";
    std::string M_COUNT             = "m_count";
    std::string M_CONNECTION        = "m_connection";
    std::string ARG_INDEX           = "arg_index";
    std::string M_IP_LAYOUT_INDEX   = "m_ip_layout_index";
    std::string MEM_DATA_INDEX      = "mem_data_index";

    Connectivity_t m_connectivity;

public:

    XclbinUtilParser( std::string device_name, uint device_idx, std::string xclbin, std::string xclbin_uuid, Global_Config_t global_config, std::atomic<bool> *abort );
    ~XclbinUtilParser();

    bool Parse(); // Implemented but not used
    bool ParseConnectivity();
    void PrintConnectivity();
    void PrintRequiredNotFound( std::vector<std::string> node_title_in );
    Connectivity_t GetConnectivity();
};

#endif /* _XCLBINUTILPARSER_H */
