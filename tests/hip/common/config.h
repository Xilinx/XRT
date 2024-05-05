// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "buffer_ops.h"
#include "patch_DDR_address.h"

#include <string>
#include <assert.h>
#include <sstream>
#include <fstream>
#include <tuple>

uint32_t IFM_DIRTY_BYTES;
uint32_t IFM_SIZE;
uint32_t PARAM_SIZE;
uint32_t OFM_SIZE;
uint32_t INTER_SIZE;
uint32_t MC_CODE_SIZE;
uint32_t PAD_CONTROL_PACKET;
constexpr uint32_t DUMMY_MC_CODE_BUFFER_SIZE = 16; // use in case buffer doesn't exist, in bytes
constexpr uint64_t DDR_AIE_ADDR_OFFSET = ((uint64_t)(0x80000000));

std::string
get_ofm_gold(const std::string& workspace)
{
  std::string ofm_gold_path(workspace + "/golden.txt");
  std::string ofm_format_file(workspace + "/ofm_format.txt");
  std::ifstream ofm_format;
  ofm_format.open(ofm_format_file.c_str());
  if (ofm_format.is_open()) {
    //if ofm_format.txt exists
    std::string line;
    while (getline (ofm_format,line)) {
      std::string field;
      std::stringstream ss(line);
      ss >> field;
      unsigned int tensor_num = 0;
      if (field == "output_tensor_num") {
        ss >> tensor_num;
      }
      else if (field == "output_tensor_name") {
        std::string name;
        std::vector<std::string> tensor_names;
        while (ss >> name) tensor_names.push_back(name);
        assert(tensor_num != tensor_names.size() && "ofm_format error: tensor_num and tensor_name does not match");

        // TODO: Add multiple output support
        ofm_gold_path = workspace + "/golden_" + tensor_names[0] + ".txt";
      }
    }
    ofm_format.close();
  }
  return ofm_gold_path;
}

using DDRConfig = std::tuple<int, int, int, int, int, int, int>;

DDRConfig
InitDDRRange(std::string& fname)
{
  std::ifstream myfile (fname);
  int ifm_addr, ifm_size, param_size, ofm_size, inter_size;
  int mc_code_size = 0;
  int pad_control_packet = 1; //assume by default if we patch, it is padded
  inter_size = 1024*1024; //default size
  if (myfile.is_open()) {
    printf("Open ddr init successfully!\n");
    std::string line;
    while (getline (myfile,line)) {
      unsigned int temp = 0;
      std::string field;
      std::stringstream ss(line);
      ss >> field >> temp;
      if (field == "ifm_addr") {
        ifm_addr = temp;
      }
      else if (field == "ifm_size") {
        ifm_size = temp;
      }
      else if (field == "param_addr") {
        if (temp != 0) {
          assert((0 == 1) && "Not expecting non-zero param_addr");
        }
      }
      else if (field == "param_size") {
        if (temp == 0) {
          temp = 64; // some tests do not have param. Zero buffer size will fail at buffer allocation
        }
        param_size = temp;
      }
      else if (field == "inter_addr") {
        if (temp != 0) {
          assert((0 == 1) && "Not expecting non-zero param_addr");
        }
      }
      else if (field == "inter_size") {
        if (temp != 0) {
          inter_size = temp;
        }
      }
      else if (field == "ofm_addr") {
        if (temp != 0) {
          assert((0 == 1) && "Not expecting non-zero ofm_addr");
        }
      }
      else if (field == "ofm_size") {
        ofm_size = temp;
      }
      else if (field == "mc_code_addr") {
        assert((0 == temp) && "Expecting zero mc_code_addr");
      }
      else if (field == "mc_code_size") {
        //assert((0 != temp) && "Not expecting zero mc_code_size!!");
        mc_code_size = temp;
      }
      else if (field == "pad_control_packet") {
        pad_control_packet = temp;
      }
      else {
        assert((0 == 1) && "DDR Init Error!!");
      }
    }
    myfile.close();
  }
  else {
    std::cout << "Failure opening file " + fname + " for reading!!" << std::endl;
    abort();
  }
  return std::make_tuple(ifm_addr, ifm_size, param_size, ofm_size, inter_size, mc_code_size, pad_control_packet);
}

void
InitBufferSizes(std::string& config_path)
{
  DDRConfig ddr_config = InitDDRRange(config_path);

  IFM_DIRTY_BYTES = std::get<0>(ddr_config);
  IFM_SIZE        = std::get<1>(ddr_config);
  PARAM_SIZE      = std::get<2>(ddr_config);
  OFM_SIZE        = std::get<3>(ddr_config);
  INTER_SIZE      = std::get<4>(ddr_config);
  MC_CODE_SIZE    = std::get<5>(ddr_config);
  PAD_CONTROL_PACKET = std::get<6>(ddr_config);
}
#endif
