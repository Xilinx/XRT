/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */

#include "ap_axi_sdata.h"
#include "ap_int.h"
#include "hls_stream.h"

#define DWIDTH 32

typedef ap_axiu<DWIDTH, 0, 0, 0> pkt;

extern "C" {
void krnl_stream_vdatamover(hls::stream<pkt> &in,
                      hls::stream<pkt> &out,
                      int adder// Internal Stream
                      ) {
//#pragma HLS interface ap_ctrl_none port=return
#pragma HLS interface ap_ctrl_chain port=return
#pragma HLS INTERFACE s_axilite port=adder
#pragma HLS STABLE variable=adder

bool eos = false;
vdatamover:
  do {
    // Reading a and b streaming into packets
    pkt t1 = in.read();

    // Packet for output
    pkt t_out;

    // Reading data from input packet
    ap_uint<DWIDTH> in1 = t1.data;

    // Vadd operation
    ap_uint<DWIDTH> tmpOut = in1+adder;

    // Setting data and configuration to output packet
    t_out.data = tmpOut;
    t_out.last = t1.last;
    t_out.keep = -1; // Enabling all bytes

    // Writing packet to output stream
    out.write(t_out);

    if (t1.last) {
      eos = true;
    }
  } while (eos == false);




}
}
