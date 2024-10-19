// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "replay_xrt.hpp"
#include "utils/message_queue.hpp"

#include <string>
#include <utility>
#include <vector>

namespace xrt_core::tools::xbreplay {

/**
 * Replay worker class
 */
class replay_worker
{
  utils::message_queue& m_in_msgq;
  std::thread m_replay_thrd;
  replay_xrt m_api;

  public:
  replay_worker(utils::message_queue& mqueues)
  :m_in_msgq(mqueues)
  {}

  void replay_worker_main();
  void start()
  {
    m_replay_thrd = std::thread([this]()
    {
      this->replay_worker_main();
    });
  }

  void th_join()
  {
    m_replay_thrd.join();
  }
};

/*
 * Replay Master class.
 */
class replay_master
{
  private:
  /**
   * This data structure holds references of those API's which invoke  another
   * exposed API's. In such scenarios Replay should ignore the false invocation.
   *
   * To acheive this replay maintains a record of those API's which invoke another
   * exposed API's
   * the first entry represents the Exposed API call to look out for
   * The second entry represent the corresponding Exposed API to ignore.
   */
  std::vector<std::pair<std::string, std::string>> m_api_skip;

  utils::message_queue& m_in_msgq;
  utils::message_queue m_out_msgq;
  std::thread m_replay_thrd;
  uint64_t m_api_skip_flag_cnt;

  /* vector<pair<API_ID ,TID>>  */
  std::vector<std::pair<std::string, uint64_t>>m_api_skip_list;
  replay_worker m_replay_worker;

  void init_api_skip_list()
  {
     m_api_skip =
     {
        /* {"API to Look for", "API to Skip" } */
        {"xrt::device::load_xclbin(const std::string&)",
                                 "xrt::xclbin::xclbin(const axlf*)"},
#ifdef __linux__
        {"xrt::device::register_xclbin(const xrt::xclbin&)",
                                 "xrt::xclbin::xclbin(const axlf*)"},
#endif
     };
  }

  public:
  replay_master(utils::message_queue& msg_q)
  : m_in_msgq(msg_q)
  , m_replay_worker(m_out_msgq)
  {
    m_api_skip_flag_cnt = 0;
    init_api_skip_list();
  }

  void replay_master_main();

  void start()
  {
    m_replay_thrd = std::thread([this]()
    {
      this->replay_master_main();
    });
  }

  bool msg_skip(std::shared_ptr<utils::message> msg)
  {
    bool skip = false;

    /* Here we need to consider two cases.
     * Step1. If the skip flag is set then skip the message and clear the flag.
     * Step2. Check if the Current API calls an exposed API, in this case set
     *        the skip flag
     */
    if (m_api_skip_flag_cnt)
    {
      auto find_skip_api = std::make_pair(msg->m_api_id, msg->m_tid);

      auto it = std::find(m_api_skip_list.begin(),
                               m_api_skip_list.end(), find_skip_api);
      if (it != m_api_skip_list.end())
      {
        m_api_skip_list.erase(it);

        /* decrement API skip flag count */
        if (m_api_skip_flag_cnt > 0)
        {
          m_api_skip_flag_cnt--;
        }
        skip = true;
      }
      else
      {
        XBREPLAY_WARN("API: (", find_skip_api.first ,", ",
                  find_skip_api.second, ") not found in the vector.");
        skip = false;
      };

      return skip;
    }

    /*step2 */
    for (const auto& api : m_api_skip)
    {
      if (msg->m_api_id == api.first)
      {
        /* This API will invoke another exposed interface, we need to
         * skip the subsequent API, set the skip flag.
         */
        m_api_skip_list.push_back(std::make_pair(api.second, msg->m_tid));
        m_api_skip_flag_cnt++;
      }
    }
    return skip;
  }

  void th_join()
  {
    m_replay_thrd.join();
    m_replay_worker.th_join();
  }
};
}// end of namespace
