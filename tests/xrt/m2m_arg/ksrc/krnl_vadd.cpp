/**
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */
extern "C" {

void
krnl_vadd(const int* in1, const int* in2, const int* in3, int* out, int size)
{
  for (int i = 0; i < size; i++) {
    out[i] = in1[i] + in2[i] + in3[i];
  }
}
                  
}
