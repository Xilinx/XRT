/**
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

extern "C" {

void
krnl_vmult_factor(const int *in, int factor, int* out, int size)
{
  for (int i = 0; i < size; i++) {
    out[i] = in[i] * factor;
  }
}

}
