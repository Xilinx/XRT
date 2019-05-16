// Copyright (C) 2018 Xilinx Inc.
// All rights reserved.

#include <CL/cl_ext_xilinx.h>   // to suppress deprecation warnings
#include <CL/cl.h>
#include <fstream>
#include <stdexcept>
#include <numeric>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;

static void
throw_if_error(cl_int errcode, const char* msg=nullptr)
{
  if (!errcode)
    return;
  std::string err = "errcode '";
  err.append(std::to_string(errcode)).append("'");
  if (msg)
    err.append(" ").append(msg);
  throw std::runtime_error(err);
}

int
run_cu(cl_context context, cl_command_queue queue, cl_kernel kernel)
{
  using data_type = unsigned long;
  const size_t size = ELEMENTS * ARRAY_SIZE;
  const size_t bytes = sizeof(data_type) * size;

  auto a = clCreateBuffer(context,CL_MEM_READ_ONLY,bytes,nullptr,nullptr);
  auto b = clCreateBuffer(context,CL_MEM_WRITE_ONLY,bytes,nullptr,nullptr);
  throw_if_error(a==nullptr,"failed to create buffer for a");
  throw_if_error(b==nullptr,"failed to create buffer for b");

  // set kernel args prior to enqueue operation such that proper
  // device side allocation can be determined
  throw_if_error(clSetKernelArg(kernel,0,sizeof(cl_mem),&a),"failed to set kernel arg 0");
  throw_if_error(clSetKernelArg(kernel,1,sizeof(cl_mem),&b),"failed to set kernel arg 1");
  throw_if_error(clSetKernelArg(kernel,2,sizeof(int),&ELEMENTS),"failed to set kernel arg 2");

  cl_int err = CL_SUCCESS;
  auto a_data = (data_type*) clEnqueueMapBuffer(queue,a,true,CL_MAP_WRITE,0,bytes,0,nullptr,nullptr,&err);
  throw_if_error(err,"failed to map buffer a");
  throw_if_error(clEnqueueUnmapMemObject(queue,a,a_data,0,nullptr,nullptr));
  auto b_data = (data_type*) clEnqueueMapBuffer(queue,b,true,CL_MAP_READ,0,bytes,0,nullptr,nullptr,&err);
  throw_if_error(err,"failed to map buffer b");
  throw_if_error(clEnqueueUnmapMemObject(queue,b,b_data,0,nullptr,nullptr));

  std::iota(a_data,a_data+size,0);

  cl_event migrate_event = nullptr;
  cl_mem mems[2] = {a,b};
  throw_if_error(clEnqueueMigrateMemObjects(queue,2,mems,0,0,nullptr,&migrate_event));

  cl_event ndrange_event = nullptr;
  clEnqueueTask(queue,kernel,1,&migrate_event,&ndrange_event);
  clReleaseEvent(migrate_event);

  throw_if_error(clEnqueueMigrateMemObjects(queue,1,mems+1,CL_MIGRATE_MEM_OBJECT_HOST,1,&ndrange_event,&migrate_event));
  clReleaseEvent(ndrange_event);

  clWaitForEvents(1,&migrate_event);
  clReleaseEvent(migrate_event);

  // verify
  for (size_t idx=0; idx<size; ++idx) {
    auto expect = a_data[idx] + (idx%8 ? 0 : 1);
    if (b_data[idx] != expect) {
      std::cout << "b_data[" << idx << "] = " << b_data[idx]
                << " expected " << expect << "\n";
      err=1;
    }
  }
  throw_if_error(err,"Results did not match");

  clReleaseMemObject(a);
  clReleaseMemObject(b);

  return 0;
}

int
run_kernel(cl_device_id device,cl_command_queue queue,cl_program program)
{
  // Create sub devices
  cl_uint num_devices = 0;
  cl_device_partition_property props[3] = {CL_DEVICE_PARTITION_EQUALLY,1,0};
  throw_if_error(clCreateSubDevices(device,props,0,nullptr,&num_devices));
  std::vector<cl_device_id> devices(num_devices);
  throw_if_error(clCreateSubDevices(device,props,num_devices,devices.data(),nullptr));

  std::for_each(devices.begin(),devices.end(),[program](cl_device_id sdev) {
      cl_int err = CL_SUCCESS;
      auto context = clCreateContext(0,1,&sdev,nullptr,nullptr,&err);
      throw_if_error(err,"failed to create context from sub device");
      auto queue = clCreateCommandQueue(context,sdev,CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,&err);
      throw_if_error(err,"failed to create command queue from context");
      auto kernel = clCreateKernel(program,"addone",&err);
      throw_if_error(err,"failed to create kernel from program");
      run_cu(context,queue,kernel);
      throw_if_error(clReleaseKernel(kernel));
      throw_if_error(clReleaseCommandQueue(queue));
      throw_if_error(clReleaseContext(context));
      throw_if_error(clReleaseDevice(sdev));
    });

  return 0;
}

int
run(int argc, char** argv)
{
  if (argc < 2)
    throw std::runtime_error("usage: host.exe <xclbin>");

  // Init OCL
  cl_int err = CL_SUCCESS;
  cl_platform_id platform = nullptr;
  throw_if_error(clGetPlatformIDs(1,&platform,nullptr));

  cl_uint num_devices = 0;
  throw_if_error(clGetDeviceIDs(platform,CL_DEVICE_TYPE_ACCELERATOR,0,nullptr,&num_devices));
  throw_if_error(num_devices==0,"no devices");
  std::vector<cl_device_id> devices(num_devices);
  throw_if_error(clGetDeviceIDs(platform,CL_DEVICE_TYPE_ACCELERATOR,num_devices,devices.data(),nullptr));
  cl_device_id device = devices.front();

  cl_context context = clCreateContext(0,1,&device,nullptr,nullptr,&err);
  throw_if_error(err);

  cl_command_queue queue = clCreateCommandQueue(context,device,0,&err);
  throw_if_error(err,"failed to create command queue");

  // Read xclbin and create program
  std::string fnm = argv[1];
  std::ifstream stream(fnm);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);
  std::vector<char> xclbin(size);
  stream.read(xclbin.data(),size);
  const unsigned char* data = reinterpret_cast<unsigned char*>(xclbin.data());
  cl_int status = CL_SUCCESS;
  cl_program program = clCreateProgramWithBinary(context,1,&device,&size,&data,&status,&err);
  throw_if_error(err,"failed to create program");

  run_kernel(device,queue,program);

  clReleaseProgram(program);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  clReleaseDevice(device);
  std::for_each(devices.begin(),devices.end(),[](cl_device_id d){clReleaseDevice(d);});

  return 0;
}

int
main(int argc, char* argv[])
{
  try {
    run(argc,argv);
    std::cout << "TEST SUCCESS\n";
    return 0;
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << "\n";
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }

  return 1;
}
