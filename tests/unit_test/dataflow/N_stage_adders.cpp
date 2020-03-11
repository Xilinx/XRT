/**
 * Copyright (C) 2019 Xilinx, Inc
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

/*******************************************************************************
Description:
    HLS Dataflow Example using array of HLS Stream datatype
    This example is N Stages of Vector Addition to demonstrate Array of
    Stream usage in HLS C Kernel.

    1) read_input():
        This API reads the input vector from Global Memory and writes it into
        HLS Stream inStream using blocking write command.

    2) Multiple instance of Adder():
        This API reads the input vector from inStream using blocking
        read command and increment the value by user specified increment. It writes
        the results into outStream using blocking write command.

    3) write_result():
        This API reads the result vector from outStream using blocking read
        command and write the result into Global Memory Location.

    Four Stage Adder will be implemented as below:

                     _____________
                    |             |<----- Input Vector from Global Memory
                    |  read_input |       __
                    |_____________|----->|  |
                     _____________       |  | streamArray[0]
                    |             |<-----|__|
                    |   adder_0   |       __
                    |_____________|----->|  |
                     _____________       |  | streamArray[1]
                    |             |<-----|__|
                    |   adder_1   |       __
                    |_____________|----->|  |
                     _____________       |  | streamArray[2]
                    |             |<-----|__|
                    |   adder_2   |       __
                    |_____________|----->|  |
                     _____________       |  | streamArray[3]
                    |             |<-----|__|
                    |   adder_3   |       __
                    |_____________|----->|  |
                     ______________      |  | streamArray[4]
                    |              |<----|__|
                    | write_result |
                    |______________|-----> Output result to Global Memory


*******************************************************************************/
#include <hls_stream.h>
#include <string.h>
#include <ap_int.h>

#define DATA_SIZE 4096
#define STAGES 4 //Number of Aadder Stages

//TRIPCOUNT identifiers
const unsigned int c_size = DATA_SIZE;

//read_input(): Read Data from Global Memory and write into Stream inStream
static void read_input(int *input, hls::stream<int> &inStream , int size)
{
    mem_rd: for (int i = 0 ; i < size ; i++){
#pragma HLS pipeline
#pragma HLS LOOP_TRIPCOUNT min=c_size max=c_size
        //Blocking write command to inStream
        inStream << input[i];
    }
}

//adder(): Read Input data from inStream and write the result into outStream
static void adder(hls::stream<int> &inStream , hls::stream<int> &outStream, int incr, int size)
{
    execute: for (int i = 0 ; i < size ; i ++){
#pragma HLS pipeline
#pragma HLS LOOP_TRIPCOUNT min=c_size max=c_size
        int inVar = inStream.read();
        int adderedVal = inVar + incr;
        //Blocking read command from inStream and Blocking write command
        //to outStream
        outStream << adderedVal;
    }
}

//write_result(): Read result from outStream and write the result to Global Memory
static void write_result(int *output, hls::stream<int> &outStream , int size)
{
    mem_wr: for (int i = 0 ; i < size ; i++){
#pragma HLS pipeline
#pragma HLS LOOP_TRIPCOUNT min=c_size max=c_size
        //Blocking read command from OutStream
        output[i] = outStream.read();
    }
}

extern "C" {
void N_stage_Adders(int *input, int *output, int incr, int size)
{
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem
#pragma HLS INTERFACE s_axilite port=input bundle=control
#pragma HLS INTERFACE s_axilite port=output bundle=control
#pragma HLS INTERFACE s_axilite port=incr bundle=control
#pragma HLS INTERFACE s_axilite port=size bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE ap_ctrl_chain port=return bundle=control

    //array of stream declaration
    static hls::stream<int> streamArray[STAGES+1];

    #pragma HLS dataflow
    //one read input unit for data read
    read_input(input,streamArray[0],size);
    compute_loop: for (int i = 0 ; i < STAGES ; i++){
    #pragma HLS UNROLL
        // total 4 units of adder(). each is compute vector addition
        // and sending result to immediate next unit using stream
        // datatype
        adder(streamArray[i],streamArray[i+1],incr,size);
    }
    //one write result unit to write result back to global Memory
    write_result(output,streamArray[STAGES],size);
}
}
