// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "hip/core/common.h"
#include "hip/core/event.h"
#include "hip/core/graph.h"
#include "hip/core/stream.h"
#include "hip/hip_xrt.h"

#include <iostream>

namespace xrt::core::hip {

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

  // Add dependencies if provided
  if (pDependencies && numDependencies) {
    auto node = hip_graph->get_node(node_hdl);
    for (size_t i = 0; i < numDependencies; ++i)
      node->add_dep_node(hip_graph->get_node(pDependencies[i]));
  }

  return node_hdl;
}

// TODO: Implement error reporting and logging for graph instantiation:
//   - pErrorNode: Pointer to error node if error occured during graph instantiation
//   - pLogBuffer, bufferSize: log messages about instantiation to the buffer
static command_handle
hip_graph_instantiate(hipGraph_t g, hipGraphNode_t* /*pErrorNode*/,
                      char* /*pLogBuffer*/, size_t /*bufferSize*/)
{
  throw_invalid_resource_if(!g, "graph is nullptr");

  auto hip_graph = graph_cache.get_or_error(g);
  throw_invalid_resource_if(!hip_graph, "invalid graph passed");

  return insert_in_map(command_cache, std::make_shared<graph_exec>(hip_graph));
}

static void
hip_graph_launch(hipGraphExec_t gE, hipStream_t stream)
{
  throw_invalid_resource_if(!gE, "graph exec is nullptr");

  auto s_hdl = get_stream(stream).get();
  s_hdl->enqueue(command_cache.get_or_error(gE));
}

static void
hip_graph_exec_destroy(hipGraphExec_t gE)
{
  throw_invalid_resource_if(!gE, "graph exec is nullptr");

  command_cache.remove(reinterpret_cast<command_handle>(gE));
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
