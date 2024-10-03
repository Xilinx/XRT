// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "replay.hpp"
#include "utils/message_queue.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace xrt_core::tools::xbreplay {

/**
 * Sequence Reconstructor  abstract class
 */
class seq_reconstructor
{
  protected:
  std::ifstream m_trace_file;
  std::ifstream m_mem_dmp_file;
  std::thread m_seq_recon_thread;
  virtual ~seq_reconstructor() {}

  public:
  virtual void start_reconstruction() = 0;
  virtual void threads_join() = 0;
};

/**
 * Sequence Reconstructor class
 */
class xrt_seq_reconstructor : public seq_reconstructor
{
  private:
  utils::message_queue m_msgq;
  replay_master m_replay_master;

  public:
  bool m_is_mem_file_available;
  std::string m_mem_file_path;

  void start_reconstruction() override;

  void start_sequencer_thread()
  {
    m_seq_recon_thread =
        std::thread([this]() { this->start_reconstruction(); });
  }

  /* constructor */
  xrt_seq_reconstructor(const std::string &trace_file_path,
                        const std::string &mem_dmp_file_path)
      : m_replay_master(m_msgq)
  {
    m_trace_file.open(trace_file_path.c_str());

    if (!m_trace_file.is_open())
      throw std::runtime_error("Failed to open input file: " + trace_file_path);

    if (!mem_dmp_file_path.empty())
    {
      m_mem_dmp_file.open(trace_file_path.c_str(), std::ios::binary);
      if (!m_mem_dmp_file.is_open())
      {
        XBREPLAY_WARN("Failed to open memory dump file:", mem_dmp_file_path);
        m_is_mem_file_available = false;
      }
      else
      {
        m_is_mem_file_available = true;
        m_mem_file_path = mem_dmp_file_path;
        m_mem_dmp_file.close();
      }
    }

    XBREPLAY_INFO("Start Seq Reconstructor thread");
    start_sequencer_thread();
  }

  ~xrt_seq_reconstructor() {}

  void threads_join() override
  {
    m_seq_recon_thread.join();
    m_replay_master.th_join();
  }
};

/**
 * Sequence Reconstructor Factory
 * This class will create a sequence Reconstructor object based on
 * passed params
 */
class seq_reconstructor_factory
{
  public:
  std::shared_ptr<seq_reconstructor>
  create_seq_recon(const std::string &tracer_file,
                   const std::string &dump_file)
  {
    return std::make_shared<xrt_seq_reconstructor>(tracer_file, dump_file);
  }
};

} // namespace xrt_core::tools::xbreplay
