// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.

#ifndef __BUFFER_OPS_H__
#define __BUFFER_OPS_H__
#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <iomanip>

// Copy values from text files into buff, expecting values are ascii encoded decimal
void
init_buf(int* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  std::istream_iterator<int> start(ifs), end;
  std::copy(start, end, buff);
}

// Copy values from text files into buff, expecting values are ascii encoded decimal
void
init_buf_int8(int8_t* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  std::string line;
  while (getline (ifs,line))
  {
    if(line.at(0)!='#'){
      unsigned int temp = 0;
      std::stringstream ss(line);
      ss >> temp;
      *(buff++) = temp;
    }
  }
}

// Copy values from binary files into buff, expecting values are ascii encoded decimal
void
init_buf_bin(int* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }
  ifs.read((char *)buff, bytesize);
}

// Copy values from text files into buff, expecting values are ascii encoded hex
void
init_hex_buf(int* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  std::string line;
  while (getline (ifs,line))
  {
    if(line.at(0)!='#'){
      unsigned int temp = 0;
      std::stringstream ss(line);
      ss >> std::hex >> temp;
      *(buff++) = temp;
    }
  }

  // Old Way, becore comments
  //std::istream_iterator<unsigned int> start(ifs >> std::hex), end;
  //std::copy(start, end, buff);
}

// Copy values from text files into xrt::bo at given offset, expecting values are ascii encoded decimal
void
init_buf_offset(int* buff, size_t bytesize, size_t offset, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  printf("BB Value:  %p\n", buff ); 
  buff += offset/sizeof(int);
  printf("BA Value:  %p\n", buff ); 
  std::istream_iterator<int> start(ifs), end;
  std::copy(start, end, buff);
}

// Copy values from binary files into buff, expecting values are ascii encoded decimal
void
init_buf_bin_offset(int* buff, size_t bytesize, size_t offset, std::string &filename)
{
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  printf("BB Value:  %p\n", buff ); 
  buff += offset/sizeof(int);
  printf("BA Value:  %p\n", buff ); 
  ifs.read((char *)buff, bytesize);
}

void
init_buf_bin_offset_verbose(int* buff, size_t bytesize, size_t offset, std::string &filename, uint32_t verbose)
{
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  if (verbose > 3) {
    printf("BB Value:  %p\n", buff ); 
  }
  buff += offset/sizeof(int);
  if (verbose > 3) {
    printf("BA Value:  %p\n", buff ); 
  }  
  ifs.read((char *)buff, bytesize);
}

void
dump_int8_buf(int8_t* buff, size_t bytesize, std::string &filename)
{
  std::ofstream ofs(filename);

  if (!ofs.is_open()) {
    std::cout << "Failure opening file " + filename + " for writing!!" << std::endl;
    abort();
  }

  for(int i = 0; i < bytesize; i++) {
      int temp =  *(buff + i);
      ofs << temp << std::endl;
  }
  ofs.close();
}

void
write_file_int8(char* filename, int8_t *out_buffer, int n)
{
  FILE *fpout = nullptr;
#ifdef __linux__ 
  fpout = fopen(filename, "w");
#elif _WIN32  
  fopen_s(&fpout, filename, "w");
#else

#endif
  
  for (int i = 0; i < n; i++) {
    fprintf(fpout, "%d\n", *(out_buffer + i));
  }
}

void
dump_buf(int* buff, size_t bytesize, std::string &filename)
{
  std::ofstream ofs(filename);

  if (!ofs.is_open()) {
    std::cout << "Failure opening file " + filename + " for writing!!" << std::endl;
    abort();
  }

  std::ostream_iterator<int> it(ofs,"\n");
  std::copy(buff, buff+(bytesize/sizeof(int)), it);
}

void
dump_buf_bin(int* buff, size_t bytesize, std::string &filename)
{
  std::ofstream ofs(filename, std::ios::binary);

  if (!ofs.is_open()) {
    std::cout << "Failure opening file " + filename + " for writing!!" << std::endl;
    abort();
  }

  ofs.write(reinterpret_cast<char*>(buff), bytesize);
}

void
dump_hex_buf(int* buff, size_t bytesize, std::string &filename)
{
  std::ofstream ofs(filename);

  if (!ofs.is_open()) {
    std::cout << "Failure opening file " + filename + " for writing!!" << std::endl;
    abort();
  }

  std::ostream_iterator<int> it(ofs << std::hex, "\n");
  std::copy(buff, buff+(bytesize/sizeof(int)), it);
}

int
comp_int8_buf(std::string &ofm, size_t bytesize, std::string &gold)
{
  std::ifstream gold_fs(gold);

  if (!gold_fs.is_open()) {
    std::cout << "Failure opening file " + gold + " for reading!!" << std::endl;
    abort();
  }

  std::ifstream ofm_fs(ofm);

  if (!ofm_fs.is_open()) {
    std::cout << "Failure opening file " + ofm + " for reading!!" << std::endl;
    abort();
  }

  std::istream_iterator<int> gold_it(gold_fs), ofm_it(ofm_fs), end;

  int errCount = 0;
  for (int i = 0; gold_it != end; ++i, ++gold_it, ++ofm_it) {
    if ((int) *ofm_it != *gold_it) ++errCount;
  }

  if (errCount == 0) std::cout << "TEST PASSED!" << std::endl;
  else std::cout << "TEST FAILED with " << errCount << " mismatches!" << std::endl;
  return errCount;
}

int
comp_int8_buf(int8_t* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  std::istream_iterator<int> it(ifs), end;

  int errCount = 0;
  for (int i = 0; it != end; ++i, ++it) {
    if ((int) buff[i] != *it) ++errCount;
  }

  if (errCount == 0) std::cout << "TEST PASSED!" << std::endl;
  else std::cout << "TEST FAILED with " << errCount << " mismatches!" << std::endl;
  return errCount;
}

void
print_dolphin()
{
  std::cout << "                                       .--.                           " << std::endl;
  std::cout << "                _______             .-\"  .\'                         " << std::endl;
  std::cout << "        .---u\"\"\"       \"\"\"\"---._  .\"    %                     " << std::endl;
  std::cout << "      .\'                        \"--.    %                           " << std::endl;
  std::cout << " __.--\'  o                          \"\".. \"                        " << std::endl;
  std::cout << "(____.                                  \":                           " << std::endl;
  std::cout << " `----.__                                 \".                         " << std::endl;
  std::cout << "         `----------__                     \".                        " << std::endl;
  std::cout << "               \".   . \"\"--.                 \".                    " << std::endl;
  std::cout << "                 \". \". bIt \"\"-.              \".                  " << std::endl;
  std::cout << "                   \"-.)        \"\"-.           \".                  " << std::endl;
  std::cout << "                                   \"\".         \".                  " << std::endl;
  std::cout << "                                      \"\".       \".                 " << std::endl;
  std::cout << "                                         \"\".      \".               " << std::endl;
  std::cout << "                                            \"\".    \".              " << std::endl;
  std::cout << "                      ^~^~^~^~^~^~^~^~^~^~^~^~^\"\".  \"^~^~^~^~^     " << std::endl;
  std::cout << "                                            ^~^~^~^  ~^~              " << std::endl;
  std::cout << "                                                 ^~^~^~               " << std::endl << std::endl;
}

void
print_eagle()
{
  std::cout << "                                         .' " << std::endl;
  std::cout << "            .------._                 ;     " << std::endl;
  std::cout << "      .-\"\"\"`-.<')    `-._           .'   " << std::endl;
  std::cout << "     (.--. _   `._       `'---.__.-'        " << std::endl;
  std::cout << "      `   `;'-.-'         '-    ._          " << std::endl;
  std::cout << "        .--'``  '._      - '   .            " << std::endl;
  std::cout << "         `""'-.    `---'    ,               " << std::endl;
  std::cout << " ''--..__      `\\                          " << std::endl;
  std::cout << "         ``''---'`\\      .'                " << std::endl;
  std::cout << "              jgs  `'. '                    " << std::endl;
  std::cout << "                     `'.                    " << std::endl << std::endl;
}

void
comp_buf(int* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  std::istream_iterator<int> it(ifs), end;

  int errCount = 0;
  for (int i = 0; it != end; ++i, ++it) {
    if (buff[i] != *it) ++errCount;
  }

  if (errCount == 0) std::cout << "TEST PASSED!" << std::endl;
  else std::cout << "TEST FAILED with " << errCount << " mismatches!" << std::endl;
}

void
comp_hex_buf(int* buff, size_t bytesize, std::string &filename)
{
  std::ifstream ifs(filename);

  if (!ifs.is_open()) {
    std::cout << "Failure opening file " + filename + " for reading!!" << std::endl;
    abort();
  }

  std::istream_iterator<unsigned int> it(ifs >> std::hex), end;

  int errCount = 0;
  for (int i = 0; it != end; ++i, ++it) {
    if ((unsigned)buff[i] != *it) ++errCount;
  }

  if (errCount == 0) std::cout << "TEST PASSED!" << std::endl;
  else std::cout << "TEST FAILED with " << errCount << " mismatches!" << std::endl;
}

size_t
get_instr_size(std::string &fname)
{
  std::ifstream myfile (fname);
  size_t i = 0;
  if (myfile.is_open()) {
    printf("Open instr successfully!\n");
    std::string line;
    while (getline (myfile,line)) {
      if (line.at(0)!='#') {
        i++;
      }
    }
    myfile.close();
  }
  return i;
}

void
DumpDDRWithStride(int8_t* ddr_addr, int dim0_stride, int dim1_stride, int dim2_stride,
                       int dim3_stride, int dim0_len, int dim1_len, int dim2_len, int dim3_len,
                       const char *filename)
{
  int8_t* dump_ddr_addr = ddr_addr;
  FILE *fpout = nullptr;
#ifdef __linux__ 
  fpout = fopen(filename, "w");
#elif _WIN32  
  fopen_s(&fpout, filename, "w");
#endif
  
  if (fpout == NULL) {
    printf("[ERROR] Failure opening file %s for reading!!\n", filename);
    abort();
  } else {
    for (int dim3_idx = 0; dim3_idx < dim3_len; dim3_idx++) {
      int8_t* dump_dim3_ddr_addr = dump_ddr_addr + dim3_idx * dim3_stride;
      for (int dim2_idx = 0; dim2_idx < dim2_len; dim2_idx++) {
        int8_t* dump_dim2_ddr_addr = dump_dim3_ddr_addr + dim2_idx * dim2_stride;
        for (int dim1_idx = 0; dim1_idx < dim1_len; dim1_idx++) {
          int8_t* dump_dim1_ddr_addr = dump_dim2_ddr_addr + dim1_idx * dim1_stride;
          for (int i = 0; i < dim0_len; i++) {
            fprintf(fpout, "%d\n", *(dump_dim1_ddr_addr + i));
          }
        }
      }
    }
  }
  fclose(fpout);
}

// Compare values from binary file to output buffer, expecting values are raw binary
int
comp_buf_strides(
  const int8_t *buff,
  std::string &goldenFile,
  std::string &dumpFile,
  const std::vector<unsigned>& shapes,
  const std::vector<unsigned>& strides,
  bool dump_output,
  bool dump_output_diff_only)
{
  int num_mismatches = 0;

  std::ifstream ifs(goldenFile, std::ios::in | std::ios::binary);
  if (!ifs.is_open()) throw std::runtime_error("Failed to open golden file");
  
  std::ofstream ofs(dumpFile, std::ios::out);
  if(!ofs.is_open()) throw std::runtime_error("Failed to open dump file");

  std::istreambuf_iterator<char> it(ifs), end;

  int32_t num_elems = 0;

  ofs << "Output (LHS) vs Golden (RHS) Dump\n---------------------------------\n";
  ofs << "tensor shape: [" << shapes[0] << ", " << shapes[1] << ", " << shapes[2] << ", " << shapes[3] << "]\n";
  ofs << "tensor stride: [" << strides[0] << ", " << strides[1] << ", " << strides[2] << ", " << strides[3] << "]\n";

  std::vector<std::int32_t> output_slice(shapes[3]);
  std::vector<std::int32_t> golden_slice(shapes[3]);

  for (int n = 0; n < shapes[0]; n++) {
    for (int h = 0; h < shapes[1]; h++) {
      for (int w = 0; w < shapes[2]; w++) {
        int32_t slice_mismatches = 0;
        for (int c = 0; c < shapes[3]; c++) {

          auto idx = n*strides[0] + h*strides[1] + w*strides[2] + c*strides[3];
          int32_t output_val = buff[idx];
          char byte = *it++;
          int32_t golden_val = *reinterpret_cast<int8_t*>(&byte);

          num_elems++;

          if (output_val != golden_val) {
            slice_mismatches++;
          }

          output_slice.at(c) = output_val;
          golden_slice.at(c) = golden_val;
        }

        bool enable_output_dump = dump_output && (!dump_output_diff_only || (slice_mismatches != 0));

        if (enable_output_dump) {
          std::stringstream ss_output;
          std::stringstream ss_golden;

          for (int c = 0; c < shapes[3]; c++) {
            ss_output << std::setw(4) << output_slice.at(c) << " ";
            ss_golden << std::setw(4) << golden_slice.at(c) << " ";
          }

          ss_output << " | ";
          ss_golden << "\n";

          ofs << "n: " << n << ", h: " << h << ", w: " << w <<"\n";
          ofs << ss_output.str() << ss_golden.str();
          ofs << "mismatches: " << slice_mismatches << "\n";
        }

        num_mismatches += slice_mismatches;
      }
    }
  }

  ofs << "num_mismatches: " << num_mismatches << " out of num_elems: " << num_elems << "\n";

  return num_mismatches;
}
#endif
