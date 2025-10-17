// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef xrthip_graph_h
#define xrthip_graph_h

#include "event.h"
#include "module.h"

namespace xrt::core::hip {

// node_handle - opaque graph node handle
using node_handle = void*;

// graph_handle - opaque graph handle
using graph_handle = void*;

// Represents a node in the HIP graph, wrapping a command.
class graph_node : public std::enable_shared_from_this<graph_node>
{
private:
  std::shared_ptr<command> m_cmd;
  std::vector<std::shared_ptr<graph_node>> m_deps_list; // parents
  std::vector<std::weak_ptr<graph_node>> m_children;    // children as weak_ptr to avoid cycles
  size_t m_deps_size = 0;

public:
  graph_node() = default;
  explicit graph_node(std::shared_ptr<command> cmd)
    : m_cmd(std::move(cmd))
  {}

  const std::shared_ptr<command>&
  get_cmd() const
  {
    return m_cmd;
  }

  size_t
  get_deps_size() const
  {
    return m_deps_size;
  }

  const std::vector<std::shared_ptr<graph_node>>&
  get_deps_list() const
  {
    return m_deps_list;
  }

  // Returns all valid child nodes by locking weak_ptrs.
  // Uses weak_ptr to avoid strong reference cycles between parent and child nodes.
  std::vector<std::shared_ptr<graph_node>>
  get_children() const
  {
    std::vector<std::shared_ptr<graph_node>> result;
    for (const auto& wptr : m_children) {
      if (auto sptr = wptr.lock()) {
        result.push_back(sptr);
      }
    }
    return result;
  }

  // Adds a dependency (parent) to this node and updates the parent's children list.
  // This establishes a direct connection from parent to child in the graph.
  // The child is stored as a weak_ptr in the parent's m_children vector to avoid
  // strong reference cycle dependencies.
  void
  add_dep_node(std::shared_ptr<graph_node> parent)
  {
    parent->m_children.push_back(weak_from_this());
    m_deps_list.push_back(std::move(parent));
    m_deps_size++;
  }
};

// Represents a graph of nodes (commands) for HIP execution.
class graph
{
public:
  graph() = default;
  explicit graph(unsigned int /*flags*/)
  {}

  node_handle
  add_node(std::shared_ptr<graph_node> node);

  std::vector<std::shared_ptr<graph_node>>
  get_ordered_nodes() const;

  const std::vector<std::shared_ptr<graph_node>>&
  get_node_list() const
  {
    return m_node_list;
  }

  const std::shared_ptr<graph_node>&
  get_node(node_handle node_handle) const
  {
    return m_node_cache.get_or_error(node_handle);
  }

private:
  xrt_core::handle_map<node_handle, std::shared_ptr<graph_node>> m_node_cache;
  std::vector<std::shared_ptr<graph_node>> m_node_list;
};

// Represents an executable instance of a HIP graph.
class graph_exec : public command
{
private:
  std::vector<std::shared_ptr<graph_node>> m_node_exec_list;
  std::future<void> m_exec_future;

public:
  graph_exec() = default;
  explicit graph_exec(std::shared_ptr<graph> graph);

  bool submit() override;
  bool wait() override;
};

// Global map of graph
extern xrt_core::handle_map<graph_handle, std::shared_ptr<graph>> graph_cache;
} // xrt::core::hip

#endif
