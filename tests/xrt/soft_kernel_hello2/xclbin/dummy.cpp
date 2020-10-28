#ifndef __SYNTHESIS__
#include <stdio.h>
#endif
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ap_int.h"

extern "C" {

void dummy(unsigned int arg1)
{

#pragma HLS INTERFACE s_axilite port=arg1 bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE ap_ctrl_chain port=return bundle=control 

    unsigned int tmp = arg1;
    tmp |= 0xF
    arg1 = tmp;

}

}//extern C



