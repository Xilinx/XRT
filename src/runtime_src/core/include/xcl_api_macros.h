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

#define SCOPE_GUARD_MUTEX() \
if ( sock->server_started == false ) { if (mLogStream.is_open()) mLogStream << __func__ << "\n socket communication is not possible now!"; exit(0);  } \
std::lock_guard<std::mutex> socketlk{mtx}; 

#define RPC_PROLOGUE(func_name) \
    auto _s_inst = sock;  \
    func_name##_call c_msg; \
    func_name##_response r_msg; \
    SCOPE_GUARD_MUTEX()

#if GOOGLE_PROTOBUF_VERSION < 3006001
// Use the deprecated 32 bit version of the size
#define SERIALIZE_AND_SEND_MSG(func_name)                               \
    auto c_len = c_msg.ByteSize();                                      \
    buf_size = alloc_void(c_len);                                       \
    auto socket_call_status = -1;                                       \
    bool rv = c_msg.SerializeToArray(buf,c_len);                        \
    if (rv == false) { std::cerr << "FATAL ERROR:protobuf SerializeToArray failed for alloc_void call." << std::endl; exit(1);} \
                                                                        \
    ci_msg.set_size(c_len);                                             \
    ci_msg.set_xcl_api(func_name##_n);                                  \
    auto ci_len = ci_msg.ByteSize();                                    \
    rv = ci_msg.SerializeToArray(ci_buf,ci_len);                        \
    if (rv == false) { std::cerr <<"FATAL ERROR:protobuf SerializeToArray failed." << std::endl; exit(1); } \
                                                                        \
    _s_inst->sk_write(ci_buf,ci_len);                                   \
    _s_inst->sk_write(buf,c_len);                                       \
                                                                        \
    socket_call_status = _s_inst->sk_read(ri_buf,ri_msg.ByteSize());    \
    if (socket_call_status != -1) { rv = ri_msg.ParseFromArray(ri_buf,ri_msg.ByteSize()); }              \
    if (true != rv) { if (mLogStream.is_open()) mLogStream << __func__ << "\n ParseFromArray failed, sk_read/sk_write failed, so exit the application now!"; exit(0);  } \
    buf_size = alloc_void(ri_msg.size());                               \
    socket_call_status = _s_inst->sk_read(buf,ri_msg.size());           \
    if (socket_call_status != -1) { rv = r_msg.ParseFromArray(buf,ri_msg.size()); } \
    if (true != rv) { if (mLogStream.is_open()) mLogStream << __func__ << "\n ParseFromArray failed, sk_read failed for alloc_void, so exit- the application now!!!"; exit(0); }
#else
// More recent protoc handles 64 bit size objects and the 32 bit version is deprecated
#define SERIALIZE_AND_SEND_MSG(func_name)                               \
    auto c_len = c_msg.ByteSizeLong();                                  \
    buf_size = alloc_void(c_len);                                       \
    bool rv = c_msg.SerializeToArray(buf,c_len);                        \
    if (rv == false) { std::cerr << "FATAL ERROR:protobuf SerializeToArray failed." << std::endl; exit(1); } \
                                                                        \
    ci_msg.set_size(c_len);                                             \
    ci_msg.set_xcl_api(func_name##_n);                                  \
    auto ci_len = ci_msg.ByteSizeLong();                                \
    rv = ci_msg.SerializeToArray(ci_buf,ci_len);                        \
    if (rv == false) { std::cerr << "FATAL ERROR:protobuf SerializeToArray failed." << std::endl; exit(1); } \
                                                                        \
    _s_inst->sk_write(ci_buf,ci_len);                                   \
    _s_inst->sk_write(buf,c_len);                                       \
                                                                        \
    _s_inst->sk_read(ri_buf,ri_msg.ByteSizeLong());                     \
    rv = ri_msg.ParseFromArray(ri_buf,ri_msg.ByteSizeLong());           \
    assert(true == rv);                                                 \
    buf_size = alloc_void(ri_msg.size());                               \
    _s_inst->sk_read(buf,ri_msg.size());                                \
    rv = r_msg.ParseFromArray(buf,ri_msg.size());                       \
    assert(true == rv);
#endif

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
    xclFreeDeviceBuffer_RETURN();
//------------------------------------------------------------
//--------------------xclWriteAddrSpaceDeviceRam--------------------------------
//Generate call and info message
#define xclWriteAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,addr,data,size,pf_id,bar_id) \
    c_msg.set_addr(addr); \
    c_msg.set_data((char*) data, size); \
    c_msg.set_size(size); \
    c_msg.set_pf_id(pf_id); \
    c_msg.set_bar_id(bar_id);


#define xclWriteAddrSpaceDeviceRam_SET_PROTO_RESPONSE() \
    if (!r_msg.valid()) \
      size = -1;


#define xclWriteAddrSpaceDeviceRam_RETURN()\
    //return size;

#define xclWriteAddrSpaceDeviceRam_RPC_CALL(func_name,address_space,address,data,size,pf_id,bar_id) \
    RPC_PROLOGUE(func_name); \
    xclWriteAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,address,data,size,pf_id,bar_id); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclWriteAddrSpaceDeviceRam_SET_PROTO_RESPONSE(); \
    xclWriteAddrSpaceDeviceRam_RETURN();

//--------------------xclWriteAddrKernelCtrl--------------------------------
//Generate call and info message
#define xclWriteAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,addr,data,size,kernelArgsInfo,pf_id,bar_id) \
    c_msg.set_addr(addr); \
    c_msg.set_data((char*) data, size); \
    c_msg.set_size(size); \
    for (auto i : kernelArgsInfo) { \
    	xclWriteAddrKernelCtrl_call_kernelInfo* kernelInfo = c_msg.add_kernel_info();\
    	kernelInfo->set_addr(i.first);\
    	kernelInfo->set_size(i.second.second);\
    	kernelInfo->set_name(i.second.first);\
    }\
    c_msg.set_pf_id(pf_id); \
    c_msg.set_bar_id(bar_id);


#define xclWriteAddrKernelCtrl_SET_PROTO_RESPONSE() \
    if (!r_msg.valid()) \
      size = -1;


#define xclWriteAddrKernelCtrl_RETURN()\
    //return size;

#define xclWriteAddrKernelCtrl_RPC_CALL(func_name,address_space,address,data,size,kernelArgsInfo,pf_id,bar_id) \
    RPC_PROLOGUE(func_name); \
    xclWriteAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,address,data,size,kernelArgsInfo,pf_id,bar_id); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclWriteAddrKernelCtrl_SET_PROTO_RESPONSE(); \
    xclWriteAddrKernelCtrl_RETURN();

//--------------------xclRegWrite--------------------------------
//Generate call and info message
#define xclRegWrite_SET_PROTOMESSAGE(func_name,baseaddress,offset,data,pf_id,bar_id) \
    c_msg.set_baseaddress(baseaddress); \
    c_msg.set_offset(offset); \
    c_msg.set_data((char*) data, 4); \
    c_msg.set_pf_id(pf_id); \
    c_msg.set_bar_id(bar_id);


#define xclRegWrite_SET_PROTO_RESPONSE() \

#define xclRegWrite_RETURN()\
    //return size;

#define xclRegWrite_RPC_CALL(func_name,baseaddress,offset,data,pf_id,bar_id) \
    RPC_PROLOGUE(func_name); \
    xclRegWrite_SET_PROTOMESSAGE(func_name,baseaddress,offset,data,pf_id,bar_id); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclRegWrite_SET_PROTO_RESPONSE(); \
    xclRegWrite_RETURN();

//-----------------------xclReadAddrSpaceDeviceRam----------------------------
//Generate call and info message
#define xclReadAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,addr,data,size,pf_id,bar_id) \
    c_msg.set_addr(addr); \
    c_msg.set_size(size); \
    c_msg.set_pf_id(pf_id); \
    c_msg.set_bar_id(bar_id);


#define xclReadAddrSpaceDeviceRam_SET_PROTO_RESPONSE(datax,size) \
    if (!r_msg.valid()) \
      size = -1; \
    else { \
      memcpy(datax,r_msg.data().c_str(),size);\
    }


#define xclReadAddrSpaceDeviceRam_RETURN()\
  //  return size;

#define xclReadAddrSpaceDeviceRam_RPC_CALL(func_name,address_space,address,data,size,pf_id,bar_id) \
    RPC_PROLOGUE(func_name); \
    xclReadAddrSpaceDeviceRam_SET_PROTOMESSAGE(func_name,address_space,address,data,size,pf_id,bar_id); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclReadAddrSpaceDeviceRam_SET_PROTO_RESPONSE(data,size); \
    xclReadAddrSpaceDeviceRam_RETURN();
//-----------------------xclReadAddrKernelCtrl----------------------------
//Generate call and info message
#define xclReadAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,addr,data,size,pf_id,bar_id) \
    c_msg.set_addr(addr); \
    c_msg.set_size(size); \
    c_msg.set_pf_id(pf_id); \
    c_msg.set_bar_id(bar_id);


#define xclReadAddrKernelCtrl_SET_PROTO_RESPONSE(datax,size) \
    if (!r_msg.valid()) \
      size = -1; \
    else { \
      memcpy(datax,r_msg.data().c_str(),size);\
    }


#define xclReadAddrKernelCtrl_RETURN()\
  //  return size;

#define xclReadAddrKernelCtrl_RPC_CALL(func_name,address_space,address,data,size,pf_id,bar_id) \
    RPC_PROLOGUE(func_name); \
    xclReadAddrKernelCtrl_SET_PROTOMESSAGE(func_name,address_space,address,data,size,pf_id,bar_id); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclReadAddrKernelCtrl_SET_PROTO_RESPONSE(data,size); \
    xclReadAddrKernelCtrl_RETURN();
//-----------------------xclRegRead----------------------------
#define xclRegRead_SET_PROTOMESSAGE(func_name,baseaddress,offset,data,size,pf_id,bar_id) \
    c_msg.set_baseaddress(baseaddress); \
    c_msg.set_offset(offset); \
    c_msg.set_size(size); \
    c_msg.set_pf_id(pf_id); \
    c_msg.set_bar_id(bar_id);

#define xclRegRead_SET_PROTO_RESPONSE(datax,size) \
    if (!r_msg.valid()) \
      size = -1; \
    else { \
      memcpy(datax,r_msg.data().c_str(),size);\
    }

#define xclRegRead_RETURN()\

#define xclRegRead_RPC_CALL(func_name,baseaddress,offset,data,size,pf_id,bar_id) \
    RPC_PROLOGUE(func_name); \
    xclRegRead_SET_PROTOMESSAGE(func_name,baseaddress,offset,data,size,pf_id,bar_id); \
    SERIALIZE_AND_SEND_MSG(func_name) \
    xclRegRead_SET_PROTO_RESPONSE(data,size); \
    xclRegRead_RETURN();

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
    xclCopyBufferDevice2Host_RETURN();


//----------xclPerfMonReadCounters------------
//----------xclPerfMonReadCounters------------
#define xclPerfMonReadCounters_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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
    xclPerfMonReadCounters_RETURN();

//----------xclPerfMonReadCounters(Streaming)------------
#define xclPerfMonReadCounters_Streaming_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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
    xclPerfMonReadCounters_Streaming_RETURN();

//----------xclPerfMonGetTraceCount------------
#define xclPerfMonGetTraceCount_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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

//----------xclPerfMonReadTrace------------
#define xclPerfMonReadTrace_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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

//----------xclPerfMonReadTrace(Streaming)------------
#define xclPerfMonReadTrace_Streaming_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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

//----------xclWriteHostEvent------------
#define xclWriteHostEvent_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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

//----------xclGetDeviceTimestamp------------
#define xclGetDeviceTimestamp_SET_PROTOMESSAGE() \
  if(simulator_started == false) \
    {\
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

//----------xclReadBusStatus-------------------
#define xclReadBusStatus_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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

//----------xclGetDebugMessages-------------------
#define xclGetDebugMessages_SET_PROTOMESSAGE() \
    if(simulator_started == false) \
    {\
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

//----------xclCopyBOFromFd-------------------
#define xclCopyBOFromFd_SET_PROTOMESSAGE(filename,dest_boHandle,size,src_offset,dst_offset) \
  c_msg.set_dst_handle(dest_boHandle); \
  c_msg.set_src_filename(filename); \
  c_msg.set_size(size); \
  c_msg.set_src_offset(src_offset); \
  c_msg.set_dst_offset(dst_offset);

#define xclCopyBOFromFd_SET_PROTO_RESPONSE() \
  ack = r_msg.ack();

#define xclCopyBOFromFd_RPC_CALL(func_name,filename,dest_boHandle,size,src_offset,dst_offset) \
  RPC_PROLOGUE(func_name); \
  xclCopyBOFromFd_SET_PROTOMESSAGE(filename, dest_boHandle, size, src_offset, dst_offset); \
  SERIALIZE_AND_SEND_MSG(func_name) \
  xclCopyBOFromFd_SET_PROTO_RESPONSE(); \

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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphInit_SET_PROTOMESSAGE(func_name,graphhandle,graphname); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphInit_SET_PROTO_RESPONSE(); \
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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphRun_SET_PROTOMESSAGE(func_name,graphhandle,iterations); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphRun_SET_PROTO_RESPONSE(); \
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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphWait_SET_PROTOMESSAGE(func_name,graphhandle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphWait_SET_PROTO_RESPONSE(); \
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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphEnd_SET_PROTOMESSAGE(func_name,graphhandle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphEnd_SET_PROTO_RESPONSE(); \
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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphUpdateRTP_SET_PROTOMESSAGE(func_name,graphhandle,portname,buffer,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphUpdateRTP_SET_PROTO_RESPONSE(); \
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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphReadRTP_SET_PROTOMESSAGE(func_name,graphhandle,portname,buffer,size); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphReadRTP_SET_PROTO_RESPONSE(buffer); \
    xclGraphReadRTP_RETURN();

//-----------xclSyncBOAIENB-----------------
#define xclSyncBOAIENB_SET_PROTOMESSAGE(func_name,gmioname,dir,size,offset,boh) \
    c_msg.set_gmioname((char*)gmioname); \
    c_msg.set_dir(dir); \
    c_msg.set_size(size); \
    c_msg.set_offset(offset); \
    c_msg.set_boh(boh);

#define xclSyncBOAIENB_SET_PROTO_RESPONSE() \
     ack = r_msg.ack(); 


#define xclSyncBOAIENB_RETURN() \
   // return ret;

#define xclSyncBOAIENB_RPC_CALL(func_name,gmioname,dir,size,offset,boh) \
    RPC_PROLOGUE(func_name); \
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclSyncBOAIENB_SET_PROTOMESSAGE(func_name,gmioname,dir,size,offset,boh); \
    SERIALIZE_AND_SEND_MSG(func_name); \
    xclSyncBOAIENB_SET_PROTO_RESPONSE(); \
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
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGMIOWait_SET_PROTOMESSAGE(func_name,gmioname); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGMIOWait_SET_PROTO_RESPONSE(); \
    xclGMIOWait_RETURN();

//-----------xclGraphTimedWait-----------------
#define xclGraphTimedWait_SET_PROTOMESSAGE(func_name,graphhandle,cycle) \
    c_msg.set_gh(graphhandle); \
    c_msg.set_cycletimeout(cycle);

#define xclGraphTimedWait_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGraphTimedWait_RETURN()\
    //return size;

#define xclGraphTimedWait_RPC_CALL(func_name,graphhandle,cycle) \
    RPC_PROLOGUE(func_name); \
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphTimedWait_SET_PROTOMESSAGE(func_name,graphhandle,cycle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphTimedWait_SET_PROTO_RESPONSE(); \
    xclGraphTimedWait_RETURN();

//-----------xclGraphTimedEnd-----------------
#define xclGraphTimedEnd_SET_PROTOMESSAGE(func_name,graphhandle,cycle) \
    c_msg.set_gh(graphhandle); \
    c_msg.set_cycletimeout(cycle);

#define xclGraphTimedEnd_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGraphTimedEnd_RETURN()\
    //return size;

#define xclGraphTimedEnd_RPC_CALL(func_name,graphhandle,cycle) \
    RPC_PROLOGUE(func_name); \
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphTimedEnd_SET_PROTOMESSAGE(func_name,graphhandle,cycle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphTimedEnd_SET_PROTO_RESPONSE(); \
    xclGraphTimedEnd_RETURN();

//-----------xclGraphResume-----------------
#define xclGraphResume_SET_PROTOMESSAGE(func_name,graphhandle) \
    c_msg.set_gh(graphhandle);

#define xclGraphResume_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclGraphResume_RETURN()\
    //return size;

#define xclGraphResume_RPC_CALL(func_name,graphhandle) \
    RPC_PROLOGUE(func_name); \
    if(aiesim_sock != nullptr) { _s_inst = aiesim_sock; } \
    xclGraphResume_SET_PROTOMESSAGE(func_name,graphhandle); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclGraphResume_SET_PROTO_RESPONSE(); \
    xclGraphResume_RETURN();

//-----------xclLoadXclbinContent-----------------
#define xclLoadXclbinContent_SET_PROTOMESSAGE(func_name,xmlbuff,xmlbuffsize,sharedbin,sharedbinsize,emuldata,emuldatasize,keepdir) \
    c_msg.set_xmlbuff((char*)xmlbuff,xmlbuffsize); \
    c_msg.set_xmlbuffsize(xmlbuffsize); \
    c_msg.set_sharedbin((char*)sharedbin,sharedbinsize); \
    c_msg.set_sharedbinsize(sharedbinsize); \
    c_msg.set_emuldata((char*)emuldata,emuldatasize); \
    c_msg.set_emuldatasize(emuldatasize); \
    c_msg.set_keepdir(keepdir);

#define xclLoadXclbinContent_SET_PROTO_RESPONSE() \
    ack = r_msg.ack();  

#define xclLoadXclbinContent_RETURN()\
    //return size;

#define xclLoadXclbinContent_RPC_CALL(func_name,xmlbuff,xmlbuffsize,sharedbin,sharedbinsize,emuldata,emuldatasize,keepdir) \
    RPC_PROLOGUE(func_name); \
    xclLoadXclbinContent_SET_PROTOMESSAGE(func_name,xmlbuff,xmlbuffsize,sharedbin,sharedbinsize,emuldata,emuldatasize,keepdir); \
    SERIALIZE_AND_SEND_MSG(func_name)\
    xclLoadXclbinContent_SET_PROTO_RESPONSE(); \
    xclLoadXclbinContent_RETURN();

#define swemuDriverVersion_SET_PROTOMESSAGE(version) \
  c_msg.set_version(version);

#define swemuDriverVersion_SET_PROTO_RESPONSE() \
  success = r_msg.success();

#define swemuDriverVersion_RPC_CALL(func_name, version) \
  RPC_PROLOGUE(func_name);                              \
  swemuDriverVersion_SET_PROTOMESSAGE(version);         \
  SERIALIZE_AND_SEND_MSG(func_name)                     \
  swemuDriverVersion_SET_PROTO_RESPONSE();              

