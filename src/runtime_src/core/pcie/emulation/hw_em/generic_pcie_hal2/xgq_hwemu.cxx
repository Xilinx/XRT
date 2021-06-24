/*
 *  Copyright (C) 2021, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.  This program is
 *  distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 *  License for more details.  You should have received a copy of the
 *  GNU General Public License along with this program; if not, write
 *  to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 *
 */

#include "shim.h"
#include "xgq_cmd.h"

using namespace xclhwemhal2;

namespace hwemu {

  void xgq_hwemu_mem_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
  {
    auto device = reinterpret_cast<xclhwemhal2::HwEmShim *>(io_hdl);
    device->xclWrite(XCL_ADDR_SPACE_DEVICE_RAM, addr, (void*)(&val), 4);
  }

  uint32_t xgq_hwemu_mem_read32(uint64_t io_hdl, uint64_t addr)
  {
    uint32_t value;
    auto device = reinterpret_cast<xclhwemhal2::HwEmShim *>(io_hdl);
    device->xclRead(XCL_ADDR_SPACE_DEVICE_RAM, addr, (void*)(&value), 4);
    return value;
  }

  void xgq_hwemu_reg_write32(uint64_t io_hdl, uint64_t addr, uint32_t val)
  {
    auto device = reinterpret_cast<xclhwemhal2::HwEmShim *>(io_hdl);
    device->xclWrite(XCL_ADDR_KERNEL_CTRL, addr, (void*)(&val), 4);
  }

  uint32_t xgq_hwemu_reg_read32(uint64_t io_hdl, uint64_t addr)
  {
    uint32_t value;
    auto device = reinterpret_cast<xclhwemhal2::HwEmShim *>(io_hdl);
    device->xclRead(XCL_ADDR_KERNEL_CTRL, addr, (void *)(&value), 4);
    return value;
  }

  xgq_queue::xgq_queue(HwEmShim* in_dev, xocl_xgq* in_xgqp, uint16_t in_nslot, uint32_t in_slot_size, uint64_t in_xgq_sub_base, uint64_t in_xgq_com_base)
    : device(in_dev)
    , xgqp(in_xgqp)
    , nslot(in_nslot)
    , slot_size(in_slot_size)
    , xgq_sub_base(in_xgq_sub_base)
    , xgq_com_base(in_xgq_com_base)
  {
    stop = false;
    qid = 0;

    sub_thread = new std::thread(&xgq_queue::submit_worker, this);
    com_thread = new std::thread(&xgq_queue::complete_worker, this);

    size_t ring_len = XRT_QUEUE1_RING_LENGTH;
    auto devp = reinterpret_cast<uint64_t>(device);
    xgq_alloc(&queue, false, devp, XRT_QUEUE1_RING_BASE, &ring_len, slot_size, xgq_sub_base, xgq_com_base);
  }

  xgq_queue::~xgq_queue()
  {
    stop = true;

    if (sub_thread->joinable()) {
      sub_thread->join();
    }

    if (com_thread->joinable()) {
      com_thread->join();
    }
  }

  void xgq_queue::iowrite32_ctrl(uint32_t addr, uint32_t data)
  {
    device->xclWrite(XCL_ADDR_KERNEL_CTRL, addr, (void*)(&data), 4);
  }

  uint32_t xgq_queue::ioread32_ctrl(uint32_t addr)
  {
    uint32_t value;
    device->xclRead(XCL_ADDR_KERNEL_CTRL, addr, (void*)(&value), 4);
    return value;
  }

  void xgq_queue::iowrite32_mem(uint32_t addr, uint32_t data)
  {
    device->xclWrite(XCL_ADDR_SPACE_DEVICE_RAM, addr, (void*)(&data), 4);
  }

  uint32_t xgq_queue::ioread32_mem(uint32_t addr)
  {
    uint32_t value;
    device->xclRead(XCL_ADDR_SPACE_DEVICE_RAM, addr, (void*)(&value), 4);
    return value;
  }

  int xgq_queue::submit_cmd(xgq_cmd *xcmd)
  {
    if (xcmd->xcmd_size() > slot_size)
      return -EINVAL;

    uint64_t slot_addr = 0;
    int rval = xgq_produce(&queue, &slot_addr);
    if (rval)
      return rval;

    for (int i = xcmd->sq_buf.size() - 1; i >= 0; i--)
      iowrite32_mem(slot_addr + i * 4, xcmd->sq_buf.at(i));

    return 0;
  }

  void xgq_queue::read_completion(xrt_com_queue_entry& ccmd, uint64_t addr)
  {
    for (uint32_t i = 0; i < XRT_COM_Q1_SLOT_SIZE / 4; i++)
      ccmd.data[i] = ioread32_mem(addr + i * 4);

    // Write 0 to first word to make sure the cmd state is not NEW
    iowrite32_mem(addr, 0);
  }

  // Update XGQ submission queue tail pointer.
  // The sub_tail is a 32 bits integer that points to the first empty slot
  // in the queue. The consumer should mask this value with (slot_size - 1)
  // and handle new slots to right before this slot.
  void xgq_queue::update_doorbell()
  {
    xgq_notify_peer_produced(&queue);
  }

  int xgq_queue::submit_worker()
  {
    while (!this->stop) {
      std::unique_lock<std::mutex> lck(queue_mutex);
      sub_cv.wait(lck, [this] { return !pending_cmds.empty(); });

      for (auto& xcmd : pending_cmds) {
        // TODO Handle submission queue full
        int rval = submit_cmd(xcmd);
        if (rval) {
          std::cout << "Error: fail to submit command " << xcmd->cmdid << ": rval is " << rval << std::endl;
          break;
        }
        submitted_cmds[xcmd->cmdid] = xcmd;
        std::cout << "Info: command " << xcmd->cmdid << " submitted." << std::endl;
      }
      update_doorbell();
      pending_cmds.clear();
      com_cv.notify_all();
    }

    return 0;
  }

  int xgq_queue::complete_worker()
  {
    while (!this->stop) {
      std::unique_lock<std::mutex> lck(queue_mutex);
      com_cv.wait(lck, [this] { return !submitted_cmds.empty(); });

      while (!submitted_cmds.empty()) {
        uint64_t slot_addr = 0;
        int rval = xgq_consume(&queue, &slot_addr);
        if (rval)
          continue;

        xrt_com_queue_entry ccmd;
        read_completion(ccmd, slot_addr);

        auto scmd = submitted_cmds[ccmd.cid];
        if (scmd == nullptr) {
          std::cout << "Error: completion command not found." << std::endl;
          return -ENODEV;
        } else
          std::cout << "Info: command " << scmd->cmdid << " completed." << std::endl;

        if (scmd->is_ertpkt()) {
          if (ccmd.cstate == XRT_CMD_STATE_COMPLETED)
            scmd->set_state(ERT_CMD_STATE_COMPLETED);
          else
            scmd->set_state(ERT_CMD_STATE_ERROR);
          xgqp->cmd_pool.destroy(scmd);
        } else {
          scmd->rval = ccmd.cstate == XRT_CMD_STATE_COMPLETED ? 0 : -1;
          // Notify command completion
          scmd->cmd_cv.notify_all();
        }

        submitted_cmds.erase(ccmd.cid);

        xgq_notify_peer_consumed(&queue);
      }
    }

    return 0;
  }

  //! initialize static variable, which gives unique ID for each cmd
  uint64_t xgq_cmd::next_uid = 0;

  xgq_cmd::xgq_cmd()
  {
    cmdid = ++next_uid; //! Assign unique ID for each new command
    ert_pkt = nullptr;
    rval = 0;
  }

  bool xgq_cmd::is_ertpkt()
  {
    return (!(ert_pkt == nullptr));
  }

  uint32_t xgq_cmd::opcode()
  {
    return ert_pkt->opcode;
  }

  void xgq_cmd::set_state(enum ert_cmd_state state)
  {
    ert_pkt->state = state;
  }

  uint32_t xgq_cmd::payload_size()
  {
    return ert_pkt->count * sizeof(uint32_t);
  }

  uint32_t xgq_cmd::xcmd_size()
  {
    return sq_buf.size() * sizeof(uint32_t);
  }

  int xgq_cmd::convert_bo(xclemulation::drm_xocl_bo *bo)
  {
    this->ert_pkt = (struct ert_packet *)bo->buf;

    switch (opcode()) {
      case ERT_CONFIGURE:
        {
          sq_buf.resize((payload_size() + XGQ_SUB_HEADER_SIZE) / sizeof(uint32_t));
          auto *cmdp = reinterpret_cast<xrt_cmd_configure *>(sq_buf.data());

          cmdp->opcode = XRT_CMD_OP_CONFIGURE;
          cmdp->state = 1;
          cmdp->cid = cmdid;
          cmdp->count = payload_size();
          memcpy(cmdp->data, this->ert_pkt->data, payload_size());
        }
        break;

      case ERT_START_CU:
        {
          sq_buf.resize((payload_size() + XGQ_SUB_HEADER_SIZE) / sizeof(uint32_t));
          auto *cmdp = reinterpret_cast<xrt_cmd_start_cuidx *>(sq_buf.data());

          cmdp->opcode = XRT_CMD_OP_START_PL_CUIDX;
          cmdp->state = 1;
          cmdp->cid = cmdid;
          cmdp->count = payload_size();
          cmdp->cu_idx = 0;
          auto ert_start_cu = reinterpret_cast<ert_start_kernel_cmd *>(ert_pkt);
          memcpy(cmdp->data, ert_start_cu->data, payload_size() - 4);
        }
        break;

      case ERT_EXIT:
        {
          sq_buf.resize((payload_size() + XGQ_SUB_HEADER_SIZE) / sizeof(uint32_t));
          auto *cmdp = reinterpret_cast<xrt_cmd_exit_ert*>(sq_buf.data());

          cmdp->opcode = XRT_CMD_OP_EXIT_ERT;
          cmdp->state = 1;
          cmdp->cid = cmdid;
          cmdp->count = payload_size();
        }
        break;

      default:
        std::cout << "Error: Unknown command." << std::endl;
        return -EINVAL;
    }

    return 0;
  }

  int xgq_cmd::load_xclbin(xrt::bo& xbo, char *buf, size_t size)
  {
    auto data = xbo.map();
    memcpy(data, buf, size);
    auto paddr = static_cast<uint32_t>(xbo.address());

    sq_buf.resize(sizeof(xrt_cmd_load_xclbin));
    auto *cmdp = reinterpret_cast<xrt_cmd_load_xclbin *>(sq_buf.data());

    cmdp->opcode = XRT_CMD_OP_LOAD_XCLBIN;
    cmdp->state = 1;
    cmdp->cid = cmdid;
    cmdp->count = sizeof(xrt_cmd_load_xclbin) - XGQ_SUB_HEADER_SIZE;
    cmdp->address = paddr;
    cmdp->size = size;
    cmdp->addr_type = XRT_CMD_ADD_TYPE_SLAVEBRIDGE;

    return 0;
  }

  xocl_xgq::xocl_xgq(HwEmShim* dev)
    : queue(dev, this, XRT_QUEUE1_SLOT_NUM, XRT_SUB_Q1_SLOT_SIZE, XRT_XGQ_SUB_BASE, XRT_XGQ_COM_BASE)
  {
    device = dev;
  }

  xocl_xgq::~xocl_xgq()
  {
  }

  int xocl_xgq::add_exec_buffer(xclemulation::drm_xocl_bo *buf)
  {
    xgq_cmd *xcmd = cmd_pool.construct();
    if (!xcmd)
      return 1;

    if (xcmd->convert_bo(buf))
      return 1;

    {
      std::lock_guard<std::mutex> lk(queue.queue_mutex);
      queue.pending_cmds.push_back(xcmd);
    }
    queue.sub_cv.notify_all();

    return 0;
  }

  int xocl_xgq::load_xclbin(char *buf, size_t size)
  {
    xrt::device xdev(device->getMCoreDevice());
    xrt::bo xbo(xdev, size, xrt::bo::flags::host_only, 0);

    xgq_cmd *xcmd = cmd_pool.construct();
    if (!xcmd)
      return 1;

    if (xcmd->load_xclbin(xbo, buf, size))
      return 1;

    {
      std::lock_guard<std::mutex> lk(queue.queue_mutex);
      queue.pending_cmds.push_back(xcmd);
    }
    queue.sub_cv.notify_all();

    {
      // Wait for command completion
      std::unique_lock<std::mutex> lk(xcmd->cmd_mutex);
      xcmd->cmd_cv.wait(lk);
    }

    int rval = xcmd->rval;
    cmd_pool.destroy(xcmd);

    return rval;
  }

} // namespace hwemu
