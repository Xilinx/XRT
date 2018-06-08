/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2014 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.

//Initialize Messages
#include "xcl_macros.h"


#define AQUIRE_MUTEX() \
mtx.lock(); \

#define RELEASE_MUTEX() \
mtx.unlock();

#define RPC_PROLOGUE(func_name) \
    unix_socket* _s_inst = sock; \
    func_name##_call c_msg; \
    func_name##_response r_msg; \
    AQUIRE_MUTEX()

#define SERIALIZE_AND_SEND_MSG(func_name)\
     unsigned c_len = c_msg.ByteSize(); \
    buf_size = alloc_void(c_len); \
    bool rv = c_msg.SerializeToArray(buf,c_len); \
    if(rv == false){std::cerr<<"FATAL ERROR:protobuf SerializeToArray failed"<<std::endl;exit(1);} \
    \
    ci_msg.set_size(c_len); \
    ci_msg.set_xcl_api(func_name##_n); \
    unsigned ci_len = ci_msg.ByteSize(); \
    rv = ci_msg.SerializeToArray(ci_buf,ci_len); \
    if(rv == false){std::cerr<<"FATAL ERROR:protobuf SerializeToArray failed"<<std::endl;exit(1);} \
    \
    _s_inst->sk_write(ci_buf,ci_len); \
    _s_inst->sk_write(buf,c_len); \
    \
    _s_inst->sk_read(ri_buf,ri_msg.ByteSize()); \
    rv = ri_msg.ParseFromArray(ri_buf,ri_msg.ByteSize()); \
    assert(true == rv);\
    buf_size = alloc_void(ri_msg.size()); \
    _s_inst->sk_read(buf,ri_msg.size()); \
    rv = r_msg.ParseFromArray(buf,ri_msg.size()); \
    assert(true == rv);\

//RELEASE BUFFER MEMORIES
#define FREE_BUFFERS() \
  RELEASE_MUTEX() 

#define xclSetEnvironment_SET_PROTOMESSAGE() \
  for (auto i : mEnvironmentNameValueMap) \
  { \
    xclSetEnvironment_call_namevaluepair* namevalpair = c_msg.add_environment(); \
    namevalpair->set_name(i.first); \
    namevalpair->set_value(i.second); \
  }\

#define xclSetEnvironment_SET_PROTO_RESPONSE() \
    ack = r_msg.ack()
    

#define xclSetEnvironment_RETURN()\
    //return size;

#define xclSetEnvironment_RPC_CALL(func_name) \
    RPC_PROLOGUE(func_name); \
    xclSetEnvironment_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclSetEnvironment_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclSetEnvironment_RETURN();


#define xclLoadBitstream_SET_PROTOMESSAGE(func_name,xmlfile,dlopenfilename,deviceDirectory,binaryDirectory,verbose) \
  c_msg.set_xmlfile(xmlfile); \
  c_msg.set_dlopenfilename(dlopenfilename); \
  c_msg.set_devicename(mDeviceInfo.mName); \
  c_msg.set_devicedirectory(deviceDirectory); \
  c_msg.set_binarydirectory(binaryDirectory); \
  c_msg.set_verbose(verbose); \
  for (auto i : mDdrBanks) \
  { \
    const uint64_t bankSize = i.ddrSize; \
    xclLoadBitstream_call_ddrbank* ddrbank = c_msg.add_ddrbanks(); \
    ddrbank->set_size(bankSize); \
  }\

#define xclLoadBitstream_SET_PROTO_RESPONSE() \
    ack = r_msg.ack()
    

#define xclLoadBitstream_RETURN()\
    //return size;

#define xclLoadBitstream_RPC_CALL(func_name,xmlfile,dlopenfilename,deviceDirectory,binaryDirectory,verbose) \
    RPC_PROLOGUE(func_name); \
    xclLoadBitstream_SET_PROTOMESSAGE(func_name,xmlfile,dlopenfilename,deviceDirectory,binaryDirectory,verbose); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclLoadBitstream_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclLoadBitstream_RETURN();


#define xclAllocDeviceBuffer_SET_PROTOMESSAGE(func_name,ddraddress,size) \
    c_msg.set_ddraddress(ddraddress); \
    c_msg.set_size(size);


#define xclAllocDeviceBuffer_SET_PROTO_RESPONSE() \
    ack = r_msg.ack()
    

#define xclAllocDeviceBuffer_RETURN()\
    //return size;

#define xclAllocDeviceBuffer_RPC_CALL(func_name,ddraddress,size) \
    RPC_PROLOGUE(func_name); \
    xclAllocDeviceBuffer_SET_PROTOMESSAGE(func_name,ddraddress,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclAllocDeviceBuffer_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclAllocDeviceBuffer_RETURN();

#define xclFreeDeviceBuffer_SET_PROTOMESSAGE(func_name,ddraddress) \
    c_msg.set_ddraddress(ddraddress);


#define xclFreeDeviceBuffer_SET_PROTO_RESPONSE() \
    ack = r_msg.ack()
    

#define xclFreeDeviceBuffer_RETURN()\
    //return size;

#define xclFreeDeviceBuffer_RPC_CALL(func_name,ddraddress) \
    RPC_PROLOGUE(func_name); \
    xclFreeDeviceBuffer_SET_PROTOMESSAGE(func_name,ddraddress); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclFreeDeviceBuffer_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclFreeDeviceBuffer_RETURN();
//------------------------------------------------------------
//--------------------xclWriteAddrSpaceDeviceRam--------------------------------
//Generate call and info message 
#define xclWriteAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,addr,data,size) \
    c_msg.set_addr(addr); \
    c_msg.set_data((char*) data, size); \
    c_msg.set_size(size); 


#define xclWriteAddrSpaceDeviceRam_SET_PROTO_RESPONSE() \
    if (!r_msg.valid()) \
      size = -1;
    

#define xclWriteAddrSpaceDeviceRam_RETURN()\
    //return size;

#define xclWriteAddrSpaceDeviceRam_RPC_CALL(func_name,address_space,address,data,size) \
    RPC_PROLOGUE(func_name); \
    xclWriteAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,address,data,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclWriteAddrSpaceDeviceRam_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclWriteAddrSpaceDeviceRam_RETURN();

//--------------------xclWriteAddrKernelCtrl--------------------------------
//Generate call and info message 
#define xclWriteAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,addr,data,size,kernelArgsInfo) \
    c_msg.set_addr(addr); \
    c_msg.set_data((char*) data, size); \
    c_msg.set_size(size); \
    for (auto i : kernelArgsInfo) { \
    	xclWriteAddrKernelCtrl_call_kernelInfo* kernelInfo = c_msg.add_kernel_info();\
    	kernelInfo->set_addr(i.first);\
    	kernelInfo->set_size(i.second.second);\
    	kernelInfo->set_name(i.second.first);\
    }\


#define xclWriteAddrKernelCtrl_SET_PROTO_RESPONSE() \
    if (!r_msg.valid()) \
      size = -1;
    

#define xclWriteAddrKernelCtrl_RETURN()\
    //return size;

#define xclWriteAddrKernelCtrl_RPC_CALL(func_name,address_space,address,data,size,kernelArgsInfo) \
    RPC_PROLOGUE(func_name); \
    xclWriteAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,address,data,size,kernelArgsInfo); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclWriteAddrKernelCtrl_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclWriteAddrKernelCtrl_RETURN();

//-----------------------xclReadAddrSpaceDeviceRam----------------------------
//Generate call and info message 
#define xclReadAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,addr,data,size) \
    c_msg.set_addr(addr); \
    c_msg.set_size(size); \


#define xclReadAddrSpaceDeviceRam_SET_PROTO_RESPONSE(datax,size) \
    if (!r_msg.valid()) \
      size = -1; \
    else { \
      memcpy(datax,r_msg.data().c_str(),size);\
    }
    

#define xclReadAddrSpaceDeviceRam_RETURN()\
  //  return size;

#define xclReadAddrSpaceDeviceRam_RPC_CALL(func_name,address_space,address,data,size) \
    RPC_PROLOGUE(func_name); \
    xclReadAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,address,data,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclReadAddrSpaceDeviceRam_SET_PROTO_RESPONSE(data,size); \
    FREE_BUFFERS(); \
    xclReadAddrSpaceDeviceRam_RETURN();   
//-----------------------xclReadAddrKernelCtrl----------------------------
//Generate call and info message 
#define xclReadAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,addr,data,size) \
    c_msg.set_addr(addr); \
    c_msg.set_size(size); \


#define xclReadAddrKernelCtrl_SET_PROTO_RESPONSE(datax,size) \
    if (!r_msg.valid()) \
      size = -1; \
    else { \
      memcpy(datax,r_msg.data().c_str(),size);\
    }
    

#define xclReadAddrKernelCtrl_RETURN()\
  //  return size;

#define xclReadAddrKernelCtrl_RPC_CALL(func_name,address_space,address,data,size) \
    RPC_PROLOGUE(func_name); \
    xclReadAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,address,data,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclReadAddrKernelCtrl_SET_PROTO_RESPONSE(data,size); \
    FREE_BUFFERS(); \
    xclReadAddrKernelCtrl_RETURN();  

//-------------------xclClose---------------------------------
#define xclClose_SET_PROTOMESSAGE(func_name,dev_handle) \
    c_msg.set_xcldevicehandle((char*)dev_handle);\
    c_msg.set_closeall(mCloseAll);

#define xclClose_SET_PROTO_RESPONSE() \
  simulator_started = false;
    
#define xclClose_RETURN() \
	return;

#define xclClose_RPC_CALL(func_name,dev_handle) \
    RPC_PROLOGUE(func_name); \
    xclClose_SET_PROTOMESSAGE(func_name,dev_handle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclClose_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); 
  //  xclClose_RETURN();

//-----------xclCopyBufferHost2Device-----------------
#define xclCopyBufferHost2Device_SET_PROTOMESSAGE(func_name,dev_handle,dest,src,size,seek,space) \
    c_msg.set_xcldevicehandle((char*)dev_handle); \
    c_msg.set_dest(dest); \
    c_msg.set_src((char*)src,size); \
    c_msg.set_size(size); \
    c_msg.set_seek(seek); \
    c_msg.set_space(space);
   
#define xclCopyBufferHost2Device_SET_PROTO_RESPONSE() \
 //   uint64_t ret = r_msg.size();
    

#define xclCopyBufferHost2Device_RETURN() \
   // return ret;

#define xclCopyBufferHost2Device_RPC_CALL(func_name,dev_handle,dest,src,size,seek,space) \
    RPC_PROLOGUE(func_name); \
    xclCopyBufferHost2Device_SET_PROTOMESSAGE(func_name,dev_handle,dest,src,size,seek,space); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclCopyBufferHost2Device_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclCopyBufferHost2Device_RETURN();

//-----------xclCopyBufferDevice2Host-----------------
#define xclCopyBufferDevice2Host_SET_PROTOMESSAGE(func_name,dev_handle,dest,src,size,skip,space) \
    c_msg.set_xcldevicehandle((char*)dev_handle); \
    c_msg.set_dest((char*)dest,size); \
    c_msg.set_src(src); \
    c_msg.set_size(size); \
    c_msg.set_skip(skip); \
    c_msg.set_space(space);
   
#define xclCopyBufferDevice2Host_SET_PROTO_RESPONSE(c_dest) \
    uint64_t ret = r_msg.size();\
    memcpy(c_dest,r_msg.dest().c_str(),ret);
    

#define xclCopyBufferDevice2Host_RETURN() \
    //return ret;

#define xclCopyBufferDevice2Host_RPC_CALL(func_name,dev_handle,dest,src,size,skip,space) \
    RPC_PROLOGUE(func_name); \
    xclCopyBufferDevice2Host_SET_PROTOMESSAGE(func_name,dev_handle,dest,src,size,skip,space); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclCopyBufferDevice2Host_SET_PROTO_RESPONSE(dest); \
    FREE_BUFFERS(); \
    xclCopyBufferDevice2Host_RETURN();

   
//----------xclPerfMonReadCounters------------
#define xclPerfMonReadCounters_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
    c_msg.set_slotname(slotname); \
    c_msg.set_accel(accel); \
   
#define xclPerfMonReadCounters_SET_PROTO_RESPONSE() \
    wr_byte_count    = r_msg.wr_byte_count(); \
    wr_trans_count   = r_msg.wr_trans_count(); \
    total_wr_latency = r_msg.total_wr_latency(); \
    rd_byte_count    = r_msg.rd_byte_count(); \
    rd_trans_count   = r_msg.rd_trans_count(); \
    total_rd_latency = r_msg.total_rd_latency(); 
    

#define xclPerfMonReadCounters_RETURN()

#define xclPerfMonReadCounters_RPC_CALL(func_name,wr_byte_count,wr_trans_count,total_wr_latency,rd_byte_count,rd_trans_count,total_rd_latency,sampleIntervalUsec,slotname,accel) \
    RPC_PROLOGUE(func_name); \
    xclPerfMonReadCounters_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclPerfMonReadCounters_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclPerfMonReadCounters_RETURN();

//----------xclPerfMonGetTraceCount------------
#define xclPerfMonGetTraceCount_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
  c_msg.set_ack(ack); \
  c_msg.set_slotname(slotname); \
  c_msg.set_accel(accel);
   
#define xclPerfMonGetTraceCount_SET_PROTO_RESPONSE() \
    no_of_samples = r_msg.no_of_samples();
    

#define xclPerfMonGetTraceCount_RPC_CALL(func_name,ack,no_of_samples,slotname,accel) \
    RPC_PROLOGUE(func_name); \
    xclPerfMonGetTraceCount_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclPerfMonGetTraceCount_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS();

//----------xclPerfMonReadTrace------------
#define xclPerfMonReadTrace_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
    c_msg.set_ack(ack); \
    c_msg.set_slotname(slotname); \
    c_msg.set_accel(accel);
   
#define xclPerfMonReadTrace_SET_PROTO_RESPONSE() \
    samplessize = r_msg.output_data_size(); \

#define xclPerfMonReadTrace_RPC_CALL(func_name,ack,samplessize,slotname,accel) \
    RPC_PROLOGUE(func_name); \
    xclPerfMonReadTrace_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclPerfMonReadTrace_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS();

//----------xclWriteHostEvent------------
#define xclWriteHostEvent_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
    c_msg.set_ack(ack); \
    c_msg.set_slot_n(slot_n);

#define xclWriteHostEvent_SET_PROTO_RESPONSE() \
    samplessize = r_msg.output_data_size(); \

#define xclWriteHostEvent_RPC_CALL(func_name,type,id) \
    RPC_PROLOGUE(func_name); \
    xclWriteHostEvent_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclWriteHostEvent_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS();

//----------xclGetDeviceTimestamp------------
#define xclGetDeviceTimestamp_SET_PROTOMESSAGE() \
  if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
  c_msg.set_ack(ack);
   
#define xclGetDeviceTimestamp_SET_PROTO_RESPONSE() \
    deviceTimeStamp = r_msg.device_timestamp(); \

#define xclGetDeviceTimestamp_RPC_CALL(func_name,ack,deviceTimeStamp) \
    RPC_PROLOGUE(func_name); \
    xclGetDeviceTimestamp_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGetDeviceTimestamp_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS();

//----------xclReadBusStatus-------------------
#define xclReadBusStatus_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return; \
    }\
    c_msg.set_slot_n(slot_n);

#define xclReadBusStatus_SET_PROTO_RESPONSE() \
  idle_bus_cycles = r_msg.idle_bus_cycles();

#define xclReadBusStatus_RPC_CALL(func_name,idle_bus_cycles,slot_n) \
  RPC_PROLOGUE(func_name); \
  xclReadBusStatus_SET_PROTOMESSAGE(); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclReadBusStatus_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclGetDebugMessages-------------------
#define xclGetDebugMessages_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return; \
    }\
    c_msg.set_ack(ack); \
    c_msg.set_force(force);

#define xclGetDebugMessages_SET_PROTO_RESPONSE() \
  displayMsgs = r_msg.display_msgs();\
  logMsgs = r_msg.log_msgs();\
  stopMsgs = r_msg.stop_msgs();\

#define xclGetDebugMessages_RPC_CALL(func_name,ack,force,displayMsgs,logMsgs,stopMsgs) \
  RPC_PROLOGUE(func_name); \
  xclGetDebugMessages_SET_PROTOMESSAGE(); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclGetDebugMessages_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

