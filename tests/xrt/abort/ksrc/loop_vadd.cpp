/*
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ap_int.h"

extern "C" {
void loop_vadd(int *in1,              // Read-Only Vector 1
               int *in2,              // Read-Only Vector 2
               int *out,              // Output Vector
               int size,              // Size in integer
               int hang               // Invoke loop
               )
{
  int cnt = 1;
  while (hang + cnt) {
    for (int i = 0; i < size; i++) {
      out[i] = in1[i] + in2[i] + hang;
    }
    cnt = 0;
  }
}

}
