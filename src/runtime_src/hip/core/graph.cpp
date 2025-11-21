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
get_ordered_nodes() const
{
  std::vector<std::shared_ptr<graph_node>> result;
  std::unordered_map<graph_node*, size_t> indegree;
  std::queue<std::shared_ptr<graph_node>> q;

  // Compute indegree and enqueue root nodes
  for (const auto& node : m_node_list) {
    indegree[node.get()] = node->get_deps_list().size();
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
  if (result.size() != m_node_list.size())
    throw_hip_error(hipErrorGraphExecUpdateFailure, "Cyclic dependency detected in graph nodes");

  return result;
}

static
std::vector<std::shared_ptr<graph_node>>
init(const std::shared_ptr<graph>& graph)
{
  std::vector<std::shared_ptr<graph_node>> node_list;
  std::unordered_map<std::shared_ptr<graph_node>, std::shared_ptr<graph_node>> kernel_to_list_map;

  for (const auto& node : graph->get_ordered_nodes()) {
    auto cmd_ptr = node->get_cmd();
    if (!cmd_ptr)
      continue;

    // Add non-kernel_start commands to node_list
    if (cmd_ptr->get_type() != command::type::kernel_start) {
      node_list.push_back(node);
      continue;
    }

    // For kernel_start commands, group by hardware context if possible
    auto cmd = std::dynamic_pointer_cast<kernel_start>(cmd_ptr);
    auto hw_ctx = cmd->get_function()->get_hw_ctx();
    if (!hw_ctx)
      throw_hip_error(hipErrorInvalidContext, "Invalid hardware context");

    // Group kernel_start commands by hardware context:
    // If the last node is a kernel_list_start with the same hardware context,
    // add the current kernel run to its list and map this kernel_start to that node.
    std::shared_ptr<graph_node> kl_node;
    if (!node_list.empty()) {
      auto last_node = node_list.back();
      auto last_cmd = last_node->get_cmd();
      auto last_kl_cmd = std::dynamic_pointer_cast<kernel_list_start>(last_cmd);
      if (last_kl_cmd && last_kl_cmd->get_hw_ctx() && (last_kl_cmd->get_hw_ctx().get_handle() == hw_ctx.get_handle())) {
        last_kl_cmd->add_run(cmd->get_run());
        kernel_to_list_map[node] = last_node;
        kl_node = std::move(last_node);
      }
    }

    // If the last command is not a kernel_list_start or with different hardware context,
    // Create a new kernel_list_start command for this hardware context
    if (!kl_node) {
      auto kl_cmd = std::make_shared<kernel_list_start>(hw_ctx);
      kl_node = std::make_shared<graph_node>(kl_cmd);
      node_list.push_back(kl_node);
      kl_cmd->add_run(cmd->get_run());
      kernel_to_list_map[node] = kl_node;
    }

    // Resolve dependencies for this kernel_list_start node.
    for (const auto& dep_node : node->get_deps_list()) {
      auto dep_cmd = dep_node->get_cmd();

      // If dep_node is a non-kernel_start node, or
      // a kernel_start node not yet mapped, add directly.
      if (dep_cmd->get_type() != command::type::kernel_start ||
          kernel_to_list_map.find(dep_node) == kernel_to_list_map.end()) {
        kl_node->add_dep_node(dep_node);
        continue;
      }

      // If dep_cmd belongs to the same kernel_list_start node,
      // ignore (already grouped).
      // If dep_cmd belongs to a different kernel_list_start node,
      // add that kernel_list_start node as a dependency node.
      auto dep_kl_node = kernel_to_list_map[dep_node];
      if (dep_kl_node != kl_node)
        kl_node->add_dep_node(std::move(dep_kl_node));
    }
  }

  return node_list;
}

graph_exec::
graph_exec(const std::shared_ptr<graph>& graph)
  : m_node_exec_list(init(graph))
{}

void
graph_exec::
execute(std::shared_ptr<stream> s)
{
  // Set stream on event_record_command and event_wait_command nodes before enqueuing
  for (auto& node : m_node_exec_list) {
    auto cmd = node->get_cmd();
    if (cmd && cmd->get_type() == command::type::event_record) {
      auto event_record_cmd = std::dynamic_pointer_cast<event_record_command>(cmd);
      if (event_record_cmd)
        event_record_cmd->set_stream(s);
    }
    else if (cmd && cmd->get_type() == command::type::event_wait) {
      auto event_wait_cmd = std::dynamic_pointer_cast<event_wait_command>(cmd);
      if (event_wait_cmd)
        event_wait_cmd->set_stream(s);
    }
  }

  // Create async task to enqueue commands to stream
  auto future = std::async(std::launch::async, [this, s]() {
    for (auto& node : m_node_exec_list) {
      // Wait for all dependencies to complete before enqueuing this node
      for (const auto& dep : node->get_deps_list())
        dep->get_cmd()->wait();

      // Enqueue the command to the stream instead of submitting directly
      auto cmd = node->get_cmd();
      if (cmd && (cmd->get_type() == command::type::event_record ||
                  cmd->get_type() == command::type::event_wait))
        cmd->submit();
      else
        s->enqueue(cmd);
    }
  });

  // Store the future in the stream so synchronize() can wait for it
  s->set_graph_exec_future(std::move(future));
}

// Global map of graph
// override clang-tidy warning by adding NOLINT since graph_cache is non-const parameter
xrt_core::handle_map<graph_handle, std::shared_ptr<graph>> graph_cache; // NOLINT

// Global map of graph_exec
// override clang-tidy warning by adding NOLINT since graph_exec_cache is non-const parameter
xrt_core::handle_map<graph_exec_handle, std::shared_ptr<graph_exec>> graph_exec_cache; // NOLINT
}
