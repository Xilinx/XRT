/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author(s) : Sonal Santan
 *           : Hem Neema
 *           : Ryan Radjabi
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
#include <iostream>
#include <string>
#include <fstream>
#include <cassert>
#include <thread>
#include <cstring>
#include <vector>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include "xspi.h"
#include "mgmt-reg.h"
#include "flasher.h"

#ifdef WINDOWS
#define __func__ __FUNCTION__
#endif

#ifdef __GNUC__
# define XSPI_UNUSED __attribute__((unused))
#endif

//#define FLASH_BASE_ADDRESS BPI_FLASH_OFFSET
#define PAGE_SIZE 256
static const bool FOUR_BYTE_ADDRESSING = false;

uint32_t MAX_NUM_SECTORS = 0;
uint32_t selected_sector = -1;

//testing sizes.
#define WRITE_DATA_SIZE 128
#define READ_DATA_SIZE 128


#define COMMAND_PAGE_PROGRAM            0x02 /* Page Program command */
#define COMMAND_QUAD_WRITE              0x32 /* Quad Input Fast Program */
#define COMMAND_EXT_QUAD_WRITE          0x38 /* Extended quad input fast program */
#define COMMAND_4KB_SUBSECTOR_ERASE     0x20 /* 4KB Subsector Erase command */
#define COMMAND_32KB_SUBSECTOR_ERASE    0x52 /* 32KB Subsector Erase command */
#define COMMAND_SECTOR_ERASE            0xD8 /* Sector Erase command */
#define COMMAND_BULK_ERASE              0xC7 /* Bulk Erase command */
#define COMMAND_RANDOM_READ             0x03 /* Random read command */
#define COMMAND_DUAL_READ               0x3B /* Dual Output Fast Read */
#define COMMAND_DUAL_IO_READ            0xBB /* Dual IO Fast Read */
#define COMMAND_QUAD_READ               0x6B /* Quad Output Fast Read */
#define COMMAND_QUAD_IO_READ            0xEB /* Quad IO Fast Read */
#define COMMAND_IDCODE_READ             0x9F /* Read ID Code */
//read commands
#define COMMAND_STATUSREG_READ               0x05 /* Status read command */
#define COMMAND_FLAG_STATUSREG_READ          0x70 /* Status flag read command */
#define COMMAND_NON_VOLATILE_CFGREG_READ     0xB5 /* Non volatile configuration register read command */
#define COMMAND_VOLATILE_CFGREG_READ         0x85 /* Volatile configuration register read command */
#define COMMAND_ENH_VOLATILE_CFGREG_READ     0x65 /* Enhanced volatile configuration register read command */
#define COMMAND_EXTENDED_ADDRESS_REG_READ    0xC8 /* Enhanced volatile configuration register read command */
//write commands
#define COMMAND_STATUSREG_WRITE              0x01 /* Status read command */
#define COMMAND_NON_VOLATILE_CFGREG_WRITE    0xB1 /* Non volatile configuration register read command */
#define COMMAND_VOLATILE_CFGREG_WRITE        0x81 /* Volatile configuration register read command */
#define COMMAND_ENH_VOLATILE_CFGREG_WRITE    0x61 /* Enhanced volatile configuration register read command */
#define COMMAND_EXTENDED_ADDRESS_REG_WRITE   0xC5 /* Enhanced volatile configuration register read command */

#define COMMAND_CLEAR_FLAG_REGISTER          0x50 /* Clear flag register */

//4-byte addressing
#define ENTER_FOUR_BYTE_ADDR_MODE               0xB7 /* enter 4-byte address mode */
#define EXIT_FOUR_BYTE_ADDR_MODE                0xE9 /* exit 4-byte address mode */
#define FOUR_BYTE_READ                          0x13 /* 4-byte read */
#define FOUR_BYTE_FAST_READ                     0x0C /* 4-byte fast read */
#define FOUR_BYTE_DUAL_OUTPUT_FAST_READ         0x3C /* 4-byte dual output fast read */
#define FOUR_BYTE_DUAL_IO_FAST_READ             0xBC /* 4-byte dual Input/output fast read */
#define FOUR_BYTE_QUAD_OUTPUT_FAST_READ         0x6C /* 4-byte quad output fast read */
#define FOUR_BYTE_QUAD_IO_FAST_READ             0xEC /* 4-byte quad output fast read */
#define FOUR_BYTE_PAGE_PROGRAM                  0x12 /* 4-byte page program */
#define FOUR_BYTE_QUAD_INPUT_FAST_PROGRAM       0x34 /* 4-byte quad input fast program */
#define FOUR_BYTE_QUAD_INPUT_EXT_FAST_PROGRAM   0x3E /* 4-byte quad input extended fast program */
#define FOUR_BYTE_SECTOR_ERASE                  0xDC /* 4-byte sector erase */

static const unsigned READ_WRITE_EXTRA_BYTES = FOUR_BYTE_ADDRESSING ? 5 :4;
static const unsigned  SECTOR_ERASE_BYTES = FOUR_BYTE_ADDRESSING ? 5 :4;


#define IDCODE_READ_BYTES              5

#define DUAL_READ_DUMMY_BYTES           2
#define QUAD_READ_DUMMY_BYTES           4
#define DUAL_IO_READ_DUMMY_BYTES        2
#define QUAD_IO_READ_DUMMY_BYTES        5

//#define READ_WRITE_EXTRA_BYTES          4 /* Read/Write extra bytes */
//#define SECTOR_ERASE_BYTES              4 /* Sector erase extra bytes */
#define WRITE_ENABLE_BYTES              1 /* Write Enable bytes */
#define BULK_ERASE_BYTES                1 /* Bulk erase extra bytes */
#define STATUS_READ_BYTES               2 /* Status read bytes count */
#define STATUS_WRITE_BYTES              2 /* Status write bytes count */



#define NUM_SLAVES 2
#define SLAVE_SELECT_MASK ((1 << NUM_SLAVES) -1)
/*
 * Flash not busy mask in the status register of the flash device.
 */
#define FLASH_SR_IS_READY_MASK          0x01 /* Ready mask */
#define COMMAND_WRITE_ENABLE        0x06 /* Write Enable command */

//SPI control reg masks.
#define XSP_CR_LOOPBACK_MASK       0x00000001 /**< Local loopback mode */
#define XSP_CR_ENABLE_MASK         0x00000002 /**< System enable */
#define XSP_CR_MASTER_MODE_MASK    0x00000004 /**< Enable master mode */
#define XSP_CR_CLK_POLARITY_MASK   0x00000008 /**< Clock polarity high or low */
#define XSP_CR_CLK_PHASE_MASK      0x00000010 /**< Clock phase 0 or 1 */
#define XSP_CR_TXFIFO_RESET_MASK   0x00000020 /**< Reset transmit FIFO */
#define XSP_CR_RXFIFO_RESET_MASK   0x00000040 /**< Reset receive FIFO */
#define XSP_CR_MANUAL_SS_MASK      0x00000080 /**< Manual slave select assert */
#define XSP_CR_TRANS_INHIBIT_MASK  0x00000100 /**< Master transaction inhibit */

/**
 * LSB/MSB first data format select. The default data format is MSB first.
 * The LSB first data format is not available in all versions of the Xilinx Spi
 * Device whereas the MSB first data format is supported by all the versions of
 * the Xilinx Spi Devices. Please check the HW specification to see if this
 * feature is supported or not.
 */
#define XSP_CR_LSB_MSB_FIRST_MASK       0x00000200

//End SPI CR masks

//SPI status reg masks
#define XSP_SR_RX_EMPTY_MASK       0x00000001 /**< Receive Reg/FIFO is empty */
#define XSP_SR_RX_FULL_MASK        0x00000002 /**< Receive Reg/FIFO is full */
#define XSP_SR_TX_EMPTY_MASK       0x00000004 /**< Transmit Reg/FIFO is empty */
#define XSP_SR_TX_FULL_MASK        0x00000008 /**< Transmit Reg/FIFO is full */
#define XSP_SR_MODE_FAULT_MASK     0x00000010 /**< Mode fault error */
#define XSP_SR_SLAVE_MODE_MASK     0x00000020 /**< Slave mode select */

/*
 * The following bits are available only in axi_qspi Status register.
 */
#define XSP_SR_CPOL_CPHA_ERR_MASK  0x00000040 /**< CPOL/CPHA error */
#define XSP_SR_SLAVE_MODE_ERR_MASK 0x00000080 /**< Slave mode error */
#define XSP_SR_MSB_ERR_MASK        0x00000100 /**< MSB Error */
#define XSP_SR_LOOP_BACK_ERR_MASK  0x00000200 /**< Loop back error */
#define XSP_SR_CMD_ERR_MASK        0x00000400 /**< 'Invalid cmd' error */


//End SPI SR masks

#define XSP_SRR_OFFSET          0x40    /**< Software Reset register */
#define XSP_CR_OFFSET           0x60    /**< Control register */
#define XSP_SR_OFFSET           0x64    /**< Status Register */
#define XSP_DTR_OFFSET          0x68    /**< Data transmit */
#define XSP_DRR_OFFSET          0x6C    /**< Data receive */
#define XSP_SSR_OFFSET          0x70    /**< 32-bit slave select */
#define XSP_TFO_OFFSET          0x74    /**< Tx FIFO occupancy */
#define XSP_RFO_OFFSET          0x78    /**< Rx FIFO occupancy */

#define BYTE1               0 /* Byte 1 position */
#define BYTE2               1 /* Byte 2 position */
#define BYTE3               2 /* Byte 3 position */
#define BYTE4               3 /* Byte 4 position */
#define BYTE5               4 /* Byte 5 position */
#define BYTE6               5 /* Byte 6 position */
#define BYTE7               6 /* Byte 7 position */
#define BYTE8               7 /* Byte 8 position */

//JEDEC vendor IDs
#define MICRON_VENDOR_ID   0x20
#define MACRONIX_VENDOR_ID 0xC2

/**
 * SPI Software Reset Register (SRR) mask.
 */
#define XSP_SRR_RESET_MASK              0x0000000A

// Bitstream guard information
#define NOOP        0x00000020//0x20000000
#define DUMMY       0xFFFFFFFF
#define BUSWIDTH1   0xBB000000//0x000000BB
#define BUSWIDTH2   0x44002211//0x11220044
#define SYNC        0x665599AA//0xAA995566
#define TIMER       0x01200230//0x30022001
#define WDT_ENABLE  0x02000040//0x40000002

#define BITSTREAM_GUARD_SIZE 0x1000
uint32_t BITSTREAM_START_LOC = -1; //Set to 0xFFFFFFFF
uint32_t BITSTREAM_GUARD[] = {
            DUMMY,
            BUSWIDTH1,
            BUSWIDTH2,
            DUMMY,
            DUMMY,
            SYNC,
            NOOP,
            NOOP,
            TIMER,
            WDT_ENABLE,
            NOOP,
            NOOP
};

//----
#define XSpi_ReadReg(RegOffset) readReg(RegOffset)
#define XSpi_WriteReg(RegOffset, RegisterValue) writeReg(RegOffset, RegisterValue)

#define XSpi_SetControlReg(Mask) XSpi_WriteReg(XSP_CR_OFFSET, (Mask))
#define XSpi_GetControlReg() XSpi_ReadReg(XSP_CR_OFFSET)

#define XSpi_GetStatusReg() XSpi_ReadReg(XSP_SR_OFFSET)

#define XSpi_SetSlaveSelectReg(Mask) XSpi_WriteReg(XSP_SSR_OFFSET, (Mask))
#define XSpi_GetSlaveSelectReg() XSpi_ReadReg(XSP_SSR_OFFSET)

//---

static uint8_t WriteBuffer[PAGE_SIZE + READ_WRITE_EXTRA_BYTES];
static uint8_t ReadBuffer[PAGE_SIZE + READ_WRITE_EXTRA_BYTES + 4];

static int slave_index = 0;

static std::array<int,2> flashVendors = {
    MICRON_VENDOR_ID,
    MACRONIX_VENDOR_ID
};
static int flashVendor = -1;

static bool TEST_MODE = false;
static bool TEST_MODE_MCS_ONLY = false;

static const uint32_t CONTROL_REG_START_STATE =  XSP_CR_TRANS_INHIBIT_MASK | XSP_CR_MANUAL_SS_MASK |XSP_CR_RXFIFO_RESET_MASK
        | XSP_CR_TXFIFO_RESET_MASK | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK ;

static void clearReadBuffer(unsigned size) {
    for(unsigned i =0; i < size; ++i) {
        ReadBuffer[i] = 0;
    }
}

static void clearWriteBuffer(unsigned size) {
    for(unsigned i =0; i < size; ++i) {
        WriteBuffer[i] = 0;
    }
}

static void clearBuffers() {
    clearReadBuffer(PAGE_SIZE + READ_WRITE_EXTRA_BYTES+4);
    clearWriteBuffer(PAGE_SIZE + READ_WRITE_EXTRA_BYTES);
}

XSPI_Flasher::XSPI_Flasher( unsigned int device_index, char *inMap )
{
    mMgmtMap = inMap; // brought in from Flasher object
}

/**
 * @brief XSPI_Flasher::~XSPI_Flasher
 *
 * - munmap
 * - delete file descriptor
 */
XSPI_Flasher::~XSPI_Flasher()
{
}

unsigned XSPI_Flasher::getSector(unsigned address) {
    return (address >> 24) & 0xF;
}

bool XSPI_Flasher::setSector(unsigned address) {
    uint32_t sector = getSector(address);
    //Select sector before
    if(sector >= MAX_NUM_SECTORS) {
        std::cout << "ERROR: Invalid sector encountered" << std::endl;
        std::cout << "ERROR: Bad address 0x" << std::hex << address << std::dec << std::endl;
        return false;
    } else if(sector == selected_sector) //Don't do anything if its already selected
        return true;

    if(!writeRegister(COMMAND_EXTENDED_ADDRESS_REG_WRITE, sector, 1))
        return false;
    else {
        selected_sector = sector;
        return true;
    }
}

int XSPI_Flasher::xclTestXSpi(int index)
{
    TEST_MODE = true;

    if(TEST_MODE_MCS_ONLY) {
        //just test the mcs.
        return 0;
    }

    //2 slaves present, set the slave index.
    slave_index = index;

    //print the IP (not of flash) control/status register.
    uint32_t ControlReg = XSpi_GetControlReg();
    uint32_t StatusReg = XSpi_GetStatusReg();
    std::cout << "Boot IP Control/Status " << std::hex << ControlReg << "/" << StatusReg << std::dec << std::endl;

    //Make sure it is ready to receive commands.
    ControlReg = XSpi_GetControlReg();
    ControlReg = CONTROL_REG_START_STATE;

    XSpi_SetControlReg(ControlReg);
    ControlReg = XSpi_GetControlReg();
    StatusReg = XSpi_GetStatusReg();
    std::cout << "Reset IP Control/Status " << std::hex << ControlReg << "/" << StatusReg << std::dec << std::endl;

    //1. Testing idCode reads.
    //--
    std::cout << "Testing id code " << std::endl;
    if(!getFlashId()) {
        std::cout << "Exiting now, as could not get correct idcode" << std::endl;
        exit(-EOPNOTSUPP);
    }

    std::cout << "id code successful (please verify the idcode output too" << std::endl;
    std::cout << "Now reading various flash registers" << std::endl;

    //2. Testing register reads.
    //Using STATUS_READ_BYTES 2 for all, TODO ?
    uint8_t Cmd = COMMAND_STATUSREG_READ;
    std::cout << "Testing COMMAND_STATUSREG_READ" << std::endl;
    readRegister(Cmd, STATUS_READ_BYTES);

    std::cout << "Testing COMMAND_FLAG_STATUSREG_READ" << std::endl;
    Cmd = COMMAND_FLAG_STATUSREG_READ;
    readRegister(Cmd, STATUS_READ_BYTES);

    std::cout << "Testing COMMAND_NON_VOLATILE_CFGREG_READ" << std::endl;
    Cmd = COMMAND_NON_VOLATILE_CFGREG_READ;
    readRegister(Cmd, 4);

    std::cout << "Testing COMMAND_VOLATILE_CFGREG_READ" << std::endl;
    Cmd = COMMAND_VOLATILE_CFGREG_READ;
    readRegister(Cmd, STATUS_READ_BYTES);

    std::cout << "Testing COMMAND_ENH_VOLATILE_CFGREG_READ" << std::endl;
    Cmd = COMMAND_ENH_VOLATILE_CFGREG_READ;
    readRegister(Cmd, STATUS_READ_BYTES);

    std::cout << "Testing COMMAND_EXTENDED_ADDRESS_REG_READ" << std::endl;
    Cmd = COMMAND_EXTENDED_ADDRESS_REG_READ;
    readRegister(Cmd, STATUS_READ_BYTES);

    //3. Testing simple read and write
    std::cout << "Testing read and write of 16 bytes" << std::endl;

    //unsigned baseAddr = 0x007A0000;
    unsigned baseAddr = 0;
    unsigned Addr = 0;
    unsigned AddressBytes = 3;
    if(FOUR_BYTE_ADDRESSING) {
        AddressBytes = 4;
        writeRegister(ENTER_FOUR_BYTE_ADDR_MODE, 0, 0);
    }else
        writeRegister(EXIT_FOUR_BYTE_ADDR_MODE, 0, 0);

    //Verify 3 or 4 byte addressing, 0th bit == 1 => 4 byte.
    std::cout << "Testing COMMAND_FLAG_STATUSREG_READ" << std::endl;
    Cmd = COMMAND_FLAG_STATUSREG_READ;
    readRegister(Cmd, STATUS_READ_BYTES);

    XSPI_UNUSED uint8_t WriteCmd = 0xff;
    XSPI_UNUSED uint8_t ReadCmd = 0xff;

    //Test the higher two sectors - first test erase.

    //First try erasing a sector and reading a
    //page (we should get FFFF ...)
    for(unsigned sector = 2 ; sector <= 3; sector++)
    {
        clearBuffers();

        if(!writeRegister(COMMAND_EXTENDED_ADDRESS_REG_WRITE, sector, 1))
            return false;

        std::cout << "Testing COMMAND_EXTENDED_ADDRESS_REG_READ" << std::endl;
        Cmd = COMMAND_EXTENDED_ADDRESS_REG_READ;
        readRegister(Cmd, STATUS_READ_BYTES);

        //Sector Erase will reset TX and RX FIFO
        if(!sectorErase(Addr + baseAddr, COMMAND_4KB_SUBSECTOR_ERASE))
            return false;

        bool ready = isFlashReady();
        if(!ready){
            std::cout << "Unable to get flash ready" << std::endl;
            return false;
        }

        //try faster read.
        if(FOUR_BYTE_ADDRESSING) {
            ReadCmd = FOUR_BYTE_QUAD_OUTPUT_FAST_READ;
        }else
            ReadCmd = COMMAND_QUAD_READ;

        //if(!readPage(Addr, ReadCmd))
        if(!readPage(Addr + baseAddr))
            return false;
    }

    clearBuffers();
    //---Erase test done


    //---Now try writing and reading a page.
    //first write 2 pages (using 4 128Mb writes) each to 2 sectors, and then read them

    //Write data
    for(unsigned sector = 2 ; sector <= 3; sector++)
    {
        if(!writeRegister(COMMAND_EXTENDED_ADDRESS_REG_WRITE, sector, 1))
            return false;

        std::cout << "Testing COMMAND_EXTENDED_ADDRESS_REG_READ" << std::endl;
        Cmd = COMMAND_EXTENDED_ADDRESS_REG_READ;
        readRegister(Cmd, STATUS_READ_BYTES);

        for(int j = 0; j < 4; ++j)
        {
            clearBuffers();
            for(unsigned i = 0; i < WRITE_DATA_SIZE; ++ i) {
                WriteBuffer[i+ AddressBytes + 1] = j + sector + i; //some random data.
            }

            Addr = baseAddr + WRITE_DATA_SIZE*j;

            if(!writePage(Addr)) {
                std::cout << "Write page unsuccessful, returning" << std::endl;
                return -ENXIO;
            }
        }

    }

    clearBuffers();

    //Read the data back, use 2 reads each of 128 bytes, twice to test 2 pages.
    for(unsigned sector = 2 ; sector <= 3; sector++)
    {
        //Select a sector (sector 2)
        if(!writeRegister(COMMAND_EXTENDED_ADDRESS_REG_WRITE, sector, 1))
            return false;

        std::cout << "Testing COMMAND_EXTENDED_ADDRESS_REG_READ" << std::endl;
        Cmd = COMMAND_EXTENDED_ADDRESS_REG_READ;
        readRegister(Cmd, STATUS_READ_BYTES);

        //This read should be mix of a b c .. and Z Y X ...
        for(int j = 0 ; j < 4; ++j)
        {
            clearBuffers();
            Addr = baseAddr + WRITE_DATA_SIZE*j;
            if(!readPage(Addr)) {
                std::cout << "Read page unsuccessful, returning" << std::endl;
                return -ENXIO;
            }
        }
        std::cout << "Done reading sector: " << sector << std::endl;
    }

    return 0;
}

int XSPI_Flasher::xclUpgradeFirmware2(std::istream& mcsStream1, std::istream& mcsStream2) {
    int status = 0;
    status = xclUpgradeFirmwareXSpi(mcsStream1, 0);
    if(status)
        return status;
    clearBuffers();
    recordList.clear();
    return xclUpgradeFirmwareXSpi(mcsStream2, 1);
}

int XSPI_Flasher::xclUpgradeFirmwareXSpi(std::istream& mcsStream, int index) {
    clearBuffers();
    recordList.clear();

    if (!mMgmtMap)
        return -EACCES;

    slave_index = index;
    std::string line;
    std::string startAddress;
    ELARecord record;
    bool endRecordFound = false;

    int lineno = 0;
    while (!mcsStream.eof() && !endRecordFound) {
    lineno++;
        std::string line;
        std::getline(mcsStream, line);
        if (line.size() == 0) {
            continue;
        }
        if (line[0] != ':') {
            return -EINVAL;
        }
        const unsigned dataLen = std::stoi(line.substr(1, 2), 0 , 16);
        const unsigned address = std::stoi(line.substr(3, 4), 0, 16);
        const unsigned recordType = std::stoi(line.substr(7, 2), 0 , 16);
        switch (recordType) {
        case 0x00:
        {
            if (dataLen > 16) {
                // For xilinx mcs files data length should be 16 for all records
                // except for the last one which can be smaller
                return -EINVAL;
            }
            if (address != (record.mDataCount+(record.mStartAddress & 0xFFFF))) {
                if(record.mDataCount == 0) {
                    //First entry only.
                    assert(record.mStartAddress != 0);
                    assert(record.mEndAddress != 0);
                    record.mStartAddress += address;
                    record.mEndAddress += address;
                }else {
                    std::cout << "Address is not contiguous ! " << std::endl;
                    return -EINVAL;
                }
            }
            //if ( ((record.mEndAddress-record.mStartAddress)& 0xFFFF) != address) {
              //  return -EINVAL;
            //}
            record.mDataCount += dataLen;
            record.mEndAddress += dataLen;
            break;
        }
        case 0x01:
        {
            if (startAddress.size() == 0) {
                break;
            }
            recordList.push_back(record);
            endRecordFound = true;
            break;
        }
        case 0x02:
        {
            assert(0);
            break;
        }
        case 0x04:
        {
            if (address != 0x0) {
                return -EINVAL;
            }
            if (dataLen != 2) {
                return -EINVAL;
            }
            std::string newAddress = line.substr(9, dataLen * 2);
            if (startAddress.size()) {
                // Finish the old record
                recordList.push_back(record);
            }
            // Start a new record
            record.mStartAddress = std::stoi(newAddress, 0 , 16);
            record.mStartAddress <<= 16;
            record.mDataPos = mcsStream.tellg();
            record.mEndAddress = record.mStartAddress;
            record.mDataCount = 0;
            startAddress = newAddress;
        }
        }
    }

    mcsStream.seekg(0);
    std::cout << "INFO: ***Found " << recordList.size() << " ELA Records" << std::endl;

    //Ensure we set bitstream guard to the first location
    BITSTREAM_START_LOC = recordList.front().mStartAddress;

    return programXSpi(mcsStream);
}

unsigned XSPI_Flasher::readReg(unsigned RegOffset) {
    unsigned value;
    if( Flasher::flashRead( 0, (unsigned long long)mMgmtMap + RegOffset, &value, 4 ) != 0 ) {
        assert(0);
        std::cout << "read reg ERROR" << std::endl;
    }
    return value;
}

int XSPI_Flasher::writeReg(unsigned RegOffset, unsigned value) {
    int status = Flasher::flashWrite(0, (unsigned long long)mMgmtMap + RegOffset, &value, 4);
    if(status != 0) {
        assert(0);
        std::cout << "write reg ERROR " << std::endl;
    }
    return 0;
}


bool XSPI_Flasher::waitTxEmpty() {
    long long delay = 0;
    const timespec req = {0, 5000};
    while (delay < 30000000000) {
        uint32_t StatusReg = XSpi_GetStatusReg();
        if(StatusReg & XSP_SR_TX_EMPTY_MASK )
            return true;
        //If not empty, check how many bytes remain.
        uint32_t Data = XSpi_ReadReg(XSP_TFO_OFFSET);
        std::cout << std::hex << Data << std::dec << std::endl;
        nanosleep(&req, 0);
        delay += 5000;
    }
    std::cout << "Unable to get Tx Empty\n";
    return false;
}

bool XSPI_Flasher::isFlashReady() {
    uint32_t StatusReg;
    const timespec req = {0, 5000};
    long long delay = 0;
    while (delay < 30000000000) {
        //StatusReg = XSpi_GetStatusReg();
        WriteBuffer[BYTE1] = COMMAND_STATUSREG_READ;
        bool status = finalTransfer(WriteBuffer, ReadBuffer, STATUS_READ_BYTES);
        if( !status ) {
            return false;
        }
        //TODO: wait ?
        StatusReg = ReadBuffer[1];
        if( (StatusReg & FLASH_SR_IS_READY_MASK) == 0) {
            return true;
        }
        //TODO: Try resetting. Uncomment next line?
        //XSpi_WriteReg(XSP_SRR_OFFSET, XSP_SRR_RESET_MASK);
        nanosleep(&req, 0);
        delay += 5000;
    }
    std::cout << "Unable to get Flash Ready\n";
    return false;
}

bool XSPI_Flasher::sectorErase(unsigned Addr, unsigned erase_cmd) {
    if(!isFlashReady())
        return false;

    if(!FOUR_BYTE_ADDRESSING) {
        //Select sector when only using 24bit address
        if(!setSector(Addr)) {
            std::cout << "ERROR: Unable to set sector for sectorErase cmd" << std::endl;
            return false;
        }
    }

    if(!writeEnable())
        return false;

    if(TEST_MODE) {
        std::cout << "Testing COMMAND_FLAG_STATUSREG_READ" << std::endl;
        unsigned Cmd = COMMAND_FLAG_STATUSREG_READ;
        readRegister(Cmd, STATUS_READ_BYTES);
    }

    uint32_t ControlReg = XSpi_GetControlReg();
    ControlReg |= XSP_CR_RXFIFO_RESET_MASK ;
    ControlReg |= XSP_CR_TXFIFO_RESET_MASK;
    XSpi_SetControlReg(ControlReg);

    /*
    * Prepare the WriteBuffer.
    */
    if(!FOUR_BYTE_ADDRESSING) {
        WriteBuffer[BYTE1] = erase_cmd;
        WriteBuffer[BYTE2] = (uint8_t) (Addr >> 16);
        WriteBuffer[BYTE3] = (uint8_t) (Addr >> 8);
        WriteBuffer[BYTE4] = (uint8_t) (Addr);
    }else {
        WriteBuffer[BYTE1] = erase_cmd;
        WriteBuffer[BYTE2] = (uint8_t) (Addr >> 24);
        WriteBuffer[BYTE3] = (uint8_t) (Addr >> 16);
        WriteBuffer[BYTE4] = (uint8_t) (Addr >> 8);
        WriteBuffer[BYTE5] = (uint8_t) Addr;
    }

    if(!finalTransfer(WriteBuffer, NULL, SECTOR_ERASE_BYTES))
        return false;

    /*
    * Wait till the Transfer is complete and check if there are any errors
    * in the transaction..
    */
    if(!waitTxEmpty())
        return false;

    return true;
}

bool XSPI_Flasher::bulkErase()
{
    if(!isFlashReady())
        return false;

    if(!writeEnable())
        return false;

    uint32_t ControlReg = CONTROL_REG_START_STATE;
    XSpi_SetControlReg(ControlReg);

    XSPI_UNUSED uint32_t testControlReg = XSpi_GetControlReg();
    XSPI_UNUSED uint32_t testStatusReg = XSpi_GetStatusReg();

    //2
    WriteBuffer[BYTE1] = COMMAND_BULK_ERASE;

    if(!finalTransfer(WriteBuffer, NULL, BULK_ERASE_BYTES))
        return false;

    return waitTxEmpty();
}

//Bitstream guard protects from partially programmed bitstreams
bool XSPI_Flasher::writeBitstreamGuard(unsigned Addr) {
    uint32_t bufferIndex = 0;
    uint32_t page_addr = Addr+WRITE_DATA_SIZE; //We insert a few dummy words before fallback instruction sequence
    unsigned char* write_buffer = &WriteBuffer[READ_WRITE_EXTRA_BYTES];

    //Clear whatever was at bitstream guard location
    if(!sectorErase(Addr, COMMAND_4KB_SUBSECTOR_ERASE))
        return false;

    //Write fallback instruction sequence
    memcpy(write_buffer, BITSTREAM_GUARD, sizeof(BITSTREAM_GUARD));
    bufferIndex+=sizeof(BITSTREAM_GUARD);
    for(; bufferIndex<WRITE_DATA_SIZE; bufferIndex++)
        write_buffer[bufferIndex] = 0xFF;

    return writePage(page_addr);
}

bool XSPI_Flasher::clearBitstreamGuard(unsigned Addr) {
    //Clear whatever was at bitstream guard location
    return sectorErase(Addr, COMMAND_4KB_SUBSECTOR_ERASE);
}

bool XSPI_Flasher::writeEnable() {
    uint32_t StatusReg = XSpi_GetStatusReg();
    if(StatusReg & XSP_SR_TX_FULL_MASK) {
        std::cout << "Tx fifo fill during WriteEnable" << std::endl;
        return false;
    }

    //1
    uint32_t ControlReg = XSpi_GetControlReg();
    ControlReg |= CONTROL_REG_START_STATE;
    XSpi_SetControlReg(ControlReg);

    //2
    WriteBuffer[BYTE1] = COMMAND_WRITE_ENABLE; //0x06

    if(!finalTransfer(WriteBuffer, NULL, WRITE_ENABLE_BYTES))
        return false;

    return waitTxEmpty();
}

bool XSPI_Flasher::getFlashId()
{
    if(!isFlashReady()) {
        std::cout << "Unable to get flash ready " << std::endl;
        return false;
    }

    bool Status = false;
    /* * Prepare the Write Buffer. */
    WriteBuffer[BYTE1] = COMMAND_IDCODE_READ;

    Status = finalTransfer(WriteBuffer, ReadBuffer, IDCODE_READ_BYTES);
    if( !Status ) {
        return false;
    }

    //Update flash vendor
    for (size_t i = 0; i < flashVendors.size(); i++)
        if(ReadBuffer[1] == flashVendors[i])
            flashVendor = flashVendors[i];

    //Update max number of sector. Value of 0x18 is 1 128Mbit sector
    //Note that macronix/micron use different #s
    if(ReadBuffer[3] == 0xFF)
        return false;
    else {
        switch(ReadBuffer[3]) {
	case 0x38:
        case 0x17:
        case 0x18:
            MAX_NUM_SECTORS = 1;
            break;
	case 0x39:
        case 0x19:
            MAX_NUM_SECTORS = 2;
            break;
	case 0x3A:
        case 0x20:
            MAX_NUM_SECTORS = 4;
            break;
	case 0x3B:
        case 0x21:
            MAX_NUM_SECTORS = 8;
            break;
	case 0x3C:
        case 0x22:
            MAX_NUM_SECTORS = 16;
            break;
        default:
            std::cout << "ERROR: Unrecognized sector field! Exiting..." << std::endl;
            return false;
        }
    }

    for (int i = 0; i < IDCODE_READ_BYTES; i++) {
        std::cout << "Idcode byte[" << i << "] " << std::hex << (int)ReadBuffer[i] << std::endl;
        ReadBuffer[i] = 0;
    }

    unsigned ffCount = 0;
    for (int i = 1; i < IDCODE_READ_BYTES; i++) {
        if ((unsigned int)ReadBuffer[i] == 0xff)
            ffCount++;
    }

    if(ffCount == IDCODE_READ_BYTES -1)
        return false;

    return true;
}


bool XSPI_Flasher::finalTransfer(uint8_t *SendBufPtr, uint8_t *RecvBufPtr, int ByteCount)
{
    uint32_t ControlReg;
    uint32_t StatusReg;
    uint32_t Data = 0;
    uint8_t  DataWidth = 8;
    uint32_t SlaveSelectMask = SLAVE_SELECT_MASK;

    uint32_t SlaveSelectReg = 0;
    if(slave_index == 0)
        SlaveSelectReg = ~0x01;
    else if(slave_index == 1)
        SlaveSelectReg = ~0x02;

    /*
   * Enter a critical section from here to the end of the function since
   * state is modified, an interrupt is enabled, and the control register
   * is modified (r/m/w).
   */
    ControlReg = XSpi_GetControlReg();
    StatusReg = XSpi_GetStatusReg();

    if(TEST_MODE)
        std::cout << "Control/Status " << std::hex << ControlReg << "/" << StatusReg << std::dec << std::endl;


    /*
   * If configured as a master, be sure there is a slave select bit set
   * in the slave select register. If no slaves have been selected, the
   * value of the register will equal the mask.  When the device is in
   * loopback mode, however, no slave selects need be set.
   */
    if (ControlReg & XSP_CR_MASTER_MODE_MASK) {
        if ((ControlReg & XSP_CR_LOOPBACK_MASK) == 0) {
            if (SlaveSelectReg == SlaveSelectMask) {
                std::cout << "No slave selected" << std::endl;
                return false;
            }
        }
    }

    /*
    * Set up buffer pointers.
    */
    uint8_t* SendBufferPtr = SendBufPtr;
    uint8_t* RecvBufferPtr = RecvBufPtr;

    int RemainingBytes = ByteCount;
    unsigned int BytesTransferred = 0;

    /*
    * Fill the DTR/FIFO with as many bytes as it will take (or as many as
    * we have to send). We use the tx full status bit to know if the device
    * can take more data. By doing this, the driver does not need to know
    * the size of the FIFO or that there even is a FIFO. The downside is
    * that the status register must be read each loop iteration.
    */
    StatusReg = XSpi_GetStatusReg();
    if((StatusReg & (1<<10)) != 0) {
        std::cout << "status reg in error situation " << std::endl;
        return false;
    }

    while (((StatusReg & XSP_SR_TX_FULL_MASK) == 0) && (RemainingBytes > 0)) {
        if (DataWidth == 8) {
            Data = *SendBufferPtr;
        } else if (DataWidth == 16) {
            Data = *(uint16_t *)SendBufferPtr;
        } else if (DataWidth == 32){
            Data = *(uint32_t *)SendBufferPtr;
        }

        if(Flasher::flashWrite(0, (unsigned long long)mMgmtMap + XSP_DTR_OFFSET, &Data, 4) != 0) {
            return false;
        }
        SendBufferPtr += (DataWidth >> 3);
        RemainingBytes -= (DataWidth >> 3);
        StatusReg = XSpi_GetStatusReg();
        if((StatusReg & (1<<10)) != 0) {
            std::cout << "Write command caused created error" << std::endl;
            return false;
        }
    }


    /*
    * Set the slave select register to select the device on the SPI before
    * starting the transfer of data.
    */
    XSpi_SetSlaveSelectReg(SlaveSelectReg);

    ControlReg = XSpi_GetControlReg();
    StatusReg = XSpi_GetStatusReg();

    if(TEST_MODE)
        std::cout << "Control/Status " << std::hex << ControlReg << "/" << StatusReg << std::dec << std::endl;

    if((StatusReg & (1<<10)) != 0) {
        std::cout << "status reg in error situation: 2 " << std::endl;
        return false;
    }

    /*
    * Start the transfer by no longer inhibiting the transmitter and
    * enabling the device. For a master, this will in fact start the
    * transfer, but for a slave it only prepares the device for a transfer
    * that must be initiated by a master.
    */
    ControlReg = XSpi_GetControlReg();
    ControlReg &= ~XSP_CR_TRANS_INHIBIT_MASK;
    XSpi_SetControlReg(ControlReg);

    if(TEST_MODE)
        std::cout << "Control/Status " << std::hex << ControlReg << "/" << StatusReg << std::dec << std::endl;


    //Data transfer to actual flash has already started happening here.

    { /* Polled mode of operation */

        // poll the status register to * Transmit/Receive SPI data.
        while(ByteCount > 0)
        {

            /*
             * Wait for the transfer to be done by polling the
             * Transmit empty status bit
             */
            do {
                StatusReg = XSpi_GetStatusReg();
            } while ((StatusReg & XSP_SR_TX_EMPTY_MASK) == 0);

            /*
             * A transmit has just completed. Process received data
             * and check for more data to transmit. Always inhibit
             * the transmitter while the transmit register/FIFO is
             * being filled, or make sure it is stopped if we're
             * done.
             */
            ControlReg = XSpi_GetControlReg();
            XSpi_SetControlReg(ControlReg | XSP_CR_TRANS_INHIBIT_MASK);

            ControlReg = XSpi_GetControlReg();

            if(TEST_MODE)
                std::cout << "Control/Status " << std::hex << ControlReg << "/" << StatusReg << std::dec << std::endl;

            /*
             * First get the data received as a result of the
             * transmit that just completed. We get all the data
             * available by reading the status register to determine
             * when the Receive register/FIFO is empty. Always get
             * the received data, but only fill the receive
             * buffer if it points to something (the upper layer
             * software may not care to receive data).
             */
            StatusReg = XSpi_GetStatusReg();

            while ((StatusReg & XSP_SR_RX_EMPTY_MASK) == 0)
            {
                //read the data.
                if(Flasher::flashRead(0, (unsigned long long)mMgmtMap + XSP_DRR_OFFSET, &Data, 4) != 0)
                {
                    return false;
                }


                if (DataWidth == 8) {
                    if(RecvBufferPtr != NULL) {
                        *RecvBufferPtr++ = (uint8_t)Data;
                    }
                } else if (DataWidth == 16) {
                    if (RecvBufferPtr != NULL){
                        *(uint16_t *)RecvBufferPtr = (uint16_t)Data;
                        RecvBufferPtr += 2;
                    }
                } else if (DataWidth == 32) {
                    if (RecvBufferPtr != NULL){
                        *(uint32_t *)RecvBufferPtr = Data;
                        RecvBufferPtr += 4;
                    }
                }

                BytesTransferred += (DataWidth >> 3);
                ByteCount -= (DataWidth >> 3);
                StatusReg = XSpi_GetStatusReg();
                if((StatusReg & (1<<10)) != 0) {
                    std::cout << "status reg in error situation " << std::endl;
                    return false;
                }
            }

            //If there are still unwritten bytes, then finishing writing (below code)
            //and reading (above code) them.
            if (RemainingBytes > 0) {
                /*
                 * Fill the DTR/FIFO with as many bytes as it
                 * will take (or as many as we have to send).
                 * We use the Tx full status bit to know if the
                 * device can take more data.
                 * By doing this, the driver does not need to
                 * know the size of the FIFO or that there even
                 * is a FIFO.
                 * The downside is that the status must be read
                 * each loop iteration.
                 */
                StatusReg = XSpi_GetStatusReg();

                while(((StatusReg & XSP_SR_TX_FULL_MASK)== 0) && (RemainingBytes > 0))
                {
                    if (DataWidth == 8) {
                        Data = *SendBufferPtr;
                    } else if (DataWidth == 16) {
                        Data = *(uint16_t *)SendBufferPtr;
                    } else if (DataWidth == 32) {
                        Data = *(uint32_t *)SendBufferPtr;
                    }

                    if(Flasher::flashWrite(0, (unsigned long long)mMgmtMap + XSP_DTR_OFFSET, &Data, 4) != 0) {
                        return false;
                    }

                    SendBufferPtr += (DataWidth >> 3);
                    RemainingBytes -= (DataWidth >> 3);
                    StatusReg = XSpi_GetStatusReg();
                    if((StatusReg & (1<<10)) != 0) {
                        std::cout << "status reg in error situation " << std::endl;
                        return false;
                    }
                }

                //Start the transfer by not inhibiting the transmitter any longer.
                ControlReg = XSpi_GetControlReg();
                ControlReg &= ~XSP_CR_TRANS_INHIBIT_MASK;
                XSpi_SetControlReg(ControlReg);
            }
        }

        //Stop the transfer by inhibiting * the transmitter.
        ControlReg = XSpi_GetControlReg();
        XSpi_SetControlReg(ControlReg | XSP_CR_TRANS_INHIBIT_MASK);

        /*
         * Deassert the slaves on the SPI bus when the transfer is complete,
         */
        XSpi_SetSlaveSelectReg(SlaveSelectMask);
    }

    return true;
}


bool XSPI_Flasher::writePage(unsigned Addr, uint8_t writeCmd)
{
    if(!isFlashReady())
        return false;

    if(!FOUR_BYTE_ADDRESSING) {
        //Select sector when only using 24bit address
        if(!setSector(Addr)) {
            std::cout << "ERROR: Unable to set sector for writePage cmd" << std::endl;
            return false;
        }
    }

    if(!writeEnable())
        return false;

    //1 : reset Tx and Rx FIFO's
    uint32_t ControlReg = CONTROL_REG_START_STATE;
    //  uint32_t ControlReg = XSpi_GetControlReg();
    //  ControlReg |= XSP_CR_RXFIFO_RESET_MASK ;
    //  ControlReg |= XSP_CR_TXFIFO_RESET_MASK;
    XSpi_SetControlReg(ControlReg);

    uint8_t WriteCmd = writeCmd;
    //2
    if(!FOUR_BYTE_ADDRESSING) {
        //3 byte address mode
        //COMMAND_PAGE_PROGRAM gives out all FF's
        //COMMAND_EXT_QUAD_WRITE: hangs the system
        if(writeCmd == 0xff) {
            if(flashVendor == MACRONIX_VENDOR_ID)
                WriteCmd = COMMAND_PAGE_PROGRAM;
            else
                WriteCmd = COMMAND_QUAD_WRITE;
        }

        WriteBuffer[BYTE1] = WriteCmd;
        WriteBuffer[BYTE2] = (uint8_t) (Addr >> 16);
        WriteBuffer[BYTE3] = (uint8_t) (Addr >> 8);
        WriteBuffer[BYTE4] = (uint8_t) Addr;
    }else {
        if(writeCmd == 0xff)
            WriteBuffer[BYTE1] = FOUR_BYTE_QUAD_INPUT_FAST_PROGRAM;
        WriteBuffer[BYTE2] = (uint8_t) (Addr >> 24);
        WriteBuffer[BYTE3] = (uint8_t) (Addr >> 16);
        WriteBuffer[BYTE4] = (uint8_t) (Addr >> 8);
        WriteBuffer[BYTE5] = (uint8_t) Addr;
    }

    //The data to write is already filled up, so now just write the buffer.
    if(!finalTransfer(WriteBuffer, ReadBuffer, WRITE_DATA_SIZE + READ_WRITE_EXTRA_BYTES))
        return false;

    if(!waitTxEmpty())
        return false;


    return true;

}

bool XSPI_Flasher::readPage(unsigned Addr, uint8_t readCmd)
{
    if(!isFlashReady())
        return false;

    if(!FOUR_BYTE_ADDRESSING) {
        //Select sector when only using 24bit address
        if(!setSector(Addr)) {
            std::cout << "ERROR: Unable to set sector for writePage cmd" << std::endl;
            return false;
        }
    }

    //--
    uint32_t ControlReg = CONTROL_REG_START_STATE;
    //  uint32_t ControlReg = XSpi_GetControlReg();
    //  ControlReg |= XSP_CR_RXFIFO_RESET_MASK ;
    //  ControlReg |= XSP_CR_TXFIFO_RESET_MASK;
    XSpi_SetControlReg(ControlReg);

    //1 : reset TX/RX FIFO's
    uint8_t ReadCmd = readCmd;

    //uint8_t ReadCmd = COMMAND_RANDOM_READ;
    if(!FOUR_BYTE_ADDRESSING) {
        //3 byte addressing mode
        if(readCmd == 0xff)
            ReadCmd = COMMAND_QUAD_READ;

        //3 byte address mode
        WriteBuffer[BYTE1] = ReadCmd;
        WriteBuffer[BYTE2] = (uint8_t) (Addr >> 16);
        WriteBuffer[BYTE3] = (uint8_t) (Addr >> 8);
        WriteBuffer[BYTE4] = (uint8_t) Addr;
    }else {
        if(readCmd == 0xff)
            ReadCmd = FOUR_BYTE_READ;
        WriteBuffer[BYTE1] = ReadCmd;
        WriteBuffer[BYTE2] = (uint8_t) (Addr >> 24);
        WriteBuffer[BYTE3] = (uint8_t) (Addr >> 16);
        WriteBuffer[BYTE4] = (uint8_t) (Addr >> 8);
        WriteBuffer[BYTE5] = (uint8_t) Addr;
    }

    unsigned ByteCount = READ_DATA_SIZE;

    if (ReadCmd == COMMAND_DUAL_READ) {
        ByteCount += DUAL_READ_DUMMY_BYTES;
    } else if (ReadCmd == COMMAND_DUAL_IO_READ) {
        ByteCount += DUAL_READ_DUMMY_BYTES;
    } else if (ReadCmd == COMMAND_QUAD_IO_READ) {
        ByteCount += QUAD_IO_READ_DUMMY_BYTES;
    } else if ( (ReadCmd==COMMAND_QUAD_READ) || (ReadCmd==FOUR_BYTE_QUAD_OUTPUT_FAST_READ)) {
        ByteCount += QUAD_READ_DUMMY_BYTES;
    }

    if(!finalTransfer(WriteBuffer, ReadBuffer, ByteCount + READ_WRITE_EXTRA_BYTES))
        return false;

    if(!waitTxEmpty())
        return false;

    //reset the RXFIFO bit so.
    ControlReg = XSpi_GetControlReg();
    ControlReg |= XSP_CR_RXFIFO_RESET_MASK ;
    XSpi_SetControlReg(ControlReg);

    return true;

}

bool XSPI_Flasher::prepareXSpi()
{
    if(TEST_MODE)
        return true;

    //Resetting selected_sector
    selected_sector = -1;

    XSPI_UNUSED uint32_t tControlReg = XSpi_GetControlReg();
    XSPI_UNUSED uint32_t tStatusReg = XSpi_GetStatusReg();

#if defined(_debug)
    std::cout << "Boot Control/Status " << std::hex << tControlReg << "/" << tStatusReg << std::dec << std::endl;
#endif

    uint32_t ControlReg = CONTROL_REG_START_STATE;
    XSpi_SetControlReg(ControlReg);

    tControlReg = XSpi_GetControlReg();
    tStatusReg = XSpi_GetStatusReg();

#if defined(_debug)
    std::cout << "After setting start state, Control/Status " << std::hex << tControlReg << "/" << tStatusReg << std::dec << std::endl;
#endif
    //--

    if(!getFlashId()) {
        std::cout << "Exiting now, as could not get correct idcode" << std::endl;
        exit(-EOPNOTSUPP);
    }

    //WriteEnable writes CONTROL_REG_START_STATE - that should be enough for initial configuration ?
    //if(!writeEnable())
    //return false;

    //Bulk erase the flash.
    //if(!bulkErase())
    //return false;

    return true;
}

int XSPI_Flasher::programXSpi(std::istream& mcsStream, const ELARecord& record) {
    //TODO: decrease the sleep time.
    const timespec req = {0, 20000};

#if defined(_debug)
    std::cout << "Programming block (" << std::hex << record.mStartAddress << ", " << record.mEndAddress << std::dec << ")" << std::endl;
#endif

    assert(mcsStream.tellg() < record.mDataPos);
    mcsStream.seekg(record.mDataPos, std::ios_base::beg);
    unsigned char* buffer = &WriteBuffer[READ_WRITE_EXTRA_BYTES];
    int bufferIndex = 0;
    int pageIndex = 0;
    std::string prevLine("");
    for (unsigned index = record.mDataCount; index > 0;) {
        std::string line;
        std::getline(mcsStream, line);
        if(TEST_MODE)
            std::cout << line << std::endl;
        const unsigned dataLen = std::stoi(line.substr(1, 2), 0 , 16);
        index -= dataLen;
        const unsigned recordType = std::stoi(line.substr(7, 2), 0 , 16);
        if (recordType != 0x00) {
            continue;
        }
        const std::string data = line.substr(9, dataLen * 2);
        // Write in byte swapped order
        for (unsigned i = 0; i < data.length(); i += 2) {
            unsigned value = std::stoi(data.substr(i, 2), 0, 16);
            buffer[bufferIndex++] = (unsigned char)value;
            assert(bufferIndex <= WRITE_DATA_SIZE);

#if 0
            //To enable byte swapping uncomment this.
            //      if ((bufferIndex % 4) == 0) {
            //        bufferIndex += 4;
            //      }
            //      assert(bufferIndex <= WRITE_DATA_SIZE);
            //      unsigned value = std::stoi(data.substr(i, 2), 0, 16);
            //      if(TEST_MODE)
            //        std::cout << data.substr(i, 2);
            //      buffer[--bufferIndex] = (unsigned char)value;
            //      if ((bufferIndex % 4) == 0) {
            //        bufferIndex += 4;
            //      }
#endif
            if (bufferIndex == WRITE_DATA_SIZE) {
                break;
            }
        }

        if(TEST_MODE)
            std::cout << std::endl;

#if 0
        //Uncomment if byte swapping enabled.

        //account for the last line
        //which can have say 14 bytes instead of 16
        if((bufferIndex %4)!= 0) {
            while ((bufferIndex %4)!= 0) {
                unsigned char fillValue = 0xFF;
                buffer[--bufferIndex] = fillValue;
            }
            bufferIndex += 4;
        }

        assert((bufferIndex % 4) == 0);
#endif

        assert(bufferIndex <= WRITE_DATA_SIZE);
        if (bufferIndex == WRITE_DATA_SIZE) {
#if defined(_debug)
            std::cout << "writing page " << pageIndex << std::endl;
#endif
            const unsigned address = std::stoi(line.substr(3, 4), 0, 16);
            //assert ( (address + dataLen) == static_cast<unsigned int>((pageIndex +1)*WRITE_DATA_SIZE));
            assert ( (address + dataLen - (record.mStartAddress & 0xFFFF))
                == static_cast<unsigned int>((pageIndex +1)*WRITE_DATA_SIZE));
            if(TEST_MODE) {
                std::cout << (address + dataLen) << " " << (pageIndex +1)*WRITE_DATA_SIZE << std::endl;
                std::cout << record.mStartAddress << " " << record.mStartAddress + pageIndex*PAGE_SIZE;
                std::cout << " " << address << std::endl;
            } else {
                if(!writePage(record.mStartAddress + pageIndex*WRITE_DATA_SIZE))
                    return -ENXIO;
                clearBuffers();
                {
                    //debug stuff
#if defined(_debug)
                    if(pageIndex == 0) {
                        if(!readPage(record.mStartAddress + pageIndex*WRITE_DATA_SIZE))
                            return -ENXIO;
                        clearBuffers();
                    }
#endif
                }
            }
            pageIndex++;
            nanosleep(&req, 0);
            bufferIndex = 0;
        }
        prevLine = line;

    }
    if (bufferIndex) {
        //Write the last page
        if(TEST_MODE) {
            std::cout << "writing final page " << pageIndex << std::endl;
            std::cout << bufferIndex << std::endl;
            std::cout << prevLine << std::endl;
        }

        const unsigned address = std::stoi(prevLine.substr(3, 4), 0, 16);
        const unsigned dataLen = std::stoi(prevLine.substr(1, 2), 0 , 16);

        if(TEST_MODE)
            std::cout << address % WRITE_DATA_SIZE << " " << dataLen << std::endl;

        //assert( (address % WRITE_DATA_SIZE + dataLen) == bufferIndex);

        if(!TEST_MODE) {

            //Fill unused half page to FF
            for(unsigned i = bufferIndex; i < WRITE_DATA_SIZE; ++i) {
                buffer[i] = 0xff;
            }

            if(!writePage(record.mStartAddress + pageIndex*WRITE_DATA_SIZE))
                return -ENXIO;
            nanosleep(&req, 0);
            clearBuffers();
            {
                //debug stuff
#if defined(_debug)
                if(!readPage(record.mStartAddress + pageIndex*WRITE_DATA_SIZE))
                    return -ENXIO;
                clearBuffers();
#endif
            }
        }
    }
    return 0;
}

int XSPI_Flasher::programXSpi(std::istream& mcsStream)
{
    //  for (ELARecordList::iterator i = mRecordList.begin(), e = mRecordList.end(); i != e; ++i) {
    //    i->mStartAddress <<= 16;
    //    i->mEndAddress += i->mStartAddress;
    //    // Convert from 2 bytes address to 4 bytes address
    //    i->mStartAddress /= 2;
    //    i->mEndAddress /= 2;
    //  }

    if (!prepareXSpi()) {
        std::cout << "ERROR: Unable to prepare the XSpi\n";
        return -EINVAL;
    }

    //if(!bulkErase())
    //return false;

    const timespec req = {0, 20000};
    nanosleep(&req, 0);

    uint32_t bitstream_shift_addr = 0;

    //First we enable bitstream guard if not writing to address 0
    //This will protect partially erased/programmed bitstreams
    if(BITSTREAM_START_LOC != 0) {
        if(!writeBitstreamGuard(BITSTREAM_START_LOC)) {
            std::cout << "ERROR: Unable to set bitstream guard!" << std::endl;
            return -EINVAL;
        }
        bitstream_shift_addr += BITSTREAM_GUARD_SIZE;
        std::cout << "Enabled bitstream guard. Bitstream will not be loaded until flashing is finished." << std::endl;
    }

    //Now we can safely erase all subsectors
    int beatCount = 0;
    std::cout << "Erasing flash" << std::flush;
    for (ELARecordList::iterator i = recordList.begin(), e = recordList.end(); i != e; ++i) {
        beatCount++;
        if(beatCount%20==0) {
            std::cout << "." << std::flush;
        }

        //Shift all write addresses below bitstream guard
        i->mStartAddress += bitstream_shift_addr;
        i->mEndAddress += bitstream_shift_addr;

        //Erase any subsectors in address range.
        for(uint32_t j = i->mStartAddress; j < i->mEndAddress; j+=0x1000) {
            //std::cout << "DEBUG: Erasing subsector @ 0x" << std::hex << j << std::dec << std::endl;
            if(!sectorErase(j, COMMAND_4KB_SUBSECTOR_ERASE)) {
                std::cout << "\nERROR: Failed to erase subsector!" << std::endl;
                return -EINVAL;
            }
            nanosleep(&req, 0); //Pause before next sector erase
        }
    }
    //New line after ...
    std::cout << std::endl;

    //Next we program flash. Note that bitstream guard is still active
    beatCount = 0;
    std::cout << "Programming flash" << std::flush;
    for (ELARecordList::iterator i = recordList.begin(), e = recordList.end(); i != e; ++i)
    {
        beatCount++;
        if(beatCount%20==0) {
            std::cout << "." << std::flush;
        }

        if(TEST_MODE) {
            std::cout << "INFO: Start address 0x" << std::hex << recordList.front().mStartAddress << std::dec << "\n";
            std::cout << "INFO: End address 0x" << std::hex << recordList.back().mEndAddress << std::dec << "\n";
        }

        bool ready = isFlashReady();
        if(!ready){
            std::cout << "\nERROR: Unable to get flash ready" << std::endl;
            return -EINVAL;
        }

        clearBuffers();

        if (programXSpi(mcsStream, *i)) {
            std::cout << "\nERROR: Could not programXSpi the block" << std::endl;
            return -EINVAL;
        }
        nanosleep(&req, 0);
    }
    std::cout << std::endl;

    //Finally we clear bitstream guard if not writing to address 0
    //This will allow the bitstream to be loaded
    if(BITSTREAM_START_LOC != 0) {
        if(!clearBitstreamGuard(BITSTREAM_START_LOC)) {
            std::cout << "ERROR: Unable to clear bitstream guard!" << std::endl;
            return -EINVAL;
        }
        std::cout << "Cleared bitstream guard. Bitstream now active." << std::endl;
    }

    return 0;
}

bool XSPI_Flasher::readRegister(unsigned commandCode, unsigned bytes) {

    if(!isFlashReady())
        return false;

    bool Status = false;

    WriteBuffer[BYTE1] = commandCode;

    Status = finalTransfer(WriteBuffer, ReadBuffer, bytes);

    if( !Status ) {
        return false;
    }

#if defined(_debug)
    std::cout << "Printing output (with some extra bytes of readRegister cmd)" << std::endl;
#endif

    for(unsigned i = 0; i < 5; ++ i) //Some extra bytes, no harm
    {
#if defined(_debug)
        std::cout << i << " " << std::hex << (int)ReadBuffer[i] << std::dec << std::endl;
#endif
        ReadBuffer[i] = 0; //clear
    }
    //Reset the FIFO bit.
    uint32_t ControlReg = XSpi_GetControlReg();
    ControlReg |= XSP_CR_RXFIFO_RESET_MASK ;
    ControlReg |= XSP_CR_TXFIFO_RESET_MASK ;
    XSpi_SetControlReg(ControlReg);

    return Status;
}

//max 16 bits for nonvolative cfg register.
//If extra_bytes == 0, then only the command is sent.
bool XSPI_Flasher::writeRegister(unsigned commandCode, unsigned value, unsigned extra_bytes) {
    if(!isFlashReady())
        return false;

    if(!writeEnable())
        return false;

    uint32_t ControlReg = XSpi_GetControlReg();
    ControlReg |= XSP_CR_TXFIFO_RESET_MASK;
    ControlReg |= XSP_CR_RXFIFO_RESET_MASK;
    XSpi_SetControlReg(ControlReg);

    bool Status = false;

    WriteBuffer[BYTE1] = commandCode;

    if(extra_bytes == 0) {
        //do nothing
    } else if(extra_bytes == 1)
        WriteBuffer[BYTE2] = (uint8_t) (value);
    else if(extra_bytes == 2) {
        WriteBuffer[BYTE2] = (uint8_t) (value >> 8);
        WriteBuffer[BYTE3] = (uint8_t) value;
    }else {
        std::cout << "ERROR: Setting more than 2 bytes" << std::endl;
        assert(0);
    }

    //+1 for cmd byte.
    Status = finalTransfer(WriteBuffer,NULL, extra_bytes+1);
    if(!Status)
        return false;

    if(!waitTxEmpty())
        return false;

    return Status;
}

