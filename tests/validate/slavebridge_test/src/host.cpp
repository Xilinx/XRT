/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include <math.h>
#include <sys/time.h>
#include <xcl2.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>                                        
#include <boost/property_tree/json_parser.hpp>

#define TYPESIZE 512
const double typesize = TYPESIZE;


double getMicroTime() {
  struct timeval currentTime;
  gettimeofday(&currentTime, nullptr);
  return currentTime.tv_sec * 1000000 + currentTime.tv_usec;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <Platform Test Area Path>"<< std::endl;
    return EXIT_FAILURE;
  }

  int NUM_KERNEL;
  bool file_found = false;  
  std::string test_path = argv[1];
  std::string filename = "/platform.json";
  std::string platform_json = test_path+filename;

  try{
      boost::property_tree::ptree loadPtreeRoot;                                                    
      boost::property_tree::read_json(platform_json, loadPtreeRoot);
      boost::property_tree::ptree temp ;                                                            
  
      temp = loadPtreeRoot.get_child("total_banks");
      NUM_KERNEL =  temp.get_value<int>();  

      boost::filesystem::path p(test_path);
      for (auto i = boost::filesystem::directory_iterator(p); i != boost::filesystem::directory_iterator(); i++)
       {
            if (!is_directory(i->path())) //we eliminate directories
            {
                if(i->path().filename().string() == "slavebridge.xclbin")
                    file_found = true;
            }
        }

  } catch (const boost::filesystem::filesystem_error & e) {
      std::cout << "Exception!!!! " << e.what();
  } catch (const std::exception & e) {
      std::string msg("ERROR: Bad JSON format detected while marshaling build metadata (");
      msg += e.what();
      msg += ").";
      std::cout << msg;
    }
  if(!file_found){
      std::cout << "\nNOT SUPPORTED" << std::endl;
      return EOPNOTSUPP; 
  }

  double DATA_SIZE = 1024 * 1024 * 16; // 16 MB
  std::string b_file = "/slavebridge.xclbin";
  std::string binaryFile = test_path+b_file;
  size_t vector_size_bytes = sizeof(char) * DATA_SIZE;
  cl_int err;
  cl::Context context;
  std::string krnl_name = "slavebridge";
  std::vector<cl::Kernel> krnls(NUM_KERNEL);
  cl::CommandQueue q;

  // OPENCL HOST CODE AREA START
  // get_xil_devices() is a utility API which will find the xilinx
  // platforms and will return list of devices connected to Xilinx platform
  auto devices = xcl::get_xil_devices();
  // read_binary_file() is a utility API which will load the binaryFile
  // and will return the pointer to file buffer.
  auto fileBuf = xcl::read_binary_file(binaryFile);
  cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
  bool valid_device = false;
  for (unsigned int i = 0; i < devices.size(); i++) {
    auto device = devices[i];
    // Creating Context and Command Queue for selected Device
    OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
    OCL_CHECK(err,
              q = cl::CommandQueue(context, device,
                                   CL_QUEUE_PROFILING_ENABLE |
                                       CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                   &err));
    std::cout << "Trying to program device[" << i
              << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
      std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
    } else {
      std::cout << "Device[" << i << "]: program successful!\n";
      for (int i = 0; i < NUM_KERNEL; i++) {
        std::string cu_id = std::to_string(i + 1);
        std::string krnl_name_full =
            krnl_name + ":{" + "slavebridge_" + cu_id + "}";

        printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(),
               i + 1);

        // Here Kernel object is created by specifying kernel name along with
        // compute unit.
        // For such case, this kernel object can only access the specific
        // Compute unit

        OCL_CHECK(err,
                  krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
      }

      valid_device = true;
      break; // we break because we found a valid device
    }
  }
  if (!valid_device) {
    std::cout << "Failed to program any device found, exit!\n";
    exit(EXIT_FAILURE);
  }

  std::vector<unsigned char, aligned_allocator<unsigned char>> input_host(DATA_SIZE);

  for (uint32_t i = 0; i < DATA_SIZE; i++) {
    input_host[i] = i % 256;
  }

  std::vector<cl::Buffer> input_buffer(NUM_KERNEL);
  std::vector<cl::Buffer> output_buffer(NUM_KERNEL);

  std::vector<cl_mem_ext_ptr_t> input_buffer_ext(NUM_KERNEL);
  std::vector<cl_mem_ext_ptr_t> output_buffer_ext(NUM_KERNEL);
  for (int i = 0; i < NUM_KERNEL; i++) {
    input_buffer_ext[i].flags = XCL_MEM_EXT_HOST_ONLY;
    input_buffer_ext[i].obj = nullptr;
    input_buffer_ext[i].param = 0;

    output_buffer_ext[i].flags = XCL_MEM_EXT_HOST_ONLY;
    output_buffer_ext[i].obj = nullptr;
    output_buffer_ext[i].param = 0;
  }

  for (int i = 0; i < NUM_KERNEL; i++) {
    OCL_CHECK(err, input_buffer[i] = cl::Buffer(
                       context, CL_MEM_READ_WRITE|
                                    CL_MEM_EXT_PTR_XILINX,
                       vector_size_bytes, &input_buffer_ext[i], &err));
    OCL_CHECK(err, output_buffer[i] = cl::Buffer(
                       context, CL_MEM_READ_WRITE |
                                    CL_MEM_EXT_PTR_XILINX,
                       vector_size_bytes, &output_buffer_ext[i], &err));
  }

  unsigned char *map_input_buffer[NUM_KERNEL];
  unsigned char *map_output_buffer[NUM_KERNEL];
  for(int i = 0; i < NUM_KERNEL; i++){  
  OCL_CHECK(err, map_input_buffer[i] = (unsigned char *)q.enqueueMapBuffer(
                     (input_buffer[i]), CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION,
                     0, vector_size_bytes, nullptr, nullptr, &err));
  OCL_CHECK(err, err = q.finish());
  }

    /* prepare data to be written to the device */
  for(int j=0; j < NUM_KERNEL; j++){
    for (size_t i = 0; i < vector_size_bytes; i++) {
      map_input_buffer[j][i] = input_host[i];
    }
  }
  double globalbuffersizeinbeats = DATA_SIZE / (typesize / 8);
  uint32_t tests = (uint32_t)log2(globalbuffersizeinbeats) + 1;

  double dnsduration[tests];
  double dsduration[tests];
  double dbytes[tests];
  double bpersec[tests];
  double mbpersec[tests];

  // run tests with burst length 1 beat to globalbuffersize
  // double burst length each test
  uint32_t beats;
  uint32_t test = 0;

  float throughput[7] = {0, 0, 0, 0, 0, 0, 0};
  for (beats = 16; beats <= 1024; beats = beats * 4) {
    std::cout << "Loop PIPELINE " << beats << " beats\n";

    double usduration;
    double fiveseconds = 5 * 1000000;
    unsigned reps = 64;
    do {
      for (int i = 0; i < NUM_KERNEL; i++) {
        OCL_CHECK(err, err = krnls[i].setArg(0, output_buffer[i]));
        OCL_CHECK(err, err = krnls[i].setArg(1, input_buffer[i]));
        OCL_CHECK(err, err = krnls[i].setArg(2, beats));
        OCL_CHECK(err, err = krnls[i].setArg(3, reps));
      }

      double start, end;
      start = getMicroTime();
      for (int i = 0; i < NUM_KERNEL; i++) {
        OCL_CHECK(err, err = q.enqueueTask(krnls[i]));
      }
      q.finish();
      end = getMicroTime();
      
      for(int i=0; i<NUM_KERNEL; i++){
      OCL_CHECK(err, map_output_buffer[i] = (unsigned char *)q.enqueueMapBuffer(
                   (output_buffer[i]), CL_FALSE, CL_MAP_READ, 0, vector_size_bytes, nullptr,
                    nullptr, &err));
      OCL_CHECK(err, err = q.finish());
      }
      // check
      for (int i = 0; i < NUM_KERNEL; i++) {
        for (uint32_t j = 0; j < beats * (typesize / 8); j++) {
          if (map_output_buffer[i][j] != input_host[j]) {
            printf(
                "ERROR : kernel failed to copy entry %i input %i output %i\n",
                j, input_host[j], map_output_buffer[i][j]);
            return EXIT_FAILURE;
          }
        }
      }

      usduration = end - start;
      dnsduration[test] = ((double)usduration);
      dsduration[test] = dnsduration[test] / ((double)1000000);

      if (usduration < fiveseconds)
        reps = reps * 2;

    } while (usduration < fiveseconds);

    dnsduration[test] = ((double)usduration);
    dsduration[test] = dnsduration[test] / ((double)1000000);
    dbytes[test] = reps * beats * (typesize / 8);
    bpersec[test] = (NUM_KERNEL * dbytes[test]) / dsduration[test];
    mbpersec[test] = 2 * bpersec[test] /
                     ((double)1024 * 1024); // for concurrent READ and WRITE

    throughput[test] = mbpersec[test];
    printf("Test : %d, Throughput: %f MB/s\n", test, throughput[test]);
    test++;
  }

  float max_V;
  int ii;
  max_V = throughput[0];
  printf("TTTT : %f\n", throughput[0]);
  int count = 0;
  for (ii = 1; ii < 7; ii++) {
    if (max_V < throughput[ii]) {
      count++;
      max_V = throughput[ii];
    }
  }
  if (count == 0) {
    max_V = throughput[0];
  }
  printf("Maximum throughput: %f MB/s\n", max_V);

  printf("TEST PASSED\n");

  return EXIT_SUCCESS;
}
