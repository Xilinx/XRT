// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "replay.hpp"

namespace xrt_core::tools::xbreplay {


/**
 * This is replay master thread function, receives
 * command from seq reconstructor and forwards to
 * Replay worker thread.
 */
void replay_master::replay_master_main()
{
  XBREPLAY_INFO("Replay Master started");

  /* start replay worker thread */
  m_replay_worker.start();

  bool loop = true;
  while (loop)
  {
    auto msg = m_in_msgq.receive();
    if (msg->get_msgtype() != utils::message_type::stop_replay)
    {
      if (!msg_skip(msg))
      {
        /* send to worker thread */
        m_out_msgq.send(msg);
      }
    }
    else
    {
      m_out_msgq.send(msg);
      break;
    }
  }
  XBREPLAY_INFO("Replay Master Exited");
}

/**
 * This is replay worker thread function.
 * Receives instructions from Replay master thread to
 * Performs XRT API invocation along with its parameters.
 */
void replay_worker::replay_worker_main()
{
  XBREPLAY_INFO("Replay Worker started");
  while (true)
  {
    auto msg = m_in_msgq.receive();
    if (msg->get_msgtype() != utils::message_type::stop_replay)
    {
      try
      {
        m_api.invoke(msg);
      }
      catch (const std::exception& e)
      {
        XBREPLAY_ERROR("Exception occurred during API invocation: {}", e.what());
        break;
      }
      catch (...)
      {
        XBREPLAY_ERROR("An unknown error occurred");
        break;
      }
    }
    else
    {
      break;
    }
  }
  m_api.clear_map();
  XBREPLAY_INFO("Replay Worker Exited");
}

}// end of namespace
