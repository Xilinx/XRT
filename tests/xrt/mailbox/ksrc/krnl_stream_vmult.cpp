/**
* Copyright (C) 2020 Xilinx, Inc
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

#include "ap_axi_sdata.h"
#include "ap_int.h"
#include "hls_stream.h"

#define DWIDTH 32

typedef ap_axiu<DWIDTH, 0, 0, 0> pkt;

extern "C" {
void krnl_stream_vmult(int *in1,              // Read-Only Vector 1
                       hls::stream<pkt> &in2, // Internal Stream
                       int *out,              // Output Result
                       int size               // Size in integer
                       ) {

vmult:
  for (int i = 0; i < size; i++) {
#pragma HLS PIPELINE II = 1
    pkt v2 = in2.read();
    out[i] = in1[i] * v2.data;
  }
}
}
