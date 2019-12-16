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

#ifndef _LOGGING_H
#define _LOGGING_H

#include "xbtestcommon.h"
#include "logging.h"

class Logging
{

private:

    Logging();

public:

    ~Logging();

    static Logging* getInstance();
    void LogMessage( LogLevel Level, std::string Message, LogLevel Verbosity );
    std::string GetFirstError();
    bool GetReportMsgCount(std::string *msg_cnt_msg);

    std::string LogLevelToString( LogLevel Level )
    {
        switch (Level)
        {
        default:
        case LOG_FAILURE:   return "FAILURE  : "; break;
        case LOG_ERROR:     return "ERROR    : "; break;
        case LOG_PASS:      return "PASS     : "; break;
        case LOG_CRIT_WARN: return "CRIT WARN: "; break;
        case LOG_WARN:      return "WARNING  : "; break;
        case LOG_INFO:      return "INFO     : "; break;
        case LOG_STATUS:    return "STATUS   : "; break;
        case LOG_DEBUG:     return "DEBUG    : "; break;
        case LOG_DESIGNER:  return "DESIGNER : "; break;
        }
    }

    void SetTimestampMode( std::string timestamp_mode );
    bool SetLoggingMode( std::string head_log, Global_Config_t global_config );

};

#endif /* _LOGGING_H */
