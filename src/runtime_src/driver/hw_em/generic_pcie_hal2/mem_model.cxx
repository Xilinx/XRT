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

#include "mem_model.h"

mem_model::~ mem_model()
{
  serialize();
}

mem_model::mem_model(std::string deviceName):
  mDeviceName(deviceName),
  module_name("dr_wrapper_dr_i_sdaccel_generic_pcie_0.sdaccel_generic_pcie_model.ddrx_top_tlm_model_0.axi_app_tlm_model_0")
{
}

  unsigned int mem_model::writeDevMem(uint64_t offset, const void* src, unsigned int size)
  {
#ifdef DEBUGMSG
      cout<<endl<<module_name<<" write offset:"<<std::hex<<offset<<endl;
#endif 
      uint64_t written_bytes = 0;
      uint64_t addr = offset;
      while(written_bytes < size){
          uint64_t src_offset = written_bytes;

          unsigned char* page_ptr  = get_page(addr);
          uint64_t       page_addr = addr & ~(-1 << ADDRBITS);

          unsigned char* dest_buf_ptr = page_ptr + page_addr;
          unsigned char* src_buf_ptr  = (unsigned char*)(src)      + src_offset;

          uint64_t remaining_bytes_to_write = size - written_bytes;
          uint64_t unaligned_bytes_in_addr = (addr & ~(-1 << ADDRBITS));
          uint64_t bytes_upto_next_alignment = (0x1 << ADDRBITS) - unaligned_bytes_in_addr;

          uint64_t buf_size = 0;
          if(bytes_upto_next_alignment > remaining_bytes_to_write)
          {
              buf_size = remaining_bytes_to_write;
          }else{
              buf_size = bytes_upto_next_alignment;
          }

          memcpy(dest_buf_ptr,src_buf_ptr,buf_size);

          written_bytes += buf_size;
          addr += buf_size;
      }
#ifdef DEBUGMSG
      std::cout << endl;
      std::cout << "Write Operation size : " << size << endl;
      for(int i = 0; i < size;i++){
          cout << std::hex << (unsigned int)(((unsigned char*)src)[i]) << " ";
      }
      std::cout << endl;
      cout << "Write : " ;
      cout << "Offset --> " << offset << endl;
#endif    

      return 0;
  }

  unsigned int mem_model::readDevMem(uint64_t offset, void* dest, unsigned int size){
#ifdef DEBUGMSG
	  cout<<endl<<module_name<<" read offset:"<<std::hex<< (uint64_t)offset<<endl;
#endif 
	 
	  uint64_t read_bytes = 0;
	  uint64_t addr = offset;
	  while(read_bytes < size){
		  uint64_t dest_offset = read_bytes;

		  unsigned char* page_ptr  = get_page(addr);
		  uint64_t       page_addr = addr & ~(-1 << ADDRBITS);

		  unsigned char* src_buf_ptr = page_ptr + page_addr;
		  unsigned char* dest_buf_ptr  = (unsigned char*)(dest)      + dest_offset;

		  uint64_t remaining_bytes_to_read = size - read_bytes;
		  uint64_t unaligned_bytes_in_addr = (addr & ~(-1 << ADDRBITS));
		  uint64_t bytes_upto_next_alignment = (0x1 << ADDRBITS) - unaligned_bytes_in_addr;

		  uint64_t buf_size = 0;
		  if(bytes_upto_next_alignment > remaining_bytes_to_read)
		  {
			  buf_size = remaining_bytes_to_read;
		  }else{
			  buf_size = bytes_upto_next_alignment;
		  }
	  memcpy(dest_buf_ptr,src_buf_ptr,buf_size);
		  read_bytes += buf_size;
		  addr += buf_size;
	  }
#ifdef DEBUGMSG
	  std::cout << endl;
	  std::cout << "Read Operation size : " << size << endl;
	  for(unsigned int i = 0; i < size;i++){
		  cout << std::hex << (unsigned int)(((unsigned char*)dest)[i]) << " ";
	  }
	  std::cout << endl;
	  cout << "Read : " ;
	  cout << "Offset --> " << offset << endl;
#endif    

	  return 0;
  }
  unsigned char* mem_model::get_page(uint64_t offset) {
	  uint64_t page_idx = offset >> ADDRBITS;
	  std::string file_name = get_mem_file_name(page_idx);
	  if(pageCache.size() > N_1MBARRAYS)
	  {
		  std::cerr << "Out of Memory. DDR model does not support this much of memory\n";
		  exit(1);
	  } else {
      FILE* pFile = NULL;
		  if(pageCache.find(page_idx) != pageCache.end())
		  {
			  return pageCache[page_idx];
		  } else if(((pFile = fopen(file_name.c_str(),"r")) != NULL)) {
			  int fhandle = fileno(pFile);

			  if (deserialize_msg.ParseFromFileDescriptor(fhandle) == false)
        {
          fclose(pFile);
          exit(1);
        }
			  pageCache[page_idx] = new unsigned char[PAGESIZE];

			  memcpy(pageCache[page_idx],deserialize_msg.data().c_str(),PAGESIZE);
			  fclose(pFile);
			  return pageCache[page_idx];
		  } else {
			  pageCache[page_idx] = new unsigned char[PAGESIZE];
			  return pageCache[page_idx];
		  }
	  }
  }


  void mem_model::serialize() {
     FILE *pFile;
     int fhandle;
     for (pageCacheItr=pageCache.begin(); pageCacheItr != pageCache.end(); ++pageCacheItr)
     {
        std::string file_name = get_mem_file_name(pageCacheItr->first);
        pFile = fopen(file_name.c_str(),"w+");
        if(!pFile)
          continue;
        fhandle = fileno(pFile);
        if(fhandle == -1)
        {
          fclose(pFile);
          exit(1);
        }

        serialize_msg.set_data(reinterpret_cast<const char*>(pageCacheItr->second),PAGESIZE);
        if(serialize_msg.SerializeToFileDescriptor(fhandle) == false)
        {
          fclose(pFile);
          exit(1);
        }
        fclose(pFile);
     }
  }

 std::string mem_model::get_mem_file_name(uint64_t pageIdx)
 {
   std::string file_name("");
   std::string user("");
   if(getenv("USER") != NULL)
   {
     user = getenv("USER");
   }
   std::string file_path("");
   if(mDeviceName.empty() == false)
     file_path = "/tmp/" + user + "/" + std::to_string(getpid()) + "/hw_em/" + mDeviceName + "/" + module_name + "/";
    else
     file_path = "/tmp/" + user + "/hw_em/" + module_name + "/";
   std::stringstream mkdirCommand;
   mkdirCommand<<"mkdir -p "<<file_path;;
   struct stat statBuf;
   if ( stat(file_path.c_str(), &statBuf) == -1 )
   {
     int rV = system(mkdirCommand.str().c_str());
     if(rV == -1) {std::cout<<"unable to open/create mem file"<<std::endl;}
   }
    file_name = file_path + module_name + "_" + std::to_string(pageIdx);
#ifdef DEBUGMSG
      cout<<"ddr fmodel file_name: "<< file_name<<endl;
#endif
    return file_name;
 }


