// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <google/protobuf/message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "func.pb.h"
#include "common/trace_utils.h"
#include "replay/xbreplay_common.h"

using namespace xrt::tools::xbtracer;

struct cmd_arg {
  std::string in_file;
};

std::vector<std::tuple<std::thread, std::shared_ptr<xbreplay_msg_queue>>> threads_queues;

std::optional<std::shared_ptr<xbreplay_msg_queue>>
xbreplay_get_msg_queue_from_tid(const std::thread::id& tid)
{
  for (auto& t_queue: threads_queues) {
    if (std::get<0>(t_queue).get_id() == tid)
      return std::get<1>(t_queue);
  }
  return std::nullopt;
}

static void usage(const char* cmd) {
  std::cout << "Usage: " << cmd << " [options] -i <xbtracer_capture_file> -o <output_file>" << std::endl;
  std::cout << "This program is to convert xbtracer captured files to specified format output." << std::endl;
  std::cout << "Required:" << std::endl;
  std::cout << "\t-i|--input <xbtracer_capture_file> file contains what's captured by xbtracer" << std::endl;
  std::cout << "Optinoal:" << std::endl;
  std::cout << "\t-h|--help display this helper messsage." << std::endl;
}

static
int
parse_args(struct cmd_arg &args, int argc, const char* argv[])
{
  for (int i = 1; i < argc; i++) {
    std::string arg_str = argv[i];
    if (arg_str == "-h" || arg_str == "--help") {
      usage(argv[0]);
      std::exit(0);
    }
    else if (arg_str == "-i" || arg_str == "--input") {
      args.in_file = argv[++i];
    }
  }

  if (args.in_file.empty()) {
    xbtracer_perror("no input file is specified.");
    usage(argv[0]);
    std::exit(-1);
  }

  return 0;
}

std::shared_ptr<xrt::tools::xbtracer::replayer> replayer_sh = std::make_shared<xrt::tools::xbtracer::replayer>();

static
void
xbreplay_worker(std::shared_ptr<xbreplay_msg_queue> queue_sh)
{
  xbreplay_receive_msgs(queue_sh);
}

static
bool
xbreplay_coded_get_sequence_from_file(std::ifstream& input)
{
  google::protobuf::io::IstreamInputStream raw_input(&input);
  google::protobuf::io::CodedInputStream coded_input(&raw_input);
  uint32_t size;
  if (!coded_input.ReadVarint32(&size))
    xbtracer_pcritical("failed to read header protobuf message length.");
  google::protobuf::io::CodedInputStream::Limit limit = coded_input.PushLimit(size);
  xbtracer_proto::XrtExportApiCapture header_msg;
  if (!header_msg.ParseFromCodedStream(&coded_input))
      xbtracer_pcritical("failed to parse header from coded protobuf input.");
  coded_input.PopLimit(limit);
  xbtracer_pinfo("APIs sequence captured for XRT version: ", header_msg.version(), ".");

  // We only have one queue at the moment
  std::shared_ptr<xbreplay_msg_queue> queue_sh = std::make_shared<xbreplay_msg_queue>();
  std::tuple<std::thread, std::shared_ptr<xbreplay_msg_queue>> t_queue(std::thread(xbreplay_worker,
                                                                                   queue_sh),
                                                                       queue_sh);
  threads_queues.push_back(std::move(t_queue));
  auto& msg_queue_sh = std::get<1>(threads_queues[0]);

  // The version of protobuf we have doesn't have IsAtEnd() or PeekTag() method which can be
  // used to check if it is the end of stream. And thus, we read the 32bit for size. If we
  // fail to read the 32bit size, it means it reaches the end of stream.
  xbtracer_pinfo("reading XRT APIs...");
  while (coded_input.ReadVarint32(&size)) {
    limit = coded_input.PushLimit(size);
    std::shared_ptr<xbtracer_proto::Func> sh_func_msg = std::make_shared<xbtracer_proto::Func>();
    if (!sh_func_msg->ParseFromCodedStream(&coded_input))
        xbtracer_pcritical("failed to parse header from coded protobuf input.");
    coded_input.PopLimit(limit);
    msg_queue_sh->push(sh_func_msg);
  }
  xbtracer_pinfo("Done reading XRT APIs...");
  msg_queue_sh->end_queue();

  for (auto& tq: threads_queues)
    std::get<0>(tq).join();
  google::protobuf::ShutdownProtobufLibrary();
  return true;
}

int main(int argc, const char* argv[])
{
  // initialize logger name
  int ret = setenv_os("XBRACER_PRINT_NAME", "replay");
  if (ret) {
    std::cerr << "ERROR: xbracer: failer to set logging env." << std::endl;
    return -EINVAL;
  }

  struct cmd_arg args;
  if (parse_args(args, argc, argv))
    xbtracer_pcritical("failed to parse user input argumetns.");

  std::ifstream in_file(args.in_file, std::ios::binary);
  if (!in_file.is_open())
    xbtracer_pcritical("failed to open protobuf file \"", args.in_file, "\".");

  xbtracer_pinfo("Replaying \"", args.in_file, "\".");
  if (!xbreplay_coded_get_sequence_from_file(in_file))
    xbtracer_pcritical("Failed to replay \"", args.in_file, "\".");

  in_file.close();
  return 0;
}
