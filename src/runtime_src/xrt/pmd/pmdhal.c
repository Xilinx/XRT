/**
 * Copyright (C) 2016 Xilinx, Inc
 * Author: Sonal Santan
 * DPDK wrapper implementation of essential DPDK functions needed for a bsic intergartion with XRT.
 *
 * NOTES:
 * ------
 * 1. xNIC DPDK fails build of shared library with CONFIG_RTE_BUILD_SHARED_LIB=y
 *
 * 2. This wrapper links with DPDK static libraries to produce a shared library called pmd.so
 *
 * 3. This wrapper cannot be built with rdi since the library requires newer system calls not
 *    available in RHEL 5.X build machines
 *
 * 4. The wrapper helps to decouple XRT from DPDK
 *
 * 5. Need to bring in the concept of device here which is represented by m_port. Today there is
 *    only one device with m_port == 0.
 *
 * 6. Compile this with DPDK static objects to create pmd.so shared library. Use the command like
 *    below:
 *    gcc -g -Wall -fvisibility=hidden -march=core2 -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_COMPILE_TIME_CPUFLAGS=RTE_CPUFLAG_SSE,RTE_CPUFLAG_SSE2,RTE_CPUFLAG_SSE3,RTE_CPUFLAG_SSSE3 -fPIC -include $RTE_SDK/build/include/rte_config.h -I /mnt/sonals/development/RDI_sonals_picasso3/HEAD/src/products/sdaccel/src/runtime_src/ -I $RTE_SDK/build/include pmdhal.c -shared -o pmd.so -L $RTE_SDK/build/lib -lrte_mbuf -lrte_eal -lrte_pmd_bond -lethdev -lrte_mempool -lrte_ring -Wl,--whole-archive -lrte_pmd_xnic -Wl,-no-whole-archive
 *
 * 7. Software Stack Layering (This may change going forward)
 *    OCL  API
 *    --------
 *      XRT
 *    --------
 *    PMD  HAL
 *    --------
 *      DPDK
 */


#include <rte_eal.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_errno.h>

#include "pmdhal.h"

#include <string.h>

#define NUM_MBUFS 8191
#define MBUF_SIZE (1600 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 250
#define MAX_RECV_Q 1
#define MAX_SEND_Q 1

static unsigned char m_port;
static unsigned short m_recv_q;
static unsigned short m_send_q;
static PacketObjectPool m_po_pool;

/* Copy paste XCLHAL device info here for now. Later we would like to redefine
 * this or include xclhal.h directly */

struct xclDeviceInfo2 {
    unsigned mMagic; // = 0X586C0C6C; XL OpenCL X->58(ASCII), L->6C(ASCII), O->0 C->C L->6C(ASCII);
    char mName[256];
    unsigned short mHALMajorVersion;
    unsigned short mHALMinorVersion;
    unsigned short mVendorId;
    unsigned short mDeviceId;
    unsigned mDeviceVersion;
    unsigned short mSubsystemId;
    unsigned short mSubsystemVendorId;
    size_t mDDRSize;                    // Size of DDR memory
    size_t mDataAlignment;              // Minimum data alignment requirement for host buffers
    size_t mDDRFreeSize;                // Total unused/available DDR memory
    size_t mMinTransferSize;            // Minimum DMA buffer size
    float mTemp;
    float mVoltage;
    float mCurrent;
    unsigned mDDRBankCount;
    unsigned mOCLFrequency;
    unsigned mPCIeLinkWidth;
    unsigned mPCIeLinkSpeed;
    unsigned short mDMAThreads;
    // More properties here
};


unsigned pmdProbe(int argc, char *argv[])
{
  m_port = 0xff;
  m_recv_q = 0xffff;
  m_send_q = 0xffff;

  int ret = rte_eal_init(argc, argv);
  if (ret)
    return ret;
  //throw std::runtime_error("Error with EAL initialization");

  unsigned count = rte_eth_dev_count();
  if (count == 0)
    return 0xffffffff;
    //    throw std::runtime_error("Unable to find any stream capable ports");

  m_po_pool = rte_mempool_create("MBUF_POOL",
                                 NUM_MBUFS * 1,
                                 MBUF_SIZE,
                                 MBUF_CACHE_SIZE,
                                 sizeof(struct rte_pktmbuf_pool_private),
                                 rte_pktmbuf_pool_init, NULL,
                                 rte_pktmbuf_init,      NULL,
                                 rte_socket_id(),
                                 0);

  if (!m_po_pool)
    return 0xffff;
  return count;
}

unsigned pmdOpen(unsigned port)
{
  struct rte_eth_conf port_conf;
  memset(&port_conf, 0, sizeof(port_conf));
  port_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;
  m_port = port;
  return rte_eth_dev_configure(port, MAX_RECV_Q, MAX_SEND_Q, &port_conf);
}

unsigned pmdGetDeviceInfo(unsigned port, struct xclDeviceInfo2 *info)
{
  /* TODO: Get the device specific data using DPDK APIs */
  memset(info, 0, sizeof(struct xclDeviceInfo2));
  strcpy(info->mName, "xilinx:adm-pcie-ku3:xNIC:1.0");
  info->mHALMajorVersion = 1;
  info->mHALMinorVersion = 0;
  info->mVendorId = 0x10ee;
  info->mDeviceId = 0x8038;
  info->mDeviceVersion = 1;
  info->mSubsystemId = 1;
  info->mSubsystemVendorId = 1;
  info->mDMAThreads = 1;
  return 0;
}

StreamHandle pmdOpenStream(unsigned port, unsigned depth, unsigned dir)
{
  int result;
  StreamHandle handle;
  const char *d;
  if (dir == 1) {
    result = rte_eth_rx_queue_setup(m_port, ++m_recv_q, depth, SOCKET_ID_ANY, 0, m_po_pool);
    handle = m_recv_q;
    d = "receive";
  }
  else {
    result = rte_eth_tx_queue_setup(m_port, ++m_send_q, depth, SOCKET_ID_ANY, 0);
    handle = m_send_q;
    d = "transmit";
  }
  if (result) {
    return 0xFFFF;
    //    throw xrt::error(result, "Unable to open " + d + " stream on port "
    //                     + std::to_string(m_port) + "with depth " + std::to_string(depth));
  }
  return handle;
}

void pmdCloseStream(unsigned port, StreamHandle strm)
{

}

unsigned pmdSendPkts(unsigned port, StreamHandle strm, PacketObject *pkts, unsigned count)
{
  return rte_eth_tx_burst(m_port, (unsigned short)strm, pkts, (unsigned short)count);
}

unsigned pmdRecvPkts(unsigned port, StreamHandle strm, PacketObject *pkts, unsigned count)
{
  return rte_eth_rx_burst(m_port, (unsigned short)strm, pkts, (unsigned short)count);
}

PacketObject pmdAcquirePkts(unsigned port)
{
  return rte_pktmbuf_alloc(m_po_pool);
}

void pmdReleasePkts(unsigned port, PacketObject pkt)
{
  rte_pktmbuf_free(pkt);
}


