/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author: Hem C Neema
 * Simple command line utility to inetract with SDX EDGE devices
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

#include "utils.h"

#define BIT(x) (0x1 << x)

std::string parseCUStatus(unsigned int val)  {
    char delim = '(';
    std::string status;
    if (val & 0x1) {
	status += delim;
	status += "START";
	delim = '|';
    }
    if (val & 0x2) {
	status += delim;
	status += "DONE";
	delim = '|';
    }
    if (val & 0x4) {
	status += delim;
	status += "IDLE";
	delim = '|';
    }
    if (val & 0x8) {
	status += delim;
	status += "READY";
	delim = '|';
    }
    if (val & 0x10) {
	status += delim;
	status += "RESTART";
	delim = '|';
    }
    if (status.size())
	status += ')';
    else if (val == 0x0)
	status = "(--)";
    else
	status = "(UNKNOWN)";
    return status;
}

std::string unitConvert(size_t size){
    int i = 0, bit_shift=6;
    std::string ret, unit[8]={"Byte", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"};
    if(size < 64)
        return std::to_string(size)+" "+unit[i];
    if(!(size & (size-1)))
      bit_shift = 0;
    while( (size>>bit_shift) !=0 && i<8){
        ret = std::to_string(size);
        size >>= 10;
        i++;
    }
    return ret+" "+unit[i-1];

}
