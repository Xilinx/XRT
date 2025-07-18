// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <google/protobuf/message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/json_util.h>

#include <func.pb.h>
#include "common/trace_utils.h"

using namespace xrt::tools::xbtracer;

struct cmd_arg {
  std::string in_file;
  std::string out_file;
  std::string format;
};

static
void
usage(const char* cmd) {
  std::cout << "Usage: " << cmd << " [options] -i <xbtracer_capture_file> -o <output_file>" << std::endl;
  std::cout << "This program is to convert xbtracer captured files to specified format output." << std::endl;
  std::cout << "Required:" << std::endl;
  std::cout << "\t-i|--input <xbtracer_capture_file> file contains what's captured by xbtracer" << std::endl;
  std::cout << "Optinoal:" << std::endl;
  std::cout << "\t-f|--format [FORMAT] output format, default is JSON. We support JSON only for now." << std::endl;
  std::cout << "\t-h|--help display this helper messsage." << std::endl;
  std::cout << "\t-o|--output <output_file> file for the converted output format" << std::endl;
}

// NOLINTBEGIN(*-avoid-c-arrays)
static
int
parse_args(struct cmd_arg &args, int argc, const char* argv[])
// NOLINTEND(*-avoid-c-arrays)
{
  args.format = "JSON";
  for (int i = 1; i < argc; i++) {
    std::string arg_str = argv[i];
    if (arg_str == "-h" || arg_str == "--help") {
      usage(argv[0]);
      return 1;
    }
    else if (arg_str == "-i" || arg_str == "--input") {
      args.in_file = argv[++i];
    }
    else if (arg_str == "-o" || arg_str == "--output") {
      args.out_file = argv[++i];
    }
    else if (arg_str == "-f" || arg_str == "--format") {
      if (strcmp(argv[++i], "JSON") != 0)
        xbtracer_pcritical("invalid format: ", std::string(argv[i]), ", only JSON is supported.");
    }
  }

  if (args.in_file.empty()) {
    xbtracer_perror("no input file is specified.");
    usage(argv[0]);
    return -1;
  }

  return 0;
}

static
bool
xbtracer_coded_protobuf_to_json(std::ifstream& input, std::ostream& output)
{
  std::string json_string;
  google::protobuf::util::JsonPrintOptions json_options;
  json_options.add_whitespace = true;
  json_options.always_print_primitive_fields = true;
  json_options.preserve_proto_field_names = true;


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
  auto status =
      google::protobuf::util::MessageToJsonString(header_msg, &json_string, json_options);
  if (!status.ok()) {
    xbtracer_perror("failed to convert header protobuf to JSON: ", status.ToString(), ".");
    return false;
  }

  output << json_string;
  output.flush();

  // The version of protobuf we have doesn't have IsAtEnd() or PeekTag() method which can be
  // used to check if it is the end of stream. And thus, we read the 32bit for size. If we
  // fail to read the 32bit size, it means it reaches the end of stream.
  while (coded_input.ReadVarint32(&size)) {
    limit = coded_input.PushLimit(static_cast<int>(size));
    xbtracer_proto::Func func_msg;
    if (!func_msg.ParseFromCodedStream(&coded_input)) {
        xbtracer_perror("failed to parse header from coded protobuf input.");
        return false;
    }
    coded_input.PopLimit(limit);
    json_string.clear();
    status = google::protobuf::util::MessageToJsonString(func_msg, &json_string, json_options);
    if (!status.ok()) {
      xbtracer_perror("failed  to convert function protobuf to JSON: ", status.ToString(), ".");
      return false;
    }
    output << json_string;
    output.flush();
  }

  google::protobuf::ShutdownProtobufLibrary();
  return true;
}

int
main(int argc, const char* argv[])
{
  // initialize logger name
  int ret = setenv_os("XBRACER_PRINT_NAME", "display");
  if (ret) {
    std::cerr << "ERROR: xbracer: failer to set logging env." << std::endl;
    return -EINVAL;
  }

  struct cmd_arg args;
  if (parse_args(args, argc, argv)) {
    xbtracer_perror("failed to parse user input argumetns.");
    return -EINVAL;
  }

  std::ifstream in_file(args.in_file, std::ios::binary);
  if (!in_file.is_open()) {
    xbtracer_perror("failed to open protobuf file \"", args.in_file, "\".");
    return -EINVAL;
  }

  std::ofstream out_file;
  if (!args.out_file.empty()) {
    out_file.open(args.out_file, std::ios::app); // Append to file
    if (!out_file.is_open()) {
      xbtracer_perror("failed to open output file \"", args.out_file, "\".");
      in_file.close();
      return -EINVAL;
    }
  }
  xbtracer_pinfo("Converting \"", args.in_file, "\" to JSON, output will be in \"", args.out_file, "\".");
  bool protobuf_ret = false;
  if (out_file.is_open()) {
    protobuf_ret = xbtracer_coded_protobuf_to_json(in_file, out_file);
  }
  else {
    protobuf_ret = xbtracer_coded_protobuf_to_json(in_file, std::cout);
  }

  in_file.close();
  if (out_file.is_open()) {
    out_file.close();
  }

  if (!protobuf_ret) {
    xbtracer_perror("failed to convert protobuf from \"", args.in_file, "\" to JSON \"", args.out_file, "\".");
    return -EINVAL;
  }

  return 0;
}
