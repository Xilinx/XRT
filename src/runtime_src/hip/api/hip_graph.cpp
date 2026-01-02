// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "hip/core/common.h"
#include "hip/core/event.h"
#include "hip/core/graph.h"
#include "hip/core/memory.h"
#include "hip/core/stream.h"
#include "hip/hip_xrt.h"

#include <iostream>
#include <memory>

namespace xrt::core::hip {

static inline void
add_node_dependencies(const std::shared_ptr<graph>& hip_graph, node_handle node_hdl,
                      const hipGraphNode_t *pDependencies, size_t numDependencies)
{
  if (!pDependencies || !numDependencies)
    return;

  auto node = hip_graph->get_node(node_hdl);
  for (size_t i = 0; i < numDependencies; ++i)
    node->add_dep_node(hip_graph->get_node(pDependencies[i]));
}

static graph_handle
hip_graph_create(unsigned int flags)
{
  return insert_in_map(graph_cache, std::make_shared<graph>(flags));
}

static node_handle
hip_graph_add_kernel_node(hipGraph_t g, const hipGraphNode_t *pDependencies,
                          size_t numDependencies, const hipKernelNodeParams *pNodeParams)
{
  throw_invalid_resource_if(!g, "graph is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  auto func_hdl = reinterpret_cast<function_handle>(pNodeParams->func);
  throw_invalid_resource_if(!func_hdl, "invalid func_hdl");

  auto hip_mod = module_cache.get_or_error(static_cast<function*>(func_hdl)->get_module());
  throw_invalid_resource_if(!hip_mod, "module associated with function is unloaded");

  auto hip_func = hip_mod->get_function(func_hdl);
  throw_invalid_resource_if(!hip_func, "invalid function passed");

  // Create a kernel_start command and wrap it in a graph_node
  auto hip_cmd = std::make_shared<kernel_start>(hip_func, pNodeParams->kernelParams);
  auto node_hdl = hip_graph->add_node(std::make_shared<graph_node>(hip_cmd));

  add_node_dependencies(hip_graph, node_hdl, pDependencies, numDependencies);

  return node_hdl;
}

static node_handle
hip_graph_add_empty_node(hipGraph_t g, const hipGraphNode_t *pDependencies,
                         size_t numDependencies)
{
  throw_invalid_resource_if(!g, "graph is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  // Create an empty command and wrap it in a graph_node
  auto hip_cmd = std::make_shared<empty_command>();
  auto node_hdl = hip_graph->add_node(std::make_shared<graph_node>(hip_cmd));

  add_node_dependencies(hip_graph, node_hdl, pDependencies, numDependencies);

  return node_hdl;
}

static node_handle
hip_graph_add_memset_node(hipGraph_t g, const hipGraphNode_t *pDependencies,
                          size_t numDependencies, const hipMemsetParams *pMemsetParams)
{
  throw_invalid_resource_if(!g, "graph is nullptr");
  throw_invalid_value_if(!pMemsetParams, "memset params is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  // Get memory info for the destination pointer
  auto hip_mem_info = memory_database::instance().get_hip_mem_from_addr(pMemsetParams->dst);
  auto hip_mem_dst = hip_mem_info.first;
  auto offset = hip_mem_info.second;
  throw_invalid_value_if(!hip_mem_dst, "Invalid destination handle.");
  throw_invalid_value_if(hip_mem_dst->get_type() == xrt::core::hip::memory_type::invalid,
                         "memory type is invalid for memset.");

  // Calculate total size from width, height, and elementSize
  auto element_size = pMemsetParams->elementSize;
  auto width = pMemsetParams->width;
  auto height = pMemsetParams->height > 0 ? pMemsetParams->height : 1;
  auto total_size = width * height * element_size;

  throw_invalid_value_if(offset + total_size > hip_mem_dst->get_size(), "dst out of bound.");
  throw_invalid_value_if(total_size % element_size != 0, "Invalid size.");

  std::shared_ptr<command> hip_cmd;
  auto element_count = total_size / element_size;

  // Create appropriate command based on element size
  switch (element_size) {
    case 1: {
      std::vector<std::uint8_t> host_vec(element_count, static_cast<std::uint8_t>(pMemsetParams->value));
      hip_cmd = std::make_shared<copy_from_host_buffer_command<std::uint8_t>>(
        hip_mem_dst, std::move(host_vec), total_size, offset);
      break;
    }
    case 2: {
      std::vector<std::uint16_t> host_vec(element_count, static_cast<std::uint16_t>(pMemsetParams->value));
      hip_cmd = std::make_shared<copy_from_host_buffer_command<std::uint16_t>>(
        hip_mem_dst, std::move(host_vec), total_size, offset);
      break;
    }
    case 4: {
      std::vector<std::uint32_t> host_vec(element_count, static_cast<std::uint32_t>(pMemsetParams->value));
      hip_cmd = std::make_shared<copy_from_host_buffer_command<std::uint32_t>>(
        hip_mem_dst, std::move(host_vec), total_size, offset);
      break;
    }
    default:
      throw_invalid_value_if(true, "Unsupported element size.");
  }

  auto node_hdl = hip_graph->add_node(std::make_shared<graph_node>(hip_cmd));

  add_node_dependencies(hip_graph, node_hdl, pDependencies, numDependencies);

  return node_hdl;
}

static node_handle
hip_graph_add_memcpy_node_1d(hipGraph_t g, const hipGraphNode_t *pDependencies,
                             size_t numDependencies, void* dst, const void* src,
                             size_t count, hipMemcpyKind kind)
{
  throw_invalid_resource_if(!g, "graph is nullptr");
  throw_invalid_value_if(!dst, "dst is nullptr");
  throw_invalid_value_if(!src, "src is nullptr");
  throw_invalid_value_if(!count, "size is 0 for memcpy node");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  auto hip_cmd = std::make_shared<memcpy_command>(dst, src, count, kind);
  auto node_hdl = hip_graph->add_node(std::make_shared<graph_node>(hip_cmd));

  add_node_dependencies(hip_graph, node_hdl, pDependencies, numDependencies);

  return node_hdl;
}

static node_handle
hip_graph_add_event_record_node(hipGraph_t g, const hipGraphNode_t *pDependencies,
                                size_t numDependencies, hipEvent_t event_handle)
{
  throw_invalid_resource_if(!g, "graph is nullptr");
  throw_invalid_value_if(!event_handle, "event is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  auto cmd = command_cache.get(event_handle);
  throw_invalid_resource_if(!cmd, "invalid event passed");

  auto hip_ev = std::dynamic_pointer_cast<event>(cmd);
  throw_invalid_resource_if(!hip_ev, "invalid event passed");

  // Create an event_record_command and wrap it in a graph_node
  auto hip_cmd = std::make_shared<event_record_command>(hip_ev);
  auto node_hdl = hip_graph->add_node(std::make_shared<graph_node>(hip_cmd));

  add_node_dependencies(hip_graph, node_hdl, pDependencies, numDependencies);

  return node_hdl;
}

static node_handle
hip_graph_add_event_wait_node(hipGraph_t g, const hipGraphNode_t *pDependencies,
                              size_t numDependencies, hipEvent_t event_handle)
{
  throw_invalid_resource_if(!g, "graph is nullptr");
  throw_invalid_value_if(!event_handle, "event is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  auto cmd = command_cache.get(event_handle);
  throw_invalid_resource_if(!cmd, "invalid event passed");

  auto hip_ev = std::dynamic_pointer_cast<event>(cmd);
  throw_invalid_resource_if(!hip_ev, "invalid event passed");

  // Create an event_wait_command and wrap it in a graph_node
  auto hip_cmd = std::make_shared<event_wait_command>(hip_ev);
  auto node_hdl = hip_graph->add_node(std::make_shared<graph_node>(hip_cmd));

  add_node_dependencies(hip_graph, node_hdl, pDependencies, numDependencies);

  return node_hdl;
}

// TODO: Implement error reporting and logging for graph instantiation:
//   - pErrorNode: Pointer to error node if error occured during graph instantiation
//   - pLogBuffer, bufferSize: log messages about instantiation to the buffer
static graph_exec_handle
hip_graph_instantiate(hipGraph_t g, hipGraphNode_t* /*pErrorNode*/,
                      char* /*pLogBuffer*/, size_t /*bufferSize*/)
{
  throw_invalid_resource_if(!g, "graph is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  return insert_in_map(graph_exec_cache, std::make_shared<graph_exec>(hip_graph));
}

static void
hip_graph_launch(hipGraphExec_t gE, hipStream_t stream)
{
  throw_invalid_resource_if(!gE, "graph exec is nullptr");

  auto gE_ptr = graph_exec_cache.get_or_error(gE);
  throw_invalid_resource_if(!gE_ptr, "invalid graph exec");

  gE_ptr->execute(get_stream(stream));
}

static void
hip_graph_exec_destroy(hipGraphExec_t gE)
{
  throw_invalid_resource_if(!gE, "graph exec is nullptr");

  graph_exec_cache.remove(reinterpret_cast<graph_exec_handle>(gE));
}

static void
hip_graph_destroy(hipGraph_t g)
{
  throw_invalid_resource_if(!g, "graph is nullptr");

  graph_cache.remove(reinterpret_cast<graph_handle>(g));
}
} // // xrt::core::hip

// =========================================================================
// Graph related apis implementation
hipError_t
hipGraphCreate(hipGraph_t *pGraph, unsigned int flags)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraph, "Graph passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_create(flags);
    *pGraph = reinterpret_cast<hipGraph_t>(handle);
  });
}

hipError_t
hipGraphAddKernelNode(hipGraphNode_t *pGraphNode, hipGraph_t graph,
                      const hipGraphNode_t *pDependencies, size_t numDependencies,
                      const hipKernelNodeParams *pNodeParams)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphNode, "Graph Node passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_add_kernel_node(graph,
                                                            pDependencies,
                                                            numDependencies,
                                                            pNodeParams);
    *pGraphNode = reinterpret_cast<hipGraphNode_t>(handle);
  });
}

hipError_t
hipGraphAddEmptyNode(hipGraphNode_t *pGraphNode, hipGraph_t graph,
                     const hipGraphNode_t *pDependencies, size_t numDependencies)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphNode, "Graph Node passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_add_empty_node(graph,
                                                           pDependencies,
                                                           numDependencies);
    *pGraphNode = reinterpret_cast<hipGraphNode_t>(handle);
  });
}

hipError_t
hipGraphAddMemsetNode(hipGraphNode_t *pGraphNode, hipGraph_t graph,
                      const hipGraphNode_t *pDependencies, size_t numDependencies,
                      const hipMemsetParams *pMemsetParams)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphNode, "Graph Node passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_add_memset_node(graph,
                                                            pDependencies,
                                                            numDependencies,
                                                            pMemsetParams);
    *pGraphNode = reinterpret_cast<hipGraphNode_t>(handle);
  });
}

hipError_t
hipGraphAddMemcpyNode1D(hipGraphNode_t *pGraphNode, hipGraph_t graph,
                        const hipGraphNode_t *pDependencies, size_t numDependencies,
                        void* dst, const void* src, size_t count, hipMemcpyKind kind)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphNode, "Graph Node passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_add_memcpy_node_1d(graph,
                                                               pDependencies,
                                                               numDependencies,
                                                               dst,
                                                               src,
                                                               count,
                                                               kind);
    *pGraphNode = reinterpret_cast<hipGraphNode_t>(handle);
  });
}

hipError_t
hipGraphAddEventRecordNode(hipGraphNode_t *pGraphNode, hipGraph_t graph,
                           const hipGraphNode_t *pDependencies, size_t numDependencies,
                           hipEvent_t event)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphNode, "Graph Node passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_add_event_record_node(graph,
                                                                  pDependencies,
                                                                  numDependencies,
                                                                  event);
    *pGraphNode = reinterpret_cast<hipGraphNode_t>(handle);
  });
}

hipError_t
hipGraphAddEventWaitNode(hipGraphNode_t *pGraphNode, hipGraph_t graph,
                        const hipGraphNode_t *pDependencies, size_t numDependencies,
                        hipEvent_t event)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphNode, "Graph Node passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_add_event_wait_node(graph,
                                                                pDependencies,
                                                                numDependencies,
                                                                event);
    *pGraphNode = reinterpret_cast<hipGraphNode_t>(handle);
  });
}

hipError_t
hipGraphInstantiate(hipGraphExec_t *pGraphExec, hipGraph_t graph,
                    hipGraphNode_t *pErrorNode, char *pLogBuffer,
                    size_t bufferSize)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!pGraphExec, "Graph Exec passed is nullptr");
    auto handle = xrt::core::hip::hip_graph_instantiate(graph,
                                                        pErrorNode,
                                                        pLogBuffer,
                                                        bufferSize);
    *pGraphExec = reinterpret_cast<hipGraphExec_t>(handle);
  });
}

hipError_t
hipGraphLaunch(hipGraphExec_t graphExec, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    xrt::core::hip::hip_graph_launch(graphExec, stream);
  });
}

hipError_t
hipGraphExecDestroy(hipGraphExec_t graphExec)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    xrt::core::hip::hip_graph_exec_destroy(graphExec);
  });
}

hipError_t
hipGraphDestroy(hipGraph_t graph)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    xrt::core::hip::hip_graph_destroy(graph);
  });
}
