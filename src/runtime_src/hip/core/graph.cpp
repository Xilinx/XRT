// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "common.h"
#include "graph.h"

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <queue>

namespace xrt::core::hip {
// Add a node to the graph and return its handle.
node_handle
graph::
add_node(std::shared_ptr<graph_node> node)
{
  auto node_hdl = insert_in_map(m_node_cache, node);
  m_node_list.push_back(m_node_cache.get(node_hdl));
  return node_hdl;
}

// Returns all nodes in order from roots
std::vector<std::shared_ptr<graph_node>>
graph::
get_ordered_nodes()
{
  std::vector<std::shared_ptr<graph_node>> result;
  std::unordered_map<graph_node*, size_t> indegree;
  std::queue<std::shared_ptr<graph_node>> q;

  // Compute indegree and enqueue root nodes
  for (const auto& node : m_node_list) {
    indegree[node.get()] = node->get_deps_size();
    if (indegree[node.get()] == 0)
      q.push(node);
  }

  // enqueue nodes only when all their dependencies are added to result
  while (!q.empty()) {
    auto node = q.front();
    q.pop();
    result.push_back(node);
    for (const auto& child : node->get_children()) {
      if (child && --indegree[child.get()] == 0)
        q.push(child);
    }
  }

  // Check for cyclic dependencies: if not all nodes are processed, throw HIP error
  if (result.size() != m_node_list.size()) {
    throw_hip_error(hipErrorGraphExecUpdateFailure, "Cyclic dependency detected in graph nodes");
  }

  return result;
}

static
std::vector<std::shared_ptr<command>>
init(std::shared_ptr<graph> graph)
{
  std::vector<std::shared_ptr<command>> cmd_list;
  for (const auto& node : graph->get_ordered_nodes()) {
    // Skip nodes that are null or have no command to execute
    if (!node || !node->get_cmd())
      continue;

    auto cmd_ptr = node->get_cmd();

  // Add non-kernel_start commands to cmd_list
    if (cmd_ptr->get_type() != command::type::kernel_start) {
      cmd_list.push_back(cmd_ptr);
      continue;
    }

    // For kernel_start commands, group by hardware context if possible
    auto cmd = std::dynamic_pointer_cast<kernel_start>(cmd_ptr);
    if (!cmd)
      continue;
    auto hw_ctx = cmd->get_function()->get_hw_ctx();
    if (!hw_ctx)
      continue;

    // If the last command is a kernel_list_start with the same hardware context,
    // add current command xrt run to that instead of creating a new kernel_list_start
    if (!cmd_list.empty()) {
      auto last_cmd = std::dynamic_pointer_cast<kernel_list_start>(cmd_list.back());
      if (last_cmd && last_cmd->get_hw_ctx() && (last_cmd->get_hw_ctx().get_handle() == hw_ctx.get_handle())) {
        last_cmd->add_run(cmd->get_run());
        continue;
      }
    }

    // If the last command is not a kernel_list_start or with different hardware context,
    // Create a new kernel_list_start command for this hardware context
    auto kl_cmd = std::make_shared<kernel_list_start>(hw_ctx);
    kl_cmd->add_run(cmd->get_run());
    cmd_list.push_back(std::move(kl_cmd));
  }

  return cmd_list;
}

graph_exec::
graph_exec(std::shared_ptr<graph> graph)
  : command(type::graph_exec)
  , m_cmd_exec_list(init(graph))
{}

bool
graph_exec::
wait()
{
  // Wait for all commands in the execution list to complete if state is running
  auto graph_exec_state = get_state();
  if (graph_exec_state == state::running) {
    for (auto cmd : m_cmd_exec_list) {
      cmd->wait();
    }
    set_state(state::completed);
    return true;
  }
  else if (graph_exec_state == state::completed)
    return true;

  return false;
}

bool
graph_exec::
submit()
{
  // Submit all commands in the execution list if in the initial state
  auto graph_exec_state = get_state();
  if (graph_exec_state == state::init) {
    for (auto cmd : m_cmd_exec_list) {
      cmd->submit();
    }
    set_state(state::running);
    return true;
  }
  else if (graph_exec_state == state::running)
    return true;

  return false;
}

// Global map of graph
//override clang-tidy warning by adding NOLINT since graph_cache is non-const parameter
xrt_core::handle_map<graph_handle, std::shared_ptr<graph>> graph_cache; //NOLINT
}
