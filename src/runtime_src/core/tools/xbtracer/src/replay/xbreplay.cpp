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

#include "common/trace_utils.h"
#include "replay/xbreplay_common.h"

using namespace xrt::tools::xbtracer;

struct cmd_arg {
  std::string in_file;
};

static void usage(const char* cmd) {
  std::cout << "Usage: " << cmd << " [options] -i <xbtracer_capture_file> -o <output_file>" << std::endl;
  std::cout << "This program is to convert xbtracer captured files to specified format output." << std::endl;
  std::cout << "Required:" << std::endl;
  std::cout << "\t-i|--input <xbtracer_capture_file> file contains what's captured by xbtracer" << std::endl;
  std::cout << "Optinoal:" << std::endl;
  std::cout << "\t-h|--help display this helper messsage." << std::endl;
}

// NOLINTBEGIN(*-avoid-c-arrays)
static
int
parse_args(struct cmd_arg &args, int argc, const char* argv[])
// NOLINTEND(*-avoid-c-arrays)
{
  for (int i = 1; i < argc; i++) {
    std::string arg_str = argv[i];
    if (arg_str == "-h" || arg_str == "--help") {
      usage(argv[0]);
      return 1;
    }
    else if (arg_str == "-i" || arg_str == "--input") {
      args.in_file = argv[++i];
    }
  }

  if (args.in_file.empty()) {
    xbtracer_perror("no input file is specified.");
    usage(argv[0]);
    return -EINVAL;
  }

  return 0;
}

static
void
xbreplay_worker(std::shared_ptr<replayer> replayer_sh,
                std::shared_ptr<xbreplay_msg_queue> queue_sh)
{
  xbreplay_receive_msgs(replayer_sh, queue_sh);
}

static
bool
xbreplay_coded_get_sequence_from_file(std::ifstream& input)
{
  google::protobuf::io::IstreamInputStream raw_input(&input);
  google::protobuf::io::CodedInputStream coded_input(&raw_input);
  uint32_t size = 0;
  if (!coded_input.ReadVarint32(&size)) {
    xbtracer_perror("failed to read header protobuf message length.");
    return false;
  }
  google::protobuf::io::CodedInputStream::Limit limit = coded_input.PushLimit(static_cast<int>(size));
  xbtracer_proto::XrtExportApiCapture header_msg;
  if (!header_msg.ParseFromCodedStream(&coded_input)) {
    xbtracer_perror("failed to parse header from coded protobuf input.");
    return false;
  }
  coded_input.PopLimit(limit);
  xbtracer_pinfo("APIs sequence captured for XRT version: ", header_msg.version(), ".");

  // We only have one queue at the moment
  std::shared_ptr<xbreplay_msg_queue> queue_sh = std::make_shared<xbreplay_msg_queue>();
  std::shared_ptr<replayer> replayer_sh = std::make_shared<replayer>();
  std::thread replayer_t(xbreplay_worker, replayer_sh, queue_sh);

  // The version of protobuf we have doesn't have IsAtEnd() or PeekTag() method which can be
  // used to check if it is the end of stream. And thus, we read the 32bit for size. If we
  // fail to read the 32bit size, it means it reaches the end of stream.
  xbtracer_pinfo("reading XRT APIs...");
  while (coded_input.ReadVarint32(&size)) {
    limit = coded_input.PushLimit(static_cast<int>(size));
    std::shared_ptr<xbtracer_proto::Func> sh_func_msg = std::make_shared<xbtracer_proto::Func>();
    if (!sh_func_msg->ParseFromCodedStream(&coded_input)) {
      xbtracer_perror("failed to parse header from coded protobuf input.");
      return false;
    }
    coded_input.PopLimit(limit);
    queue_sh->push(sh_func_msg);
  }
  xbtracer_pinfo("Done reading XRT APIs...");
  queue_sh->end_queue();

  replayer_t.join();
  google::protobuf::ShutdownProtobufLibrary();
  return true;
}

int
main(int argc, const char* argv[])
{
  // initialize logger name
  int ret = setenv_os("XBRACER_PRINT_NAME", "replay");
  if (ret) {
    std::cerr << "ERROR: xbracer: failer to set logging env." << std::endl;
    return -EINVAL;
  }

  struct cmd_arg args;
  ret = parse_args(args, argc, argv);
  if (ret < 0) {
    xbtracer_perror("failed to parse user input argumetns.");
    return -EINVAL;
  }
  // --help
  if (ret > 0) {
    return 0;
  }

  std::ifstream in_file(args.in_file, std::ios::binary);
  if (!in_file.is_open()) {
    xbtracer_perror("failed to open protobuf file \"", args.in_file, "\".");
    return -EINVAL;
  }

  xbtracer_pinfo("Replaying \"", args.in_file, "\".");
  if (!xbreplay_coded_get_sequence_from_file(in_file)) {
    xbtracer_perror("Failed to replay \"", args.in_file, "\".");
    return -EINVAL;
  }

  in_file.close();
  return 0;
}
