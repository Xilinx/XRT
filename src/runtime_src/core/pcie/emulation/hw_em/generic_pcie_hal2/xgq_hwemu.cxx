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
#include "xgq.h"

using namespace xclhwemhal2;

namespace hwemu {

  xgq_queue::xgq_queue(HwEmShim* in_dev, xocl_xgq* in_xgqp, uint16_t in_nslot, uint32_t in_slot_size, uint64_t in_xgq_sub_base, uint64_t in_xgq_com_base, uint64_t in_sub_base, uint64_t in_com_base)
    : device(in_dev)
    , xgqp(in_xgqp)
    , nslot(in_nslot)
    , slot_size(in_slot_size)
    , xgq_sub_base(in_xgq_sub_base)
    , xgq_com_base(in_xgq_com_base)
    , sub_base(in_sub_base)
    , com_base(in_com_base)
  {
    stop = false;
    sub_head = 0;
    sub_tail = 1;
    com_head = 0;
    com_tail = 0;
    qid = 0;

    sub_thread = new std::thread(&xgq_queue::submit_worker, this);
    com_thread = new std::thread(&xgq_queue::complete_worker, this);
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

    uint64_t addr = sub_base + (sub_tail & XRT_QUEUE1_SLOT_MASK) * slot_size;
    for (int i = xcmd->sq_buf.size() - 1; i >= 0; i--)
      iowrite32_mem(addr + i * 4, xcmd->sq_buf.at(i));

    sub_tail++;

    return 0;
  }

  void xgq_queue::read_completion(xrt_com_queue_entry& ccmd, uint32_t tail)
  {
    auto slot = tail & XRT_QUEUE1_SLOT_MASK;
    for (uint32_t i = 0; i < XRT_COM_Q1_SLOT_SIZE / 4; i++)
      ccmd.data[i] = ioread32_mem(com_base + slot * XRT_COM_Q1_SLOT_SIZE + i * 4);
  }

  void xgq_queue::update_doorbell()
  {
    iowrite32_ctrl(xgq_sub_base, sub_tail - 1);
  }

  void xgq_queue::clear_sub_slot_state(uint64_t sub_slot)
  {
    iowrite32_mem(sub_base + sub_slot, 0);
  }

  uint16_t xgq_queue::check_doorbell()
  {
    while (1) {
      uint32_t data = ioread32_ctrl(xgq_com_base);
      if (data != com_tail)
        return data;
    }
  }

  int xgq_queue::submit_worker()
  {
    while (!this->stop) {
      std::unique_lock<std::mutex> lck(queue_mutex);
      sub_cv.wait(lck, [this] { return !pending_cmds.empty(); });

      for (auto& xcmd : pending_cmds) {
        // TODO Handle submission queue full
        submit_cmd(xcmd);
        submitted_cmds[xcmd->cmdid] = xcmd;
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
        auto tail = check_doorbell();
        uint32_t slot = com_tail;
        for (;;) {
          slot++;

          xrt_com_queue_entry ccmd;
          read_completion(ccmd, slot);

          auto scmd = submitted_cmds[ccmd.cid];
          if (scmd == nullptr) {
            printf("Error: completion command not found.\n");
            return -ENODEV;
          }

          // TODO Error handling
          scmd->set_state(ERT_CMD_STATE_COMPLETED);

          // Update submission queue header
          uint64_t sub_slot;
          for (sub_slot = sub_tail; (sub_slot & XRT_QUEUE1_SLOT_MASK) != ccmd.sqhead; sub_slot++)
            clear_sub_slot_state(sub_slot & XRT_QUEUE1_SLOT_MASK);
          sub_head = sub_slot;

          xgqp->cmd_pool.destroy(scmd);
          submitted_cmds.erase(ccmd.cid);

          if (slot == tail)
            break;
        }
        com_tail = slot;
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
          xrt_cmd_configure *cmdp = reinterpret_cast<xrt_cmd_configure *>(sq_buf.data());

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
          xrt_cmd_start_cuidx *cmdp = reinterpret_cast<xrt_cmd_start_cuidx *>(sq_buf.data());

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
          xrt_cmd_exit_ert *cmdp = reinterpret_cast<xrt_cmd_exit_ert*>(sq_buf.data());

          cmdp->opcode = XRT_CMD_OP_EXIT_ERT;
          cmdp->state = 1;
          cmdp->cid = cmdid;
          cmdp->count = payload_size();
        }
        break;

      default:
        printf("Error: Unknown command.\n");
        return -EINVAL;
    }

    return 0;
  }

  xocl_xgq::xocl_xgq(HwEmShim* dev)
    : queue(dev, this, XRT_QUEUE1_SLOT_NUM, XRT_SUB_Q1_SLOT_SIZE, XRT_XGQ_SUB_BASE, XRT_XGQ_COM_BASE, XRT_QUEUE1_SUB_BASE, XRT_QUEUE1_COM_BASE)
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

} // namespace hwemu
