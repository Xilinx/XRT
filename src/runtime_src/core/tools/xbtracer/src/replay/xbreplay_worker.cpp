// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <memory>
#include <vector>
#include "replay/xbreplay_common.h"
#include <google/protobuf/util/json_util.h>
#include "common/trace_utils.h"

namespace xrt::tools::xbtracer
{

static
bool
xbreplay_func_proto_to_json(const xbtracer_proto::Func& func_msg, std::string& json_str)
{
  google::protobuf::util::JsonPrintOptions json_options;
  json_options.add_whitespace = true;
  json_options.always_print_primitive_fields = true;
  json_options.preserve_proto_field_names = true;
  auto status =
    google::protobuf::util::MessageToJsonString(func_msg, &json_str, json_options);
  if (!status.ok()) {
    xbtracer_perror("failed to convert XRT function protobuf message to JSON.");
    return false;
  }
  return true;
}

void
xbreplay_receive_msgs(std::shared_ptr<replayer>& replayer_sh,
                      std::shared_ptr<xbreplay_msg_queue>& queue)
{
  xbtracer_pinfo("Replay worker waiting for messages...");
  std::shared_ptr<xbtracer_proto::Func> func_entry;
  bool wait_exit_n_entry = false;
  std::string json_str;
  while(true) {
    std::shared_ptr<xbtracer_proto::Func> sh_func_msg;
    queue->wait_and_pop(sh_func_msg);
    if (!sh_func_msg) {
      xbtracer_pinfo("No more XRT function messages provided by main thread.");
      replayer_sh->untrack_all();
      return;
    }
    if (!wait_exit_n_entry) {
      if ((sh_func_msg->status() != xbtracer_proto::Func_FuncStatus_FUNC_ENTRY) &&
          (sh_func_msg->status() != xbtracer_proto::Func_FuncStatus_FUNC_INJECT)) {
        json_str.clear();
        (void)xbreplay_func_proto_to_json(*sh_func_msg, json_str);
        xbtracer_pcritical("Invalid sequence, expect function entry, but got function exit for:",
                           sh_func_msg->name(), ":\n",  json_str);
      }
      func_entry = sh_func_msg;
      if (sh_func_msg->status() == xbtracer_proto::Func_FuncStatus_FUNC_INJECT) {
        if (replayer_sh->replay(func_entry.get(), nullptr)) {
          json_str.clear();
          (void)xbreplay_func_proto_to_json(*sh_func_msg, json_str);
          xbtracer_pcritical("Failed to replay ", func_entry->name(), ".\n", json_str);
        }
        continue;
      }
      wait_exit_n_entry = true;
    } else {
      wait_exit_n_entry = false;
      if (replayer_sh->replay(func_entry.get(), sh_func_msg.get())) {
        json_str.clear();
        (void)xbreplay_func_proto_to_json(*sh_func_msg, json_str);
        xbtracer_pcritical("Failed to replay ", func_entry->name(), ".\n", json_str);
      }
    }
  }
}

} // namespace xrt::tools::xbtracer
