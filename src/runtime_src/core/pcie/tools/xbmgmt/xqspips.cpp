/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author(s) : Min Ma
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

#include <fstream>
#include <iostream>
#include <cassert>
#include <cstring>
#include "xqspips.h"
#include "core/pcie/driver/linux/include/mgmt-reg.h"
#include "flasher.h"

#include "unistd.h"

#ifdef WINDOWS
#define __func__ __FUNCTION__
#endif

#ifdef __GNUC__
# define XQSPIPS_UNUSED __attribute__((unused))
#endif

#define SAVE_FILE                   0
#define FLASH_BASE                  0x040000

/*
 * The following constants define the commands which may be sent to the Flash device.
 */
#define WRITE_STATUS_CMD            0x01
#define WRITE_CMD                   0x02
#define READ_CMD                    0x03
#define WRITE_DISABLE_CMD           0x04
#define READ_STATUS_CMD             0x05
#define WRITE_ENABLE_CMD            0x06
#define FAST_READ_CMD               0x0B
#define FAST_READ_CMD_4B            0x0C
#define WRITE_4B_CMD                0x12
#define READ_CMD_4B                 0x13
#define BANK_REG_RD                 0x16
#define BANK_REG_WR                 0x17
#define EXIT_4B_ADDR_MODE_ISSI      0x29
#define QUAD_WRITE_CMD              0x32
#define READ_CONFIG_CMD             0x35
#define DUAL_READ_CMD               0x3B
#define DUAL_READ_CMD_4B            0x3C
#define VOLATILE_WRITE_ENABLE_CMD   0x50
#define QUAD_READ_CMD               0x6B
#define QUAD_READ_CMD_4B            0x6C
#define READ_FLAG_STATUS_CMD        0x70
#define READ_ID                     0x9F
#define ENTER_4B_ADDR_MODE          0xB7
#define DIE_ERASE_CMD               0xC4
/* Bank register is called Extended Address Register in Micron */
#define EXTADD_REG_WR               0xC5
#define BULK_ERASE_CMD              0xC7
#define EXTADD_REG_RD               0xC8
#define FOURKB_SUBSECTOR_ERASE_CMD  0x20
#define SEC_ERASE_CMD               0xD8
#define SEC_4B_ERASE_CMD            0xDC
#define EXIT_4B_ADDR_MODE           0xE9

#define IDCODE_READ_BYTES               6
#define WRITE_ENABLE_BYTES              1 /* Write Enable bytes */
#define BULK_ERASE_BYTES                1 /* Bulk erase extra bytes */
#define STATUS_READ_BYTES               2 /* Status read bytes count */
#define STATUS_WRITE_BYTES              2 /* Status write bytes count */

#define FLASH_SR_BUSY_MASK          0x01
#define FOURKB_SUBSECTOR_SIZE       0x1000
#define SECTOR_SIZE                 0x10000

#define ENTER_4B    1
#define EXIT_4B     0

/* Registers offset */
#define GQSPI_CFG_OFFSET            0x100   /* GQSPI Configuration Register*/
#define GQSPI_ISR_OFFSET            0x104   /* GQSPI Status Register */
#define GQSPI_IER_OFFSET            0x108   /* GQSPI Interrupt Enable Register */
#define GQSPI_IDR_OFFSET            0x10C   /* GQSPI Interrupt Disable Register */
#define GQSPI_IMR_OFFSET            0x110   /* GQSPI Interrupt Mask Register */
#define GQSPI_EN_OFFSET             0x114   /* GQSPI Enable Register */
#define GQSPI_TXD_OFFSET            0x11C   /* GQSPI Transmit Data Register */
#define GQSPI_RXD_OFFSET            0x120   /* GQSPI Receive Data Register */
#define GQSPI_TX_THRESH_OFFSET      0x128   /* GQSPI TX FIFO Threshold Level Register */
#define GQSPI_RX_THRESH_OFFSET      0x12C   /* GQSPI RX FIFO Threshold Level Register */
#define GQSPI_GPIO_OFFSET           0x130   /* GQSPI GPIO for Write Protect Register */
#define GQSPI_LPBK_DLY_ADJ_OFFSET   0x138   /* GQSPI Lookback clock delay adjustment Register */
#define GQSPI_GEN_FIFO_OFFSET       0x140   /* GQSPI Generic FIFO Configuration Register */
#define GQSPI_SEL_OFFSET            0x144   /* GQSPI Select Register */
#define GQSPI_FIFO_CTRL_OFFSET      0x14C   /* GQSPI FIFO Control Register */
#define GQSPI_GF_THRESH_OFFSET      0x150   /* GQSPI Generic FIFO Threshold Level Register */
#define GQSPI_POLL_CFG_OFFSET       0x154   /* GQSPI Poll Configuration Register */
#define GQSPI_P_TIMEOUT_OFFSET      0x158   /* GQSPI Poll Time Out Register */
#define GQSPI_DATA_DLY_ADJ_OFFSET   0x1F8   /* GQSPI Rx Data Delay Register */
#define GQSPI_MOD_ID_OFFSET         0x1FC   /* GQSPI Module Identification Register */

/* Register constants/masks */
#define XQSPIPSU_CFG_MODE_EN_MASK                   0XC0000000U
#define XQSPIPSU_CFG_GEN_FIFO_START_MODE_MASK       0X20000000U
#define XQSPIPSU_CFG_START_GEN_FIFO_MASK            0X10000000U
#define XQSPIPSU_CFG_ENDIAN_MASK                    0X04000000U
#define XQSPIPSU_CFG_EN_POLL_TO_MASK                0X00100000U
#define XQSPIPSU_CFG_WP_HOLD_MASK                   0X00080000U
#define XQSPIPSU_CFG_BAUD_RATE_DIV_MASK             0X00000038U
#define XQSPIPSU_CFG_CLK_PHA_MASK                   0X00000004U
#define XQSPIPSU_CFG_CLK_POL_MASK                   0X00000002U

#define XQSPIPSU_GENFIFO_IMM_DATA_MASK              0x000FFU
#define XQSPIPSU_GENFIFO_DATA_XFER                  0x00100U
#define XQSPIPSU_GENFIFO_EXP                        0x00200U
#define XQSPIPSU_GENFIFO_EXP_START                  0x100U
#define XQSPIPSU_GENFIFO_MODE_MASK                  0x00C00U    /* And with ~MASK first */
#define XQSPIPSU_GENFIFO_BUS_MASK                   0x0C000U    /* And with ~MASK first */
#define XQSPIPSU_GENFIFO_TX                         0x10000U    /* inverse is zero pump */
#define XQSPIPSU_GENFIFO_RX                         0x20000U    /* inverse is RX discard */
#define XQSPIPSU_GENFIFO_STRIPE                     0x40000U
#define XQSPIPSU_GENFIFO_POLL                       0x80000U

#define XQSPIPSU_ISR_WR_TO_CLR_MASK                 0X00000002U
#define XQSPIPSU_ISR_POLL_TIME_EXPIRE_MASK          0X00000002U
#define XQSPIPSU_ISR_TXNOT_FULL_MASK                0X00000004U
#define XQSPIPSU_ISR_TXFULL_MASK                    0X00000008U
#define XQSPIPSU_ISR_RXNEMPTY_MASK                  0X00000010U
#define XQSPIPSU_ISR_RXFULL_MASK                    0X00000020U
#define XQSPIPSU_ISR_GENFIFOEMPTY_MASK              0X00000080U
#define XQSPIPSU_ISR_TXEMPTY_MASK                   0X00000100U
#define XQSPIPSU_ISR_GENFIFOFULL_MASK               0X00000400U
#define XQSPIPSU_ISR_RXEMPTY_MASK                   0X00000800U
#define XQSPIPSU_IDR_ALL_MASK                       0X00000FBEU
#define XQSPIPSU_FIFO_CTRL_RST_GEN_FIFO_MASK        0X00000001U
#define XQSPIPSU_FIFO_CTRL_RST_TX_FIFO_MASK         0X00000002U
#define XQSPIPSU_FIFO_CTRL_RST_RX_FIFO_MASK         0X00000004U
#define XQSPIPSU_LPBK_DLY_ADJ_USE_LPBK_MASK         0X00000020U

#define CFG_BAUD_RATE_DIV_2                         0X00000000U
#define CFG_BAUD_RATE_DIV_4                         0X00000008U
#define CFG_BAUD_RATE_DIV_8                         0X00000010U
#define CFG_BAUD_RATE_DIV_16                        0X00000018U
#define CFG_BAUD_RATE_DIV_32                        0X00000020U
#define CFG_BAUD_RATE_DIV_64                        0X00000028U
#define CFG_BAUD_RATE_DIV_128                       0X00000030U
#define CFG_BAUD_RATE_DIV_256                       0X00000038U

#define XQSPIPSU_GENFIFO_CS_LOWER                   0x01000U
#define XQSPIPSU_GENFIFO_CS_UPPER                   0x02000U
#define XQSPIPSU_GENFIFO_CS_BOTH                    0x03000U    /* inverse is reserved */
#define XQSPIPSU_GENFIFO_BUS_LOWER                  0x04000U
#define XQSPIPSU_GENFIFO_BUS_UPPER                  0x08000U
#define XQSPIPSU_GENFIFO_BUS_BOTH                   0x0C000U    /* inverse is no bus */
#define XQSPIPSU_GENFIFO_MODE_SPI                   0x00400U
#define XQSPIPSU_GENFIFO_MODE_DUALSPI               0x00800U
#define XQSPIPSU_GENFIFO_MODE_QUADSPI               0x00C00U
#define XQSPIPSU_GENFIFO_CS_SETUP                   0x05U
#define XQSPIPSU_GENFIFO_CS_HOLD                    0x04U
#define XQSPIPSU_TX_FIFO_THRESHOLD_RESET_VAL        0X00000001U
#define XQSPIPSU_RX_FIFO_THRESHOLD_RESET_VAL        0X00000001U
#define XQSPIPSU_GEN_FIFO_THRESHOLD_RESET_VAL       0X00000010U
#define XQSPIPSU_TXD_DEPTH                          64

// JEDEC vendor IDs
#define MICRON_VENDOR_ID   0x20
#define MACRONIX_VENDOR_ID 0xC2

#define XQSpiPS_ReadReg(RegOffset) readReg(RegOffset)
#define XQSpiPS_WriteReg(RegOffset, Value) writeReg(RegOffset, Value)

#define XQSpiPS_GetConfigReg()          XQSpiPS_ReadReg(GQSPI_CFG_OFFSET)
#define XQSpiPS_SetConfigReg(mask)      XQSpiPS_WriteReg(GQSPI_CFG_OFFSET, mask)
#define XQSpiPS_GetStatusReg()          XQSpiPS_ReadReg(GQSPI_ISR_OFFSET)
#define XQSpiPS_SetStatusReg(mask)      XQSpiPS_WriteReg(GQSPI_ISR_OFFSET, mask)
#define XQSpiPS_Enable_GQSPI()          XQSpiPS_WriteReg(GQSPI_EN_OFFSET, 0x1)
#define XQSpiPS_Disable_GQSPI()         XQSpiPS_WriteReg(GQSPI_EN_OFFSET, 0x0)
#define XQSpiPS_Sel_GQSPI()             XQSpiPS_WriteReg(GQSPI_SEL_OFFSET, 0x1)
#define is_GQSPI_Enable()               XQSpiPS_ReadReg(GQSPI_EN_OFFSET)
#define is_GQSPI_Mode()                 XQSpiPS_ReadReg(GQSPI_SEL_OFFSET)

#define XQSPIPSU_MSG_FLAG_STRIPE        0x1U
#define XQSPIPSU_MSG_FLAG_RX            0x2U
#define XQSPIPSU_MSG_FLAG_TX            0x4U

#define XQSPIPSU_SELECT_MODE_SPI        0x1U
#define XQSPIPSU_SELECT_MODE_DUALSPI    0x2U
#define XQSPIPSU_SELECT_MODE_QUADSPI    0x4U

#define printHEX(RegName, RegValue) \
do { \
    std::cout << RegName " 0x" << std::hex << RegValue << std::dec << std::endl; \
} while(0);

static bool TEST_MODE = false;

static std::array<int,2> flashVendors = {
    MICRON_VENDOR_ID,
    MACRONIX_VENDOR_ID
};
static int flashVendor = -1;

/**
 * @brief XQSPIPS_Flasher::XQSPIPS_Flasher
 *
 * - Bring mgmt mapping from Flasher object
 */
XQSPIPS_Flasher::XQSPIPS_Flasher(std::shared_ptr<pcidev::pci_device> dev)
{
    std::string err;
    std::string typeStr;

    mDev = dev;
    mTxBytes = 0;
    mRxBytes = 0;

    // maybe initialized QSPI here
    if (typeStr.empty())
        mDev->sysfs_get("flash", "flash_type", err, typeStr);
    if (typeStr.empty())
        mDev->sysfs_get("", "flash_type", err, typeStr);

    // By default, it is 'perallel'
    mConnectMode = 0;
    // U30 Rev.A use single
    if (typeStr.find("single") != std::string::npos) {
        mConnectMode = 1;
    }

    mBusWidth = 4;
    if (typeStr.find("x2") != std::string::npos) {
        // U30 Rev.B use x2
        mBusWidth = 2;
    }

    //std::cout << "minm: mConnectMode " << mConnectMode << " mBusWidth " << mBusWidth << std::endl;
}

/**
 * @brief XQSPIPS_Flasher::~XQSPIPS_Flasher
 *
 * - munmap
 * - delete file descriptor
 */

XQSPIPS_Flasher::~XQSPIPS_Flasher()
{
}

void XQSPIPS_Flasher::clearReadBuffer(unsigned size)
{
    for (unsigned i = 0; i < size; i++) {
        mReadBuffer[i] = 0;
    }
}

void XQSPIPS_Flasher::clearWriteBuffer(unsigned size)
{
    for (unsigned i = 0; i < size; i++) {
        mWriteBuffer[i] = 0;
    }
}

void XQSPIPS_Flasher::clearBuffers(unsigned size)
{
    clearReadBuffer(PAGE_SIZE);
    clearWriteBuffer(PAGE_SIZE);
}

uint32_t XQSPIPS_Flasher::readReg(unsigned RegOffset)
{
    unsigned value;
    int status = mDev->pcieBarRead(FLASH_BASE + RegOffset, &value, 4);
    if(status != 0) {
        assert(0);
        std::cout << "read reg ERROR" << std::endl;
    }
    //std::cout << "Read 0x" << std::hex << RegOffset
    //          << ": 0x" << value << std::dec << std::endl;
    return value;
}

int XQSPIPS_Flasher::writeReg(unsigned RegOffset, unsigned value)
{
    int status = mDev->pcieBarWrite(FLASH_BASE + RegOffset, &value, 4);
    if(status != 0) {
        assert(0);
        std::cout << "write reg ERROR " << std::endl;
    }
    //std::cout << "Write 0x" << std::hex << RegOffset
    //          << ": 0x" << value << std::dec << std::endl;
    return 0;

}

uint32_t XQSPIPS_Flasher::selectSpiMode(uint8_t SpiMode)
{
    uint32_t mask;

    switch (SpiMode) {
        case XQSPIPSU_SELECT_MODE_SPI:
            mask = XQSPIPSU_GENFIFO_MODE_SPI;
            break;
        case XQSPIPSU_SELECT_MODE_DUALSPI:
            mask = XQSPIPSU_GENFIFO_MODE_DUALSPI;
            break;
        case XQSPIPSU_SELECT_MODE_QUADSPI:
            mask = XQSPIPSU_GENFIFO_MODE_QUADSPI;
            break;
        default:
            mask = XQSPIPSU_GENFIFO_MODE_SPI;
    }

#if defined(_DEBUG)
    printHEX("SPI Mode is:", (unsigned)SpiMode);
#endif

    return mask;
}

bool XQSPIPS_Flasher::waitGenFifoEmpty()
{
    long long delay = 0;
    const timespec req = {0, 5000};
    while (delay < 30000000000) {
        uint32_t StatusReg = XQSpiPS_GetStatusReg();
        if (StatusReg & XQSPIPSU_ISR_GENFIFOEMPTY_MASK) {
            return true;
        }
#if defined(_DEBUG)
        printHEX("Gen FIFO Not Empty", StatusReg);
#endif
        nanosleep(&req, 0);
        delay += 5000;
    }
    std::cout << "Unable to get Gen FIFO Empty" << std::endl;
    return false;
}

bool XQSPIPS_Flasher::waitTxEmpty()
{
    long long delay = 0;
    const timespec req = {0, 5000};
    while (delay < 30000000000) {
        uint32_t StatusReg = XQSpiPS_GetStatusReg();
        if (StatusReg & XQSPIPSU_ISR_TXEMPTY_MASK) {
            return true;
        }
#if defined(_DEBUG)
        printHEX("TXD Not Empty", StatusReg);
#endif
        nanosleep(&req, 0);
        delay += 5000;
    }
    std::cout << "Unable to get Tx Empty" << std::endl;
    return false;
}

int XQSPIPS_Flasher::xclUpgradeFirmware(std::istream& binStream)
{
    int total_size = 0;
    int remain = 0;
    int pages = 0;
    unsigned addr = 0;
    unsigned size = 0;
    int beatCount = 0;
    int mismatched = 0;

    binStream.seekg(0, binStream.end);
    total_size = binStream.tellg();
    binStream.seekg(0, binStream.beg);

    std::cout << "INFO: ***BOOT.BIN has " << total_size << " bytes" << std::endl;
    /* Test only */
    //if (xclTestXQSpiPS(0))
    //    return -1;
    //else
    //    return 0;

    initQSpiPS();

    uint32_t StatusReg = XQSpiPS_GetStatusReg();

    if (StatusReg == 0xFFFFFFFF) {
        std::cout << "[ERROR]: Read PCIe device return -1. Cannot get QSPI status." << std::endl;
        exit(-EOPNOTSUPP);
    }

    /* Make sure it is ready to receive commands. */
    resetQSpiPS();
    XQSpiPS_Enable_GQSPI();

    if (!getFlashID()) {
        std::cout << "[ERROR]: Could not get Flash ID" << std::endl;
        exit(-EOPNOTSUPP);
    }

    /* Use 4 bytes address mode */
    enterOrExitFourBytesMode(ENTER_4B);

    // Sectoer size is defined by SECTOR_SIZE
    std::cout << "Erasing flash" << std::flush;
    eraseSector(0, total_size);
    //eraseBulk();
    std::cout << std::endl;

    pages = total_size / PAGE_SIZE;
    remain = total_size % PAGE_SIZE;

#if defined(_DEBUG)
    std::cout << "Verify earse flash" << std::endl;
    for (int page = 0; page <= pages; page++) {
        addr = page * PAGE_SIZE;
        if (page != pages)
            size = PAGE_SIZE;
        else
            size = remain;

        readFlash(addr, size);
        for (unsigned i = 0; i < size; i++) {
            if (0xFF != mReadBuffer[i]) {
                mismatched = 1;
            }
        }

        if (mismatched) {
            std::cout << "Erase failed at page " << page << std::endl;
            mismatched = 0;
        }

    }
#endif

    std::cout << "Programming flash" << std::flush;
    beatCount = 0;
    for (int page = 0; page <= pages; page++) {
        beatCount++;
        if (beatCount % 4000 == 0) {
            std::cout << "." << std::flush;
        }

        addr = page * PAGE_SIZE;
        if (page != pages)
            size = PAGE_SIZE;
        else
            size = remain;

        binStream.read((char *)mWriteBuffer, size);
        writeFlash(addr, size);
    }
    std::cout << std::endl;

    /* Verify (just for debug) */
    binStream.seekg(0, binStream.beg);

#if SAVE_FILE
    std::ofstream of_flash;
    of_flash.open("/tmp/BOOT.BIN", std::ofstream::out);
    if (!of_flash.is_open()) {
        std::cout << "Could not open /tmp/BOOT.BIN" << std::endl;
        return false;
    }
#endif

    remain = total_size % PAGE_SIZE;
    pages = total_size / PAGE_SIZE;

    std::cout << "Verifying" << std::flush;
    beatCount = 0;
    for (int page = 0; page <= pages; page++) {
        beatCount++;
        if (beatCount % 4000 == 0) {
            std::cout << "." << std::flush;
        }

        addr = page * PAGE_SIZE;
        if (page != pages)
            size = PAGE_SIZE;
        else
            size = remain;

        binStream.read((char *)mWriteBuffer, size);

        readFlash(addr, size);

        mismatched = 0;
        for (unsigned i = 0; i < size; i++) {
#if SAVE_FILE
            of_flash << mReadBuffer[i];
#endif
            if (mWriteBuffer[i] != mReadBuffer[i])
                mismatched = 1;
        }
        if (mismatched)
            std::cout << "Find mismatch at page " << page << std::endl;
    }
    std::cout << std::endl;

#if SAVE_FILE
    of_flash.close();
#endif

    enterOrExitFourBytesMode(EXIT_4B);

    return 0;
}

void XQSPIPS_Flasher::initQSpiPS()
{
    /* Should Select GQSPI mode */
    if (!is_GQSPI_Mode()) {
        std::cout << "Not support LQSPI mode, switch to GQSPI mode" << std::endl;
        XQSpiPS_Sel_GQSPI();
    }

    /* Disable GQSPI */
    XQSpiPS_Disable_GQSPI();

    if (TEST_MODE)
        std::cout << "Initialize GQSPI done" << std::endl;
}

void XQSPIPS_Flasher::resetQSpiPS()
{
    uint32_t ConfigReg;

    abortQSpiPS();

    /* Initial Configure register target value is 0x00080010 */
    ConfigReg = XQSpiPS_GetConfigReg();

    ConfigReg &= ~XQSPIPSU_CFG_MODE_EN_MASK; /* IO mode */
    ConfigReg &= ~XQSPIPSU_CFG_GEN_FIFO_START_MODE_MASK; /* Auto start */
    //ConfigReg |= XQSPIPSU_CFG_GEN_FIFO_START_MODE_MASK; /* Manual start */
    ConfigReg &= ~XQSPIPSU_CFG_ENDIAN_MASK; /* Little endain by default */
    ConfigReg &= ~XQSPIPSU_CFG_EN_POLL_TO_MASK; /* Disable poll timeout */
    ConfigReg |= XQSPIPSU_CFG_WP_HOLD_MASK; /* Set hold bit */
    ConfigReg &= ~XQSPIPSU_CFG_BAUD_RATE_DIV_MASK; /* Clear prescalar by default */
    ConfigReg |= CFG_BAUD_RATE_DIV_8; /* Divide by 8 */
    ConfigReg &= ~XQSPIPSU_CFG_CLK_PHA_MASK; /* CPHA 0 */
    ConfigReg &= ~XQSPIPSU_CFG_CLK_POL_MASK; /* CPOL 0 */

    XQSpiPS_SetConfigReg(ConfigReg);

    //XQSpiPS_WriteReg(GQSPI_LPBK_DLY_ADJ_OFFSET, XQSPIPSU_LPBK_DLY_ADJ_USE_LPBK_MASK);
    XQSpiPS_WriteReg(GQSPI_TX_THRESH_OFFSET, XQSPIPSU_TX_FIFO_THRESHOLD_RESET_VAL);
    XQSpiPS_WriteReg(GQSPI_RX_THRESH_OFFSET, XQSPIPSU_RX_FIFO_THRESHOLD_RESET_VAL);
    XQSpiPS_WriteReg(GQSPI_GF_THRESH_OFFSET, XQSPIPSU_GEN_FIFO_THRESHOLD_RESET_VAL);

    if (TEST_MODE) {
        printHEX("CFG Reg:", ConfigReg);
        printHEX("TX Thresh Reg:", XQSpiPS_ReadReg(GQSPI_TX_THRESH_OFFSET));
        printHEX("RX Thresh Reg:", XQSpiPS_ReadReg(GQSPI_RX_THRESH_OFFSET));
        printHEX("GF Thresh Reg:", XQSpiPS_ReadReg(GQSPI_GF_THRESH_OFFSET));
        std::cout << "Reset GQSPI done" << std::endl;
    }
}

void XQSPIPS_Flasher::abortQSpiPS()
{
    uint32_t StatusReg = XQSpiPS_GetStatusReg();
    uint32_t ConfigReg = XQSpiPS_GetConfigReg();

    /* Clear and diable interrupts (Ignore DMA register) */
    XQSpiPS_WriteReg(GQSPI_ISR_OFFSET, StatusReg | XQSPIPSU_ISR_WR_TO_CLR_MASK);
    XQSpiPS_WriteReg(GQSPI_IDR_OFFSET, XQSPIPSU_IDR_ALL_MASK);

    /* Clear FIFO */
    if (XQSpiPS_GetStatusReg() & XQSPIPSU_ISR_RXEMPTY_MASK) {
        XQSpiPS_WriteReg(GQSPI_FIFO_CTRL_OFFSET, XQSPIPSU_FIFO_CTRL_RST_TX_FIFO_MASK | XQSPIPSU_FIFO_CTRL_RST_GEN_FIFO_MASK);

    }

    if (StatusReg & XQSPIPSU_ISR_RXEMPTY_MASK) {
        /* Switch to IO mode to clear RX FIFO */
        ConfigReg &= ~XQSPIPSU_CFG_MODE_EN_MASK; /* IO mode */
        XQSpiPS_SetConfigReg(ConfigReg);
        XQSpiPS_WriteReg(GQSPI_FIFO_CTRL_OFFSET, XQSPIPSU_FIFO_CTRL_RST_RX_FIFO_MASK);
    }

    /* Disable GQSPI */
    XQSpiPS_Disable_GQSPI();

    if (TEST_MODE)
        std::cout << "Abort QSPI done" << std::endl;
}

void XQSPIPS_Flasher::readRxFifo(xqspips_msg_t *msg, int32_t Size)
{
    int32_t Count = 0;
    uint32_t Data = 0;

    assert(msg != NULL);

    while (mRxBytes != 0 && (Count < Size)) {
        Data = XQSpiPS_ReadReg(GQSPI_RXD_OFFSET);
#if defined(_DEBUG)
        printHEX("RX Data:", Data);
#endif
        if (mRxBytes >= 4) {
            memcpy(msg->bufPtr, &Data, 4);
            msg->bufPtr += 4;
            mRxBytes -= 4;
            Count += 4;
        } else {
            /* less than 4 bytes */
            memcpy(msg->bufPtr, &Data, mRxBytes);
            msg->bufPtr += mRxBytes;
            Count += mRxBytes;
            mRxBytes = 0;
        }
    }

}

void XQSPIPS_Flasher::fillTxFifo(xqspips_msg_t *msg, int32_t Size)
{
    int32_t Count = 0;
    uint32_t Data = 0;

    assert(msg != NULL);

    while ((mTxBytes > 0) && (Count < Size)) {
        if (mTxBytes >= 4) {
            memcpy(&Data, msg->bufPtr, 4);
            msg->bufPtr += 4;
            Count += 4;
            mTxBytes -= 4;
        } else {
            /* less than 4 bytes */
            memcpy(&Data, msg->bufPtr, mTxBytes);
            msg->bufPtr += mTxBytes;
            Count += mTxBytes;
            mTxBytes = 0;
        }

        XQSpiPS_WriteReg(GQSPI_TXD_OFFSET, Data);
#if defined(_DEBUG)
            printHEX("TX Data:", Data);
#endif
    }

#if defined(_DEBUG)
        std::cout << "Fill Tx FIFO " << Count << " Bytes." << std::endl;
#endif
}

void XQSPIPS_Flasher::setupTXRX(xqspips_msg_t *msg, uint32_t *GenFifoEntry)
{
    /* Transmit */
    if (msg->flags & XQSPIPSU_MSG_FLAG_TX) {
        *GenFifoEntry |= XQSPIPSU_GENFIFO_DATA_XFER;
        *GenFifoEntry |= XQSPIPSU_GENFIFO_TX;
        *GenFifoEntry &= ~XQSPIPSU_GENFIFO_RX;
        mTxBytes = msg->byteCount;
        mRxBytes = 0;
        fillTxFifo(msg, XQSPIPSU_TXD_DEPTH);
        return;
    }

    /* Receive */
    if (msg->flags & XQSPIPSU_MSG_FLAG_RX) {
        *GenFifoEntry &= ~XQSPIPSU_GENFIFO_TX;
        *GenFifoEntry |= XQSPIPSU_GENFIFO_DATA_XFER;
        *GenFifoEntry |= XQSPIPSU_GENFIFO_RX;
        mRxBytes = msg->byteCount;
        /* Support Rx DMA? */
        return;
    }

    /* dummy */
    if (!(msg->flags & XQSPIPSU_GENFIFO_RX) && !(msg->flags & XQSPIPSU_GENFIFO_TX)) {
        *GenFifoEntry |= XQSPIPSU_GENFIFO_DATA_XFER;
        *GenFifoEntry &= ~(XQSPIPSU_GENFIFO_TX | XQSPIPSU_GENFIFO_RX);
        return;
    }
}

void XQSPIPS_Flasher::sendGenFifoEntryCSAssert()
{
    uint32_t GenFifoEntry = 0x0U;

    GenFifoEntry &= ~(XQSPIPSU_GENFIFO_DATA_XFER | XQSPIPSU_GENFIFO_EXP);
    GenFifoEntry &= ~XQSPIPSU_GENFIFO_MODE_MASK;
    GenFifoEntry |= XQSPIPSU_GENFIFO_MODE_SPI;
    /* By default, use upper and lower CS and Bus */
    GenFifoEntry &= ~XQSPIPSU_GENFIFO_BUS_MASK;
    if (mConnectMode == 0) {
        GenFifoEntry |= XQSPIPSU_GENFIFO_BUS_BOTH;
        GenFifoEntry |= XQSPIPSU_GENFIFO_CS_BOTH;
    } else {
        GenFifoEntry |= XQSPIPSU_GENFIFO_BUS_LOWER;
        GenFifoEntry |= XQSPIPSU_GENFIFO_CS_LOWER;
    }
    GenFifoEntry &= ~(XQSPIPSU_GENFIFO_TX | XQSPIPSU_GENFIFO_RX |
            XQSPIPSU_GENFIFO_STRIPE | XQSPIPSU_GENFIFO_POLL);
    GenFifoEntry |= XQSPIPSU_GENFIFO_CS_SETUP;
    /* Write GEN FIFO */
    XQSpiPS_WriteReg(GQSPI_GEN_FIFO_OFFSET, GenFifoEntry);

#if defined(_DEBUG)
        printHEX("Assert CS: expectd 0xf405, got", GenFifoEntry);
#endif
}

void XQSPIPS_Flasher::sendGenFifoEntryData(xqspips_msg_t *msg)
{
    uint32_t GenFifoEntry = 0x0U;
    uint32_t tmpCount = 0;

    /* Mode SPI/Dual/Quad */
    GenFifoEntry &= ~XQSPIPSU_GENFIFO_MODE_MASK;
    GenFifoEntry |= selectSpiMode(msg->busWidth);

    /* By default, use upper and lower CS and Bus */
    GenFifoEntry &= ~XQSPIPSU_GENFIFO_BUS_MASK;
    if (mConnectMode == 0) {
        GenFifoEntry |= XQSPIPSU_GENFIFO_BUS_BOTH;
        GenFifoEntry |= XQSPIPSU_GENFIFO_CS_BOTH;
    } else {
        GenFifoEntry |= XQSPIPSU_GENFIFO_BUS_LOWER;
        GenFifoEntry |= XQSPIPSU_GENFIFO_CS_LOWER;
    }

    /* Stripe */
    if (msg->flags & XQSPIPSU_MSG_FLAG_STRIPE) {
        GenFifoEntry |= XQSPIPSU_GENFIFO_STRIPE;
    } else {
        GenFifoEntry &= ~XQSPIPSU_GENFIFO_STRIPE;
    }

    /* Do transfer in IO mode */
    XQSpiPS_SetConfigReg(XQSpiPS_GetConfigReg() & ~XQSPIPSU_CFG_MODE_EN_MASK);

    setupTXRX(msg, &GenFifoEntry);

    if (msg->byteCount < XQSPIPSU_GENFIFO_IMM_DATA_MASK) {
        GenFifoEntry &= ~XQSPIPSU_GENFIFO_IMM_DATA_MASK;
        GenFifoEntry |= msg->byteCount;
#if defined(_DEBUG)
        printHEX("GenFifo data:", GenFifoEntry);
#endif
        XQSpiPS_WriteReg(GQSPI_GEN_FIFO_OFFSET, GenFifoEntry);
    } else {
		/* Exponent entries */
        tmpCount = msg->byteCount;
        uint8_t exponent = 8;
        uint8_t immData = tmpCount & 0xFFU;

        GenFifoEntry |= XQSPIPSU_GENFIFO_EXP;
        while (tmpCount != 0x0U) {
            /* only support 1, 2, 4, 8... pages*/
            if (tmpCount & XQSPIPSU_GENFIFO_EXP_START) {
                GenFifoEntry &= ~XQSPIPSU_GENFIFO_IMM_DATA_MASK;
                GenFifoEntry |= exponent;
#if defined(_DEBUG)
                printHEX("GenFifo data:", GenFifoEntry);
#endif
                XQSpiPS_WriteReg(GQSPI_GEN_FIFO_OFFSET, GenFifoEntry);
            }
            tmpCount = tmpCount >> 1;
            exponent++;
        }

        /* Immediate entry */
        GenFifoEntry &= ~XQSPIPSU_GENFIFO_EXP;
        if (immData > 0) {
            GenFifoEntry &= ~XQSPIPSU_GENFIFO_IMM_DATA_MASK;
            GenFifoEntry |= immData;
#if defined(_DEBUG)
            printHEX("GenFifo data:", GenFifoEntry);
#endif
            XQSpiPS_WriteReg(GQSPI_GEN_FIFO_OFFSET, GenFifoEntry);
        }
    }

#if defined(_DEBUG)
        std::cout << "Sent GenFifo Entry Data" << std::endl;
#endif
}

void XQSPIPS_Flasher::sendGenFifoEntryCSDeAssert()
{
    uint32_t GenFifoEntry = 0x0U;

    GenFifoEntry &= ~(XQSPIPSU_GENFIFO_DATA_XFER | XQSPIPSU_GENFIFO_EXP);
    GenFifoEntry &= ~XQSPIPSU_GENFIFO_MODE_MASK;
    //GenFifoEntry |= XQSPIPSU_GENFIFO_MODE_SPI;
    /* By default, use upper and lower CS and Bus */
    GenFifoEntry &= ~XQSPIPSU_GENFIFO_BUS_MASK;
    if (mConnectMode == 0)
        GenFifoEntry |= XQSPIPSU_GENFIFO_BUS_BOTH;
    else
        GenFifoEntry |= XQSPIPSU_GENFIFO_BUS_LOWER;

    GenFifoEntry &= ~(XQSPIPSU_GENFIFO_TX | XQSPIPSU_GENFIFO_RX |
            XQSPIPSU_GENFIFO_STRIPE | XQSPIPSU_GENFIFO_POLL);

    GenFifoEntry |= XQSPIPSU_GENFIFO_CS_HOLD;

    /* Write GEN FIFO */
    XQSpiPS_WriteReg(GQSPI_GEN_FIFO_OFFSET, GenFifoEntry);

#if defined(_DEBUG)
        printHEX("De-Assert CS: expectd 0xc004, got", GenFifoEntry);
#endif
}

/**
 * @brief XQSPIPS_Flasher::finalTransfer
 *
 * This function performs a transfer on the bus in polled mode. The messages passed are all transferred between one CS asser and de-assert.
 *
 * @param   msg is a pointer to the struct containing transfer data
 * @param   numMsg is the number of message to be transferrd.
 *
 * @return
 *      - True Success
 *      - Error code
 *
 */
bool XQSPIPS_Flasher::finalTransfer(xqspips_msg_t *msg, uint32_t numMsg)
{
    uint32_t StatusReg;

    /* Make sure GQSPI is enable */
    XQSpiPS_Enable_GQSPI();
    sendGenFifoEntryCSAssert();

    for (uint32_t Index = 0; Index < numMsg; Index++) {
        /* Only handle one message at a time */
        sendGenFifoEntryData(&msg[Index]);

        do {
            StatusReg = XQSpiPS_GetStatusReg();

            /* Transmit more data if left */
            if (StatusReg & XQSPIPSU_ISR_TXNOT_FULL_MASK &&
                msg[Index].flags & XQSPIPSU_MSG_FLAG_TX &&
                mTxBytes > 0) {
                fillTxFifo(&msg[Index], XQSPIPSU_TXD_DEPTH);
            }

            if (msg[Index].flags & XQSPIPSU_MSG_FLAG_RX) {
                uint32_t RxThr = XQSpiPS_ReadReg(GQSPI_RX_THRESH_OFFSET);

                if (StatusReg & XQSPIPSU_ISR_RXNEMPTY_MASK) {
                    readRxFifo(&msg[Index], RxThr * 4);
                } else {
                    if (StatusReg & XQSPIPSU_ISR_GENFIFOEMPTY_MASK) {
                        readRxFifo(&msg[Index], msg[Index].byteCount);
                    }
                }

            }

            if (!waitGenFifoEmpty() || !waitTxEmpty())
                return false;

        } while (mTxBytes != 0 || mRxBytes != 0);

    }

    sendGenFifoEntryCSDeAssert();

    StatusReg = XQSpiPS_GetStatusReg();
    while (!(StatusReg & XQSPIPSU_ISR_GENFIFOEMPTY_MASK)) {
        StatusReg = XQSpiPS_GetStatusReg();
    }

    XQSpiPS_Disable_GQSPI();

#if defined(_DEBUG)
        std::cout << "Final transfer finished" << std::endl;
#endif
    return true;
}

bool XQSPIPS_Flasher::isFlashReady()
{
    xqspips_msg_t msgFlashStatus[2];
    uint8_t writeCmd = READ_STATUS_CMD;
    uint32_t StatusReg = 0;
    const timespec req = {0, 20000};
    long long delay = 0;

    msgFlashStatus[0].byteCount = 1;
    msgFlashStatus[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgFlashStatus[0].flags = XQSPIPSU_MSG_FLAG_TX;

    msgFlashStatus[1].byteCount = STATUS_READ_BYTES;
    msgFlashStatus[1].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgFlashStatus[1].flags = XQSPIPSU_MSG_FLAG_RX | XQSPIPSU_MSG_FLAG_STRIPE;

    while (delay < 30000000000) {
        msgFlashStatus[0].bufPtr = &writeCmd;
        msgFlashStatus[1].bufPtr = mReadBuffer;
        bool Status = finalTransfer(msgFlashStatus, 2);
        if (!Status) {
            return false;
        }
        StatusReg = mReadBuffer[1] |= mReadBuffer[0];
#if defined(_DEBUG)
        printHEX("Flash ready:", StatusReg);
#endif
        if (!(StatusReg & FLASH_SR_BUSY_MASK)) {
            return true;
        }
        nanosleep(&req, 0);
        delay += 5000;
    }
    std::cout << "Unable to get Flash Ready" << std::endl;
    return false;
}

bool XQSPIPS_Flasher::setWriteEnable()
{
    xqspips_msg_t msgWriteEnable[2];
    uint8_t writeCmd = WRITE_ENABLE_CMD;
    uint32_t StatusReg = XQSpiPS_GetStatusReg();

    if (StatusReg & XQSPIPSU_ISR_TXFULL_MASK) {
        std::cout << "TXD FIFO full during WriteEnable" << std::endl;
        return false;
    }

    msgWriteEnable[0].bufPtr = &writeCmd;
    msgWriteEnable[0].byteCount = 1;
    msgWriteEnable[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgWriteEnable[0].flags = XQSPIPSU_MSG_FLAG_TX;

    if (!finalTransfer(msgWriteEnable, 1))
        return false;

    if (TEST_MODE)
        std::cout << "Set write enable" << std::endl;

    return waitTxEmpty();
}

bool XQSPIPS_Flasher::getFlashID()
{
    xqspips_msg_t msgFlashID[2];
    uint32_t Status;

    if (!isFlashReady())
        return false;

    mWriteBuffer[0] = READ_ID;

    msgFlashID[0].bufPtr = mWriteBuffer;
    msgFlashID[0].byteCount = 1;
    msgFlashID[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgFlashID[0].flags = XQSPIPSU_MSG_FLAG_TX;

    msgFlashID[1].bufPtr = mReadBuffer;
    msgFlashID[1].byteCount = IDCODE_READ_BYTES;
    msgFlashID[1].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgFlashID[1].flags = XQSPIPSU_MSG_FLAG_RX | XQSPIPSU_MSG_FLAG_STRIPE;

    Status = finalTransfer(msgFlashID, 2);
    if ( !Status ) {
        return false;
    }

    if (mConnectMode == 0) {
        // Stripe data: Lower data bus uses even bytes, i.e byte 0, 2, 4, ...
        //              Upper data bus uses odd bytes, i.e byte 1, 3, 5, ...
        if (mReadBuffer[0] != mReadBuffer[1]) {
            std::cout << "Upper Flash chip and lower Flash chip have differe vender id" << std::endl;
            return false;
        }

        if (mReadBuffer[2] != mReadBuffer[3]) {
            std::cout << "Upper Flash chip and lower Flash chip have differe type" << std::endl;
            return false;
        }

        if (mReadBuffer[4] != mReadBuffer[5]) {
            std::cout << "Upper Flash chip and lower Flash chip have differe capacity" << std::endl;
            return false;
        }
    }

    //Update flash vendor
    for (size_t i = 0; i < flashVendors.size(); i++)
        if (mReadBuffer[0] == flashVendors[i])
            flashVendor = flashVendors[i];

    //Update max number of sector. Value of 0x18 is 1 128Mbit sector
    if(mReadBuffer[4] == 0xFF)
        return false;

    for (int i = 0; i < IDCODE_READ_BYTES; i++) {
        std::cout << "Idcode byte[" << i << "]=" << std::hex << (int)mReadBuffer[i] << std::dec << std::endl;
        mReadBuffer[i] = 0;
    }

    return true;
}

bool XQSPIPS_Flasher::eraseSector(unsigned addr, uint32_t byteCount, uint8_t eraseCmd)
{
    xqspips_msg_t msgEraseFlash[1];
    uint8_t writeCmds[5];
    uint32_t realAddr;
    uint32_t Sector;

    if (eraseCmd == 0xff)
        eraseCmd = SEC_4B_ERASE_CMD;

    int beatCount = 0;
    for (Sector = 0; Sector < ((byteCount / SECTOR_SIZE) + 2); Sector++) {

        if(!isFlashReady())
            return false;

        beatCount++;
        if (beatCount % 64 == 0) {
            std::cout << "." << std::flush;
        }

        if (mConnectMode == 0)
            realAddr = addr / 2;
        else
            realAddr = addr;

        if(!setWriteEnable())
            return false;

        writeCmds[0] = eraseCmd;
        writeCmds[1] = (uint8_t)((realAddr & 0xFF000000) >> 24);
        writeCmds[2] = (uint8_t)((realAddr & 0xFF0000) >> 16);
        writeCmds[3] = (uint8_t)((realAddr & 0xFF00) >> 8);
        writeCmds[4] = (uint8_t)(realAddr & 0xFF);

        msgEraseFlash[0].bufPtr = writeCmds;
        msgEraseFlash[0].byteCount = 5;
        msgEraseFlash[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
        msgEraseFlash[0].flags = XQSPIPSU_MSG_FLAG_TX;

        if (!finalTransfer(msgEraseFlash, 1))
            return false;

        addr += SECTOR_SIZE;
    }

    if (TEST_MODE)
        std::cout << "Erase Flash done " << byteCount << " bytes" << std::endl;

    return true;
}

bool XQSPIPS_Flasher::eraseBulk()
{
    xqspips_msg_t msgEraseFlash[1];
    uint8_t writeCmds[5];
    uint8_t eraseCmd;

    if(!isFlashReady())
        return false;

    eraseCmd = BULK_ERASE_CMD;

    if(!setWriteEnable())
        return false;

    writeCmds[0] = eraseCmd;

    msgEraseFlash[0].bufPtr = writeCmds;
    msgEraseFlash[0].byteCount = 1;
    msgEraseFlash[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgEraseFlash[0].flags = XQSPIPSU_MSG_FLAG_TX;

    if (!finalTransfer(msgEraseFlash, 1))
        return false;

    if(!isFlashReady())
        return false;

    return true;
}

bool XQSPIPS_Flasher::readFlash(unsigned addr, uint32_t byteCount, uint8_t readCmd)
{
    xqspips_msg_t msgReadFlash[3];
    uint8_t writeCmds[5];
    uint32_t realAddr;
    uint32_t commandBytes;
    uint32_t msgCnt;

    if (!isFlashReady())
        return false;

    if (mConnectMode == 0)
        realAddr = addr / 2;
    else
        realAddr = addr;

    if (readCmd == 0xff)
        readCmd = QUAD_READ_CMD;

    writeCmds[0] = readCmd;
    writeCmds[1] = (uint8_t)((realAddr & 0xFF000000) >> 24);
    writeCmds[2] = (uint8_t)((realAddr & 0xFF0000) >> 16);
    writeCmds[3] = (uint8_t)((realAddr & 0xFF00) >> 8);
    writeCmds[4] = (uint8_t)(realAddr & 0xFF);
    commandBytes = 5;

    msgReadFlash[0].bufPtr = writeCmds;
    msgReadFlash[0].byteCount = commandBytes;
    msgReadFlash[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgReadFlash[0].flags = XQSPIPSU_MSG_FLAG_TX;

    msgCnt = 1;
    /* Dummy clock */
    if (readCmd == QUAD_READ_CMD) {
        msgReadFlash[msgCnt].bufPtr = NULL;
        msgReadFlash[msgCnt].byteCount = 8;
        msgReadFlash[msgCnt].busWidth = XQSPIPSU_SELECT_MODE_SPI;
        msgReadFlash[msgCnt].flags = 0;
        msgCnt++;
    }

    msgReadFlash[msgCnt].bufPtr = mReadBuffer;
    msgReadFlash[msgCnt].byteCount = byteCount;
    msgReadFlash[msgCnt].busWidth = XQSPIPSU_SELECT_MODE_QUADSPI;
    msgReadFlash[msgCnt].flags = XQSPIPSU_MSG_FLAG_RX | XQSPIPSU_MSG_FLAG_STRIPE;
    msgCnt++;

    if (!finalTransfer(msgReadFlash, msgCnt))
        return false;

    if (TEST_MODE)
        std::cout << "Read Flash done " << byteCount << " bytes" << std::endl;

    return true;
}

bool XQSPIPS_Flasher::writeFlash(unsigned addr, uint32_t byteCount, uint8_t writeCmd)
{
    xqspips_msg_t msgWriteFlash[3];
    uint8_t writeCmds[5];
    uint32_t realAddr;
    uint32_t commandBytes;

    if(!isFlashReady())
        return false;

    if (mConnectMode == 0)
        realAddr = addr / 2;
    else
        realAddr = addr;

    if (!setWriteEnable())
        return false;

    if(!isFlashReady())
        return false;

    if (writeCmd == 0xff) {
        writeCmd = QUAD_WRITE_CMD;
    }

    writeCmds[0] = writeCmd;
    writeCmds[1] = (uint8_t)((realAddr & 0xFF000000) >> 24);
    writeCmds[2] = (uint8_t)((realAddr & 0xFF0000) >> 16);
    writeCmds[3] = (uint8_t)((realAddr & 0xFF00) >> 8);
    writeCmds[4] = (uint8_t)(realAddr & 0xFF);
    commandBytes = 5;

    msgWriteFlash[0].bufPtr = writeCmds;
    msgWriteFlash[0].byteCount = commandBytes;
    msgWriteFlash[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgWriteFlash[0].flags = XQSPIPSU_MSG_FLAG_TX;

    /* The data to write is already filled up */
    msgWriteFlash[1].bufPtr = mWriteBuffer;
    msgWriteFlash[1].byteCount = byteCount;
    msgWriteFlash[1].busWidth = XQSPIPSU_SELECT_MODE_QUADSPI;
    msgWriteFlash[1].flags = XQSPIPSU_MSG_FLAG_TX | XQSPIPSU_MSG_FLAG_STRIPE;

    if (!finalTransfer(msgWriteFlash, 2))
        return false;

    if (TEST_MODE)
        std::cout << "Write Flash done " << byteCount << " bytes" << std::endl;

    return true;
}

bool XQSPIPS_Flasher::enterOrExitFourBytesMode(uint32_t enable)
{
    uint8_t cmd;

    if (enable) {
        cmd = ENTER_4B_ADDR_MODE;
    } else {
        cmd = EXIT_4B_ADDR_MODE;
    }

    writeFlashReg(cmd, 0, 0);

    if (!isFlashReady())
        return false;

    if (TEST_MODE)
        std::cout << "Four Bytes Mode " << enable << std::endl;

    return true;
}

bool XQSPIPS_Flasher::readFlashReg(unsigned commandCode, unsigned bytes)
{
    xqspips_msg_t msgToFlash[2];
    bool Status = false;

    if (!isFlashReady())
        return false;

    mWriteBuffer[0] = commandCode;

    msgToFlash[0].bufPtr = mWriteBuffer;
    msgToFlash[0].byteCount = 1;
    msgToFlash[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgToFlash[0].flags = XQSPIPSU_MSG_FLAG_TX;

    msgToFlash[1].bufPtr = mReadBuffer;
    msgToFlash[1].byteCount = bytes;
    msgToFlash[1].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgToFlash[1].flags = XQSPIPSU_MSG_FLAG_RX | XQSPIPSU_MSG_FLAG_STRIPE;

    Status = finalTransfer(msgToFlash, 2);
    if ( !Status ) {
        return false;
    }

#if defined(_DEBUG)
    std::cout << "Printing output (with some extra bytes of readFlashReg cmd)" << std::endl;
#endif

    for(unsigned i = 0; i < bytes; ++ i) //Some extra bytes, no harm
    {
#if defined(_DEBUG)
        std::cout << i << " " << std::hex << (int)mReadBuffer[i] << std::dec << std::endl;
#endif
        mReadBuffer[i] = 0; //clear
    }

    return Status;
}

bool XQSPIPS_Flasher::writeFlashReg(unsigned commandCode, unsigned value, unsigned bytes)
{
    bool Status = false;
    xqspips_msg_t msgWriteFlash[1];

    if (!setWriteEnable())
        return false;

    mWriteBuffer[0] = commandCode;

    switch (bytes) {
        case 0:
            break;
        case 1:
            mWriteBuffer[1] = (uint8_t) (value);
            break;
        case 2:
            mWriteBuffer[1] = (uint8_t) (value >> 8);
            mWriteBuffer[2] = (uint8_t) (value);
            break;
        default:
            std::cout << "ERROR: Setting more than 2 bytes" << std::endl;
            assert(0);
    }

    msgWriteFlash[0].bufPtr = mWriteBuffer;
    msgWriteFlash[0].byteCount = 1 + bytes;
    msgWriteFlash[0].busWidth = XQSPIPSU_SELECT_MODE_SPI;
    msgWriteFlash[0].flags = XQSPIPSU_MSG_FLAG_TX;

    Status = finalTransfer(msgWriteFlash, 1);
    if (!Status)
        return false;

    if (!waitTxEmpty())
        return false;

    return Status;
}

int XQSPIPS_Flasher::xclTestXQSpiPS(int index)
{
    TEST_MODE = false;

    std::cout << ">>> Test XQSpiPS engine <<<" << std::endl;
    initQSpiPS();

    /* print the IP (not of flash) control/status register. */
    uint32_t ConfigReg = XQSpiPS_GetConfigReg();
    uint32_t StatusReg = XQSpiPS_GetStatusReg();
    std::cout << "PS GQSPI Config/Status " << std::hex << ConfigReg << "/" << StatusReg << std::dec << std::endl;

    /* Make sure it is ready to receive commands. */
    resetQSpiPS();

    XQSpiPS_Enable_GQSPI();
    printHEX("GQSPI enable:", XQSpiPS_ReadReg(GQSPI_EN_OFFSET));

    /* 1. idcode read */
    std::cout << ">>> Testing read Flash ID" << std::endl;
    if (!getFlashID()) {
        std::cout << "[ERROR]: Could not get Flash ID" << std::endl;
        exit(-EOPNOTSUPP);
    }

    std::cout << "id code successful (please verify the idcode output too)" << std::endl;
    std::cout << ">>> Now reading various flash registers <<<" << std::endl;

    /* 2. register read */
    std::cout << "Testing READ_STATUS_CMD" << std::endl;
    uint8_t Cmd = READ_STATUS_CMD;
    readFlashReg(Cmd, STATUS_READ_BYTES);

    std::cout << "Testing READ_FLAG_STATUS_CMD" << std::endl;
    Cmd = READ_FLAG_STATUS_CMD;
    readFlashReg(Cmd, STATUS_READ_BYTES);

    std::cout << "Testing EXTADD_REG_RD" << std::endl;
    Cmd = EXTADD_REG_RD;
    readFlashReg(Cmd, STATUS_READ_BYTES);

    /* 3. Testing simple read and write */
    enterOrExitFourBytesMode(ENTER_4B);

    std::cout << ">>> Testing simple read and write <<<" << std::endl;
    unsigned addr = 0;
    unsigned size = 0;
    // Write/Read 16K + 100 bytes
    //int total_size = 16 * 1024 + 100;
    int total_size = 300;

    int remain = total_size % PAGE_SIZE;
    int pages = total_size / PAGE_SIZE;

    std::cout << "Write " << total_size << " bytes" << std::endl;

    std::cout << "earse flash" << std::endl;
    eraseSector(0, total_size);
    //eraseBulk();

    std::cout << ">>>>>> Write " << std::endl;
    for (int page = 0; page <= pages; page++) {
        addr = page * PAGE_SIZE;
        if (page != pages)
            size = PAGE_SIZE;
        else
            size = remain;

        for (unsigned index = 0; index < size; index++) {
            mWriteBuffer[index] = (uint8_t)index;
        }

        writeFlash(addr, size);
    }

    remain = total_size % 256;
    pages = total_size / 256;

    std::cout << ">>>>>> Verify data" << std::endl;
    for (int page = 0; page <= pages; page++) {
        addr = page * 256;
        if (page != pages)
            size = 256;
        else
            size = remain;

        readFlash(addr, size);

        // Verify
        for (unsigned i = 0; i < size; i++) {
            std::cout << i << " 0x" << std::hex << (int)mReadBuffer[i] << std::dec << std::endl;
            if (mReadBuffer[i] != (i % PAGE_SIZE)) {
                std::cout << "Found mismatch" << std::endl;
                return -1;
            }
        }
    }
    std::cout << ">>>>>> " << total_size << " bytes data correct!" << std::endl;

    enterOrExitFourBytesMode(EXIT_4B);

    std::cout << ">>> Test Passed <<<" << std::endl;
    return 0;
}
