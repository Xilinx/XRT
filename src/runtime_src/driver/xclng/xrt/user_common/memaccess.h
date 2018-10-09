/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author: Sonal Santan
 * Simple command line utility to inetract with SDX PCIe devices
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
#ifndef MEMACCESS_H
#define MEMACCESS_H

#include <iostream>
#include <cstring>
#include <cstddef>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <numeric>

#include "dmatest.h"

#include "driver/include/xclhal2.h"
#include "driver/include/xclbin.h"

namespace xcldev {
  class memaccess {
    xclDeviceHandle mHandle;
    size_t mDDRSize, mDataAlignment;
    std::string mDevUserName;
  public:
    memaccess(xclDeviceHandle aHandle, size_t aDDRSize, size_t aDataAlignment, std::string& aDevUserName) :
              mHandle(aHandle), mDDRSize(aDDRSize), mDataAlignment (aDataAlignment), mDevUserName(aDevUserName) {}

    struct mem_bank_t {
      uint64_t m_base_address;
      uint64_t m_size;
      int m_index; 
      mem_bank_t (uint64_t aAddr, uint64_t aSize, int aIndex) : m_base_address(aAddr), m_size(aSize), m_index(aIndex) {}
    };

    /*
     * getDDRBanks()
     *
     * Get the addrs and size of each DDR bank
     * Sort the vector based on start address.
     * Returns number of banks.
     */
    int getDDRBanks( std::vector<mem_bank_t>& aBanks )
    {
        int nfound = 0;
        aBanks.clear();
        std::string path = "/sys/bus/pci/devices/" + mDevUserName + "/mem_topology"; // TODO: unify common sysfs reads
        struct stat sb;
        if( stat( path.c_str(), &sb ) < 0 ) {
            std::cout << "ERROR: failed to stat " << path << std::endl;
            return errno;
        }
        std::ifstream ifs( path.c_str(), std::ifstream::binary );
        if( !ifs.good() ) {
            return errno;
        }
        ifs.read( (char*)&nfound, sizeof(nfound) );
        ifs.seekg( 0, ifs.beg );
        if( nfound == 0 ) {
            std::cout << "ERROR: Memory topology is not available, ensure that a valid bitstream is programmed onto the card." << std::endl;
            ifs.close();
            return nfound;
        }
        int buf_size = sizeof(mem_topology)*nfound + offsetof(mem_topology, m_mem_data);
        buf_size *= 2; // double buffer size for padding safety.
        char* buffer = new char[ buf_size ];
        memset( buffer, 0, buf_size );
        ifs.read( buffer, buf_size ); // TODO: read entry by entry instead of entire mem_topology struct.
        mem_topology *map = (mem_topology *)buffer;
        for( int i = 0; i < map->m_count; i++ ) {
            if( map->m_mem_data[i].m_used && map->m_mem_data[i].m_type != MEM_STREAMING ) {
                aBanks.emplace_back( map->m_mem_data[i].m_base_address, map->m_mem_data[i].m_size*1024, i );
            }
        }
        std::sort (aBanks.begin(), aBanks.end(),
                   [] (const mem_bank_t& a, const mem_bank_t& b) {return (a.m_base_address < b.m_base_address);});
        delete [] buffer;
        ifs.close();
        return nfound;
    }

    /*
     * readBank()
     *
     * Read from specified address, specified size within a bank
     * Caller's responsibility to do sanity checks. No sanity checks done here
     */
    int readBank(std::ofstream& aOutFile, unsigned long long aStartAddr, unsigned long long aSize) {
      char *buf = 0;
      unsigned long long blockSize = 0x20000;
      if (posix_memalign((void**)&buf, getpagesize(), blockSize))
        return -1;
      std::memset(buf, 0, blockSize);

      size_t count = aSize;
      uint64_t incr;
      for (uint64_t phy = aStartAddr; phy < aStartAddr+aSize; phy += incr) {
        incr = (count >= blockSize) ? blockSize : count;
        //std::cout << "Reading from addr " << std::hex << phy << " aSize = " << std::hex << incr << std::dec << std::endl;
        if (xclUnmgdPread(mHandle, 0, buf, incr, phy) < 0) {
          //error
          std::cout << "Error (" << strerror (errno) << ") reading 0x" << std::hex << incr << " bytes from DDR at offset 0x" << std::hex << phy << std::dec << "\n";
          free(buf);
          return -1;
        }
        count -= incr;
        if (incr) {
          aOutFile.write((const char*)buf, incr);
          if ((aOutFile.rdstate() & std::ifstream::failbit) != 0) {
            std::cout << "Error writing to file at offset " << aSize-count << "\n";
          }
        }
        std::cout << "INFO: Read size 0x" << std::hex << incr << " B. Total Read so far 0x" << aSize-count << std::endl;
      }
      free(buf);
      if (count != 0) {
        std::cout << "Error! Read " << std::dec << aSize-count << " bytes, requested " << aSize << std::endl;
        return -1;
      }
      return count;
    }

    int runDMATest(size_t blocksize, unsigned int aPattern) 
    {
        int result = 0;
        std::vector<mem_bank_t> mems;
        int numBanks = getDDRBanks(mems);
        if (!numBanks) {
          std::cout << "ERROR: Memory topology is not available, ensure that a valid bitstream is programmed onto the card \n";
          return -1;
        }

        for(const auto itr : mems) {
            if( writeBank(itr.m_base_address, itr.m_size, aPattern) == -1) 
                return -1;
            result = readCompare(itr.m_base_address, itr.m_size, aPattern, false);
            if(result < 0)
                return result;
            DMARunner runner(mHandle, blocksize, 1 << itr.m_index);
            result = runner.run();
            if( result < 0 )
                return result;

        }

        
        return result;
    }

    /*
     * readWriteHelper()
     *
     * Sanity check the user's Start Address and Size against the mem topology
     * If the start address is 0 (ie. unspecified by user) change it to the first available address
     * If the size is 0 (ie. unspecified by user) change it to the maximum available size
     * Fill the vector with the available banks
     * Set the iterator to the bank containing the start address
     * returns the number of banks the start address and size going to span
     * return -1 in case of any sanity check failures
     */
    int readWriteHelper (unsigned long long& aStartAddr, unsigned long long& aSize,
                std::vector<mem_bank_t>& vec_banks, std::vector<mem_bank_t>::iterator& startbank) {
        int nbanks = getDDRBanks(vec_banks);
        if (!nbanks) {
          std::cout << "ERROR: Memory topology is not available, ensure that a valid bitstream is programmed onto the card \n";
          return -1;
        }

        std::stringstream sstr;
//        //This lambda captures the bank info in the stringstream
//        auto banksinfo = [&sstr](uint64_t result, const mem_bank_t& obj) {
//          sstr << "[Addr: 0x" << std::hex << obj.m_base_address << ", Size: " << std::dec << obj.m_size << "]";
//          return (result + obj.m_size);
//        };

//        uint64_t total_mem = std::accumulate(vec_banks.begin(), vec_banks.end(), (uint64_t)0, std::move(banksinfo));

        //if given start address is 0 then choose start address to be the lowest address available
        unsigned long long startAddr = aStartAddr == 0 ? vec_banks.front().m_base_address : aStartAddr;
        aStartAddr = startAddr;

        //Sanity check start address
        startbank = std::find_if(vec_banks.begin(), vec_banks.end(),
                    [startAddr](const mem_bank_t& item) {return (startAddr >= item.m_base_address && startAddr < (item.m_base_address+item.m_size));});

        if (startbank == vec_banks.end()) {
          std:: cout << "ERROR: Start address 0x" << std::hex << startAddr << " is not valid" << std::dec << std::endl;
          std:: cout << "Available memory banks: " << sstr.str() << std::endl;
          return -1;
        }
        //Sanity check access size
        uint64_t availableSize = std::accumulate(startbank, vec_banks.end(), (uint64_t)0,
                [](uint64_t result, const mem_bank_t& obj) {return (result + obj.m_size);}) ;

        availableSize -= (startAddr - startbank->m_base_address);
        if (aSize > availableSize) {
          std:: cout << "ERROR: Cannot access " << aSize << " bytes of memory from start address 0x" << std::hex << startAddr << std::dec << std::endl;
          std:: cout << "Available memory banks: " << sstr.str() << std::endl;
          return -1;
        }

        //if given size is 0, then the end Address is the max address of the unused bank
        unsigned long long size = (aSize == 0) ? availableSize : aSize;
        aSize = size;

        //Find the number of banks this read/write straddles, this is just for better messaging
        int bankcnt = 0;
        unsigned long long tsize = size;
        for(auto it = startbank; it!=vec_banks.end(); ++it) {
          unsigned long long available_bank_size;
          if (it != startbank) {
            available_bank_size = it->m_size;
          }
          else {
            available_bank_size = it->m_size - (startAddr - it->m_base_address);
          }
          if (tsize != 0) {
            unsigned long long accesssize = (tsize > available_bank_size) ? (unsigned long long) available_bank_size : tsize;
            ++bankcnt;
            tsize -= accesssize;
          }
          else {
            break;
          }
        }
        return bankcnt;
    }

    /*
     * read()
     */
    int read(std::string aFilename, unsigned long long aStartAddr = 0, unsigned long long aSize = 0) {
      std::vector<mem_bank_t> vec_banks;
      unsigned long long startAddr = aStartAddr;
      unsigned long long size = aSize;
      std::vector<mem_bank_t>::iterator startbank;
      int bankcnt = 0;

      //Sanity check the address and size against the mem topology
      if ((bankcnt = readWriteHelper(startAddr, size, vec_banks, startbank)) == -1) {
        return -1;
      }

      if (bankcnt > 1) {
        std::cout << "INFO: Reading " << std::dec << size << " bytes from DDR address 0x"  << std::hex << startAddr
                                    << " straddles " << bankcnt << " banks" << std::dec << std::endl;
      }
      else {
        std::cout << "INFO: Reading from single bank, " << std::dec << size << " bytes from DDR address 0x"  << std::hex << startAddr
                                    << std::dec << std::endl;
      }
      std::ofstream outFile(aFilename, std::ofstream::out | std::ofstream::binary);
      char temp[32] = "====START of DDR Data=========\n";
      outFile.write(temp, sizeof(temp));

      size_t count = size;
      for(auto it = startbank; it!=vec_banks.end(); ++it) {
        unsigned long long available_bank_size;
        if (it != startbank) {
          startAddr = it->m_base_address;
          available_bank_size = it->m_size;
        }
        else {
          //startAddr = startAddr;
          available_bank_size = it->m_size - (startAddr - it->m_base_address);
        }
        if (size != 0) {
          unsigned long long readsize = (size > available_bank_size) ? (unsigned long long) available_bank_size : size;
          if( readBank(outFile, startAddr, readsize) == -1) {
            return -1;
          }
          size -= readsize;
        }
        else {
          break;
        }
      }
      strncpy(temp, "\n=====END of DDR Data=========\n", sizeof(temp));
      outFile.write(temp, sizeof(temp));
      outFile.close();
      std::cout << "INFO: Read data saved in file: " << aFilename << "; Num of bytes: " << std::dec << count-size << " bytes " << std::endl;
      return size;
    }

    /*
     * readCompare()
     */
    int readCompare(unsigned long long aStartAddr = 0, unsigned long long aSize = 0, unsigned int aPattern = 'J', bool checks = true) {
      void *buf = 0;
      void *bufPattern = 0;
      unsigned long long size = aSize;
      std::vector<mem_bank_t> vec_banks;
      std::vector<mem_bank_t>::iterator startbank;
      int bankcnt = 0;
      //unsigned long long blockSize = 0x20000;
      unsigned long long blockSize = aSize;
      if (blockSize < 64) {
        blockSize = 64;
      }

      if (posix_memalign(&buf, getpagesize(), blockSize+1))//Last is for termination char
        return -1;
      if (posix_memalign(&bufPattern, getpagesize(), blockSize+1)) {//Last is for termination char
        free(buf);
        return -1;
      }
      std::memset(buf, '\0', blockSize+1);//Fill with termination char
      std::memset(bufPattern, '\0', blockSize+1);//Fill with termination char
      std::memset(bufPattern, aPattern, blockSize);

      if(checks) {
          //Sanity check the address and size against the mem topology
          if ((bankcnt = readWriteHelper(aStartAddr, size, vec_banks, startbank)) == -1) {
              free(buf);
              free(bufPattern);
              return -1;
          }
      }

      unsigned long long endAddr = aSize == 0 ? mDDRSize : aStartAddr+aSize;
      size = endAddr-aStartAddr;

      // Use plain POSIX open/pwrite/close.
      size_t count = size;
      uint64_t incr;
      for (uint64_t phy = aStartAddr; phy < aStartAddr+size; phy += incr) {
        incr = (count >= blockSize) ? blockSize : count;
        //Reset the read buffer
        std::memset(buf, '\0', blockSize+1);//Fill with termination char
        std::memset(bufPattern, '\0', blockSize+1);//Fill with termination char
        std::memset(bufPattern, aPattern, incr);//Need this when count is < blockSize
        //std::cout << "Reading from addr " << std::hex << phy << " size = " << std::hex << incr << std::dec << std::endl;
        if (xclUnmgdPread(mHandle, 0, buf, incr, phy) < 0) {
          //error
          std::cout << "Error (" << strerror (errno) << ") reading 0x" << std::hex << incr << " bytes from DDR at offset 0x" << std::hex << phy << std::dec << "\n";
          free(buf);
          free(bufPattern);
          return -1;
        }
        count -= incr;
        if (incr) {
          //char temp = aPattern;
          //std::cout << "INFO: Pattern char is: " << temp << std::endl;
          if( std::string((char*)buf) != std::string((char*)bufPattern)) { // strings are equal
            std::cout << "Error: read data didn't meet the pattern. Total Num of Bytes Read = " << std::dec << size << std::endl;
            std::cout << "Error: read data is: " << std::string((char*)buf) << std::endl;
          }
        }
        //std::cout << "INFO: Read size: " << std::dec << incr << " B. Total Read so far: " << std::dec << size-count << std::endl;
      }
      free(buf);
      free(bufPattern);
      if (count != 0) {
        std::cout << "Error! Read " << std::dec << size-count << " bytes, requested " << size << std::endl;
        return -1;
      }
      return count;
    }

    /*
     * writeBank()
     *
     * Write to the specified address within a bank
     * Caller's responsibility to do sanity checks. No sanity checks done here
     */
    int writeBank(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern) {
      char *buf = 0;
      unsigned long long endAddr;
      unsigned long long size;
      unsigned long long blockSize = 0x20000;//128KB
      if (posix_memalign((void**)&buf, getpagesize(), blockSize))
        return -1;

      endAddr = aStartAddr + aSize;
      size = endAddr-aStartAddr;

      // Use plain POSIX open/pwrite/close.

      std::cout << "INFO: Writing DDR with " << std::dec << size << " bytes of pattern: 0x"
         << std::hex << aPattern << " from address 0x" <<std::hex << aStartAddr << std::endl;

      unsigned long long count = size;
      uint64_t incr = 0;
      std::memset(buf, aPattern, blockSize);
      for(uint64_t phy=aStartAddr; phy<endAddr; phy+=incr) {
        incr = (count >= blockSize) ? blockSize : count;
        //std::cout << "Writing to addr " << std::hex << phy << " size = " << std::hex << incr << std::dec << std::endl;
        if (xclUnmgdPwrite(mHandle, 0, buf, incr, phy) < 0) {
          //error
          std::cout << "Error (" << strerror (errno) << ") writing 0x" << std::hex << incr << " bytes to DDR at offset 0x" << std::hex << phy << std::dec << "\n";
          free(buf);
          return -1;
        }
        count -= incr;
      }

      free(buf);
      if (count != 0) {
        std::cout << "Error! Written " << std::dec << size-count << " bytes, requested " << size << std::endl;
        return -1;
      }
      return count;
    }

    /*
     * write()
     */
    int write(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
      std::vector<mem_bank_t> vec_banks;
      unsigned long long startAddr = aStartAddr;
      unsigned long long size = aSize;
      std::vector<mem_bank_t>::iterator startbank;
      int bankcnt = 0;

      //Sanity check the address and size against the mem topology
      if ((bankcnt = readWriteHelper(startAddr, size, vec_banks, startbank)) == -1) {
        return -1;
      }

      if (bankcnt > 1) {
        std::cout << "INFO: Writing " << std::dec << size << " bytes from DDR address 0x"  << std::hex << startAddr
                                    << " straddles " << bankcnt << " banks" << std::dec << std::endl;
      }
      else {
        std::cout << "INFO: Writing to single bank, " << std::dec << size << " bytes from DDR address 0x"  << std::hex << startAddr
                                    << std::dec << std::endl;
      }
      for(auto it = startbank; it!=vec_banks.end(); ++it) {
        unsigned long long available_bank_size;
        if (it != startbank) {
          startAddr = it->m_base_address;
          available_bank_size = it->m_size;
        }
        else {
          //startAddr = startAddr;
          available_bank_size = it->m_size - (startAddr - it->m_base_address);
        }
        if (size != 0) {
          unsigned long long writesize = (size > available_bank_size) ? (unsigned long long) available_bank_size : size;
          if( writeBank(startAddr, writesize, aPattern) == -1) {
            return -1;
          }
          size -= writesize;
        }
        else {
          break;
        }
      }
      return size;
    }

    /*
     * write()
     */
    int write(unsigned long long aStartAddr, unsigned long long aSize, char *srcBuf) {
      void *buf = 0;
      unsigned long long endAddr;
      unsigned long long size;
      unsigned long long blockSize = aSize; //0x20000;//128KB
      if (posix_memalign(&buf, getpagesize(), blockSize))
        return -1;

      endAddr = aSize == 0 ? mDDRSize : aStartAddr + aSize;
      size = endAddr-aStartAddr;

      // Use plain POSIX open/pwrite/close.
      std::cout << "INFO: Writing DDR with " << size << " bytes from file, "
                << " from address 0x" << std::hex << aStartAddr << std::dec << std::endl;

      unsigned long long count = size;
      uint64_t incr;
      memcpy(buf, srcBuf, aSize);
      for(uint64_t phy=aStartAddr; phy<endAddr; phy+=incr) {
        incr = (count >= blockSize) ? blockSize : count;
        if (xclUnmgdPwrite(mHandle, 0, buf, incr, phy) < 0) {
          //error
          std::cout << "Error (" << strerror (errno) << ") writing 0x" << std::hex << incr << " bytes to DDR at offset 0x" << phy << std::dec << std::endl;
          free(buf);
          return -1;
        }
        count -= incr;
      }

      free(buf);
      if (count != 0) {
        std::cout << "Error! Written " << size-count << " bytes, requested " << size << std::endl;
        return -1;
      }
      return count;
    }

    /*
     * writeQuiet()
     */
    int writeQuiet(unsigned long long aStartAddr, unsigned long long aSize, unsigned int aPattern = 'J') {
        void *buf = 0;
        unsigned long long endAddr;
        unsigned long long size;
        //unsigned long long blockSize = 0x20000;
        unsigned long long blockSize = aSize;
        if (posix_memalign(&buf, getpagesize(), blockSize))
          return -1;

        endAddr = aSize == 0 ? mDDRSize : aStartAddr + aSize;
        size = endAddr-aStartAddr;

        // Use plain POSIX open/pwrite/close.

        //std::cout << "INFO: Writing DDR with " << std::dec << size << " bytes of pattern: 0x"
           //<< std::hex << aPattern << " from address 0x" <<std::hex << aStartAddr << std::endl;

        //char temp = aPattern;
        //std::cout << "INFO: Pattern char is: " << temp << std::endl;

        unsigned long long count = size;
        uint64_t incr;
        std::memset(buf, aPattern, blockSize);
        for(uint64_t phy=aStartAddr; phy<endAddr; phy+=incr) {
          incr = (count >= blockSize) ? blockSize : count;
          //std::cout << "Writing to addr " << std::hex << phy << " size = " << std::hex << incr << std::dec << std::endl;
          if (xclUnmgdPwrite(mHandle, 0, buf, incr, phy) < 0) {
            //error
            std::cout << "Error (" << strerror (errno) << ") writing 0x" << std::hex << incr << " bytes to DDR at offset 0x" << std::hex << phy << std::dec << "\n";
            return -1;
          }
          count -= incr;
        }

        if (count != 0) {
          std::cout << "Error! Written " << std::dec << size-count << " bytes, requested " << size << std::endl;
          return -1;
        }
        return count;
      }

  };
}

#endif /* MEMACCESS_H */

