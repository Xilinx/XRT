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


#define xclAllocDeviceBuffer_SET_PROTOMESSAGE(func_name,ddraddress,size,p2pbuffer) \
    c_msg.set_ddraddress(ddraddress); \
    c_msg.set_size(size); \
    c_msg.set_peertopeer(p2pbuffer);


#define xclAllocDeviceBuffer_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();\
    sFileName = r_msg.filename();
    

#define xclAllocDeviceBuffer_RETURN()\
    //return size;

#define xclAllocDeviceBuffer_RPC_CALL(func_name,ddraddress,size, p2pbuffer) \
    RPC_PROLOGUE(func_name); \
    xclAllocDeviceBuffer_SET_PROTOMESSAGE(func_name,ddraddress,size,p2pbuffer); \
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

#define xclPerfMonReadCounters_RPC_CALL(func_name,wr_byte_count,wr_trans_count,total_wr_latency, \
                                        rd_byte_count,rd_trans_count,total_rd_latency, \
                                        sampleIntervalUsec,slotname,accel) \
    RPC_PROLOGUE(func_name); \
    xclPerfMonReadCounters_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclPerfMonReadCounters_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclPerfMonReadCounters_RETURN();

//----------xclPerfMonReadCounters(Streaming)------------
#define xclPerfMonReadCounters_Streaming_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
    c_msg.set_slotname(slotname);

#define xclPerfMonReadCounters_Streaming_SET_PROTO_RESPONSE() \
    str_num_tranx       = r_msg.str_num_tranx(); \
    str_data_bytes      = r_msg.str_data_bytes(); \
    str_busy_cycles     = r_msg.str_busy_cycles(); \
    str_stall_cycles    = r_msg.str_stall_cycles(); \
    str_starve_cycles   = r_msg.str_starve_cycles();


#define xclPerfMonReadCounters_Streaming_RETURN()

#define xclPerfMonReadCounters_Streaming_RPC_CALL(func_name, str_num_tranx, str_data_bytes, str_busy_cycles, \
                                        str_stall_cycles, str_starve_cycles, slotname) \
    RPC_PROLOGUE(func_name); \
    xclPerfMonReadCounters_Streaming_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclPerfMonReadCounters_Streaming_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclPerfMonReadCounters_Streaming_RETURN();

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

//----------xclPerfMonReadTrace(Streaming)------------
#define xclPerfMonReadTrace_Streaming_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
      RELEASE_MUTEX();\
      return 0; \
    }\
    c_msg.set_ack(ack); \
    c_msg.set_slotname(slotname);

#define xclPerfMonReadTrace_Streaming_SET_PROTO_RESPONSE() \
    samplessize = r_msg.output_data_size(); \

#define xclPerfMonReadTrace_Streaming_RPC_CALL(func_name,ack,samplessize,slotname) \
    RPC_PROLOGUE(func_name); \
    xclPerfMonReadTrace_Streaming_SET_PROTOMESSAGE(); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclPerfMonReadTrace_Streaming_SET_PROTO_RESPONSE(); \
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

//----------xclCopyBO-------------------
#define xclCopyBO_SET_PROTOMESSAGE(src_boHandle,filename,size,src_offset,dst_offset) \
    c_msg.set_src_handle(src_boHandle); \
    c_msg.set_dst_filename(filename); \
    c_msg.set_size(size); \
    c_msg.set_src_offset(src_offset); \
    c_msg.set_dst_offset(dst_offset);

#define xclCopyBO_SET_PROTO_RESPONSE() \
  ack = r_msg.ack();

#define xclCopyBO_RPC_CALL(func_name,src_boHandle,filename,size,src_offset,dst_offset) \
  RPC_PROLOGUE(func_name); \
  xclCopyBO_SET_PROTOMESSAGE(src_boHandle,filename,size,src_offset,dst_offset); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclCopyBO_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclImportBO-------------------
#define xclImportBO_SET_PROTOMESSAGE(filename,offset,size) \
    c_msg.set_dst_filename(filename); \
    c_msg.set_offset(offset); \
    c_msg.set_size(size);

#define xclImportBO_SET_PROTO_RESPONSE() \
  ack = r_msg.ack();

#define xclImportBO_RPC_CALL(func_name,filename,offset,size) \
  RPC_PROLOGUE(func_name); \
  xclImportBO_SET_PROTOMESSAGE(filename,offset,size); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclImportBO_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclCreateQueue-------------------
#define xclCreateQueue_SET_PROTOMESSAGE(q_ctx,bWrite) \
    c_msg.set_write(bWrite); \
    c_msg.set_type(q_ctx->type); \
    c_msg.set_state(q_ctx->state); \
    c_msg.set_route(q_ctx->route); \
    c_msg.set_flow(q_ctx->flow); \
    c_msg.set_qsize(q_ctx->qsize); \
    c_msg.set_desc_size(q_ctx->desc_size); \
    c_msg.set_flags(q_ctx->flags);

#define xclCreateQueue_SET_PROTO_RESPONSE() \
  q_handle = r_msg.q_handle();

#define xclCreateQueue_RPC_CALL(func_name, q_ctx,bWrite) \
  RPC_PROLOGUE(func_name); \
  xclCreateQueue_SET_PROTOMESSAGE(q_ctx, bWrite); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclCreateQueue_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclWriteQueue-------------------
#define xclWriteQueue_SET_PROTOMESSAGE(q_handle,src,size) \
    c_msg.set_q_handle(q_handle); \
    c_msg.set_src((char*)src,size); \
    c_msg.set_size(size); \
    c_msg.set_req(mReqCounter);\
    c_msg.set_nonblocking(nonBlocking);\
    c_msg.set_eot(eot);

#define xclWriteQueue_SET_PROTO_RESPONSE() \
  uint64_t written_size = r_msg.written_size();

#define xclWriteQueue_RPC_CALL(func_name,q_handle,src,size) \
  RPC_PROLOGUE(func_name); \
  xclWriteQueue_SET_PROTOMESSAGE(q_handle,src,size); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclWriteQueue_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclReadQueue-------------------
#define xclReadQueue_SET_PROTOMESSAGE(q_handle,dest,size) \
    c_msg.set_q_handle(q_handle); \
    c_msg.set_dest((char*)dest,size); \
    c_msg.set_size(size); \
    c_msg.set_req(mReqCounter);\
    c_msg.set_nonblocking(nonBlocking);\
    c_msg.set_eot(eot);

#define xclReadQueue_SET_PROTO_RESPONSE(dest) \
    read_size = r_msg.size();\
    memcpy(dest,r_msg.dest().c_str(),read_size);

#define xclReadQueue_RPC_CALL(func_name,q_handle,dest,size) \
  RPC_PROLOGUE(func_name); \
  xclReadQueue_SET_PROTOMESSAGE(q_handle,dest,size); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclReadQueue_SET_PROTO_RESPONSE(dest); \
  FREE_BUFFERS();

//----------xclPollCompletion-------------------
#define xclPollCompletion_SET_PROTOMESSAGE(reqcounter) \
    c_msg.set_req(reqcounter); \

#define xclPollCompletion_SET_PROTO_RESPONSE(vaLenMap) \
  std::map<uint64_t,uint64_t>::iterator vaLenMapItr = vaLenMap.begin();\
  if(r_msg.fullrequest_size() == (int)(vaLenMap.size()))\
  {\
    for(int i = 0; i < r_msg.fullrequest_size() ; i++) \
    { \
      const xclPollCompletion_response::request &oReq = r_msg.fullrequest(i); \
      uint64_t read_size = oReq.size();\
      numBytesProcessed  += read_size; \
      if((*vaLenMapItr).second != 0) \
        memcpy((void*)(*vaLenMapItr).first,oReq.dest().c_str(),read_size);\
      vaLenMapItr++;\
    } \
  }\

#define xclPollCompletion_RPC_CALL(func_name,reqcounter,vaLenMap) \
  RPC_PROLOGUE(func_name); \
  xclPollCompletion_SET_PROTOMESSAGE(reqcounter); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclPollCompletion_SET_PROTO_RESPONSE(vaLenMap); \
  FREE_BUFFERS();

//----------xclPollQueue-------------------
#define xclPollQueue_SET_PROTOMESSAGE(q_handle,reqcounter) \
    c_msg.set_q_handle(q_handle); \
    c_msg.set_req(reqcounter); \

#define xclPollQueue_SET_PROTO_RESPONSE(vaLenMap) \
  std::map<uint64_t,uint64_t>::iterator vaLenMapItr = vaLenMap.begin();\
  if(r_msg.fullrequest_size() == (int)(vaLenMap.size()))\
  {\
    for(int i = 0; i < r_msg.fullrequest_size() ; i++) \
    { \
      const xclPollQueue_response::request &oReq = r_msg.fullrequest(i); \
      uint64_t read_size = oReq.size();\
      numBytesProcessed  += read_size; \
      if((*vaLenMapItr).second != 0) \
        memcpy((void*)(*vaLenMapItr).first,oReq.dest().c_str(),read_size);\
      vaLenMapItr++;\
    } \
  }\

#define xclPollQueue_RPC_CALL(func_name,q_handle, reqcounter,vaLenMap) \
  RPC_PROLOGUE(func_name); \
  xclPollQueue_SET_PROTOMESSAGE(q_handle, reqcounter); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclPollQueue_SET_PROTO_RESPONSE(vaLenMap); \
  FREE_BUFFERS();

//----------xclSetQueueOpt-------------------
#define xclSetQueueOpt_SET_PROTOMESSAGE(q_handle,type,val) \
    c_msg.set_q_handle(q_handle); \
    c_msg.set_type(type); \
    c_msg.set_val(val);

#define xclSetQueueOpt_SET_PROTO_RESPONSE() \
  success = r_msg.success();

#define xclSetQueueOpt_RPC_CALL(func_name,q_handle,type,val) \
  RPC_PROLOGUE(func_name); \
  xclSetQueueOpt_SET_PROTOMESSAGE(q_handle,type,val); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclSetQueueOpt_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclDestroyQueue-------------------
#define xclDestroyQueue_SET_PROTOMESSAGE(q_handle) \
    c_msg.set_q_handle(q_handle);

#define xclDestroyQueue_SET_PROTO_RESPONSE() \
  success = r_msg.success();

#define xclDestroyQueue_RPC_CALL(func_name, q_handle) \
  RPC_PROLOGUE(func_name); \
  xclDestroyQueue_SET_PROTOMESSAGE(q_handle); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclDestroyQueue_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//----------xclSetupInstance-------------------
#define xclSetupInstance_SET_PROTOMESSAGE(route, argFlowIdMap) \
    c_msg.set_route(route); \
    for(auto& it: argFlowIdMap) \
    { \
      xclSetupInstance_call_argflowpair* afpair = c_msg.add_setup(); \
      afpair->set_arg(it.first); \
      afpair->set_flow((it.second).first);\
      afpair->set_tag((it.second).second);\
    }


#define xclSetupInstance_SET_PROTO_RESPONSE() \
  success = r_msg.success();

#define xclSetupInstance_RPC_CALL(func_name, route, argFlowIdMap) \
  RPC_PROLOGUE(func_name); \
  xclSetupInstance_SET_PROTOMESSAGE(route, argFlowIdMap); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclSetupInstance_SET_PROTO_RESPONSE(); \
  FREE_BUFFERS();

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-XRT Graph Api's-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-----------xclGraphInit-----------------
#define xclGraphInit_SET_PROTOMESSAGE(func_name,graphhandle,graphname) \
    c_msg.set_gh(graphhandle); \
    c_msg.set_graphname((char*)graphname);

#define xclGraphInit_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  
    
#define xclGraphInit_RETURN()\
    //return size;

#define xclGraphInit_RPC_CALL(func_name,graphhandle,graphname) \
    RPC_PROLOGUE(func_name); \
    xclGraphInit_SET_PROTOMESSAGE(func_name,graphhandle,graphname); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphInit_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclGraphInit_RETURN();

//-----------xclGraphRun-----------------
#define xclGraphRun_SET_PROTOMESSAGE(func_name,graphhandle,iterations) \
    c_msg.set_gh(graphhandle); \
    c_msg.set_iterations(iterations);

#define xclGraphRun_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGraphRun_RETURN()\
    //return size;

#define xclGraphRun_RPC_CALL(func_name,graphhandle,iterations) \
    RPC_PROLOGUE(func_name); \
    xclGraphRun_SET_PROTOMESSAGE(func_name,graphhandle,iterations); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphRun_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclGraphRun_RETURN();

//-----------xclGraphWait-----------------
#define xclGraphWait_SET_PROTOMESSAGE(func_name,graphhandle) \
    c_msg.set_gh(graphhandle);

#define xclGraphWait_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGraphWait_RETURN()\
    //return size;

#define xclGraphWait_RPC_CALL(func_name,graphhandle) \
    RPC_PROLOGUE(func_name); \
    xclGraphWait_SET_PROTOMESSAGE(func_name,graphhandle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphWait_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclGraphWait_RETURN();

//-----------xclGraphEnd-----------------
#define xclGraphEnd_SET_PROTOMESSAGE(func_name,graphhandle) \
    c_msg.set_gh(graphhandle);

#define xclGraphEnd_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGraphEnd_RETURN()\
    //return size;

#define xclGraphEnd_RPC_CALL(func_name,graphhandle) \
    RPC_PROLOGUE(func_name); \
    xclGraphEnd_SET_PROTOMESSAGE(func_name,graphhandle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphEnd_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclGraphEnd_RETURN();

//-----------xclGraphUpdateRTP-----------------
#define xclGraphUpdateRTP_SET_PROTOMESSAGE(func_name,graphhandle,portname,buffer,size) \
    c_msg.set_gh(graphhandle); \
    c_msg.set_portname((char*)portname); \
    c_msg.set_buffer((char*)buffer,size); \
    c_msg.set_size(size);

#define xclGraphUpdateRTP_SET_PROTO_RESPONSE() \
 //   uint64_t ret = r_msg.size();


#define xclGraphUpdateRTP_RETURN() \
   // return ret;

#define xclGraphUpdateRTP_RPC_CALL(func_name,graphhandle,portname,buffer,size) \
    RPC_PROLOGUE(func_name); \
    xclGraphUpdateRTP_SET_PROTOMESSAGE(func_name,graphhandle,portname,buffer,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphUpdateRTP_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclGraphUpdateRTP_RETURN();

//-----------xclGraphReadRTP-----------------
#define xclGraphReadRTP_SET_PROTOMESSAGE(func_name,graphhandle,portname,buffer,size) \
    c_msg.set_gh(graphhandle); \
    c_msg.set_portname((char*)portname); \
    c_msg.set_buffer((char*)buffer,size); \
    c_msg.set_size(size);

#define xclGraphReadRTP_SET_PROTO_RESPONSE(c_buffer) \
    uint64_t ret = r_msg.size();\
    memcpy(c_buffer,r_msg.buffer().c_str(),ret);


#define xclGraphReadRTP_RETURN() \
    //return ret;

#define xclGraphReadRTP_RPC_CALL(func_name,graphhandle,portname,buffer,size) \
    RPC_PROLOGUE(func_name); \
    xclGraphReadRTP_SET_PROTOMESSAGE(func_name,graphhandle,portname,buffer,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphReadRTP_SET_PROTO_RESPONSE(buffer); \
    FREE_BUFFERS(); \
    xclGraphReadRTP_RETURN();

//-----------xclSyncBOAIENB-----------------
#define xclSyncBOAIENB_SET_PROTOMESSAGE(func_name,gmioname,dir,bo,numbytes) \
    c_msg.set_gmioname((char*)gmioname); \
    c_msg.set_dir(dir); \
    c_msg.set_bo(bo); \
    c_msg.set_numbytes(numbytes);

#define xclSyncBOAIENB_SET_PROTO_RESPONSE() \
     ack = r_msg.ack(); 


#define xclSyncBOAIENB_RETURN() \
   // return ret;

#define xclSyncBOAIENB_RPC_CALL(func_name,gmioname,dir,bo,numbytes) \
    RPC_PROLOGUE(func_name); \
    xclSyncBOAIENB_SET_PROTOMESSAGE(func_name,gmioname,dir,bo,numbytes); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclSyncBOAIENB_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclSyncBOAIENB_RETURN();

//-----------xclGMIOWait-----------------
#define xclGMIOWait_SET_PROTOMESSAGE(func_name,gmioname) \
    c_msg.set_gmioname((char*)gmioname);

#define xclGMIOWait_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGMIOWait_RETURN()\
    //return size;

#define xclGMIOWait_RPC_CALL(func_name,gmioname) \
    RPC_PROLOGUE(func_name); \
    xclGraphEnd_SET_PROTOMESSAGE(func_name,gmioname); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphEnd_SET_PROTO_RESPONSE(); \
    FREE_BUFFERS(); \
    xclGraphEnd_RETURN();
