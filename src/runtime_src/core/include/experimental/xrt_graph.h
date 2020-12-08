/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XRT_GRAPH_H_
#define _XRT_GRAPH_H_

#include "xrt.h"
#include "experimental/xrt_uuid.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"

typedef void *xrtGraphHandle;

#ifdef __cplusplus

namespace xrt {

/*!
 * @class graph
 *
 * The graph object represents an abstraction exported by aietool matching
 * a specified name.
 * The graph is created by finding matching graph name in the currently
 * loaded xclbin.
 */
class graph_impl;
class graph
{
public:
  /**
   * graph() - Constructor from a device, xclbin and graph name
   *
   * @param device
   *  Device on which the graph should execute
   * @param xclbin_id
   *  UUID of the xclbin with the graph
   * @param name
   *  Name of graph to construct
   */
  graph(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name);

  /**
   * reset() - Reset a graph.
   *
   * Reset graph by disabling tiles and enable tiles reset
   */
  void
  reset() const;

  /**
   * get_timestamp() - Get timestamp of a graph.
   *
   * @return
   * Timestamp in AIE cycle
   */
  uint64_t
  get_timestamp() const;

  /**
   * run() - Start graph execution.
   *
   * Start the graph execution by default. Run forever; Or run a fixed
   * number of iterations if specified during compilation time.
   */
  void
  run();

  /**
   * run() - Start graph execution.
   *
   * @param iterations
   *  Number of iterations the graph should run.
   */
  void
  run(uint32_t iterations);

  /**
   * wait_done() - Wait for specified milliseconds for graph to complete.
   *
   * @param timeout_ms
   *  Timeout in milliseconds
   *
   * Wait for done status for all the tiles in graph.
   *
   * The current thread will block until graph run completes or timeout.
   */
  void
  wait_done(int timeout_ms);

  /**
   * wait() - Wait for graph to complete.
   *
   * Busy wait until graph is done.
   *
   * The current thread will block until graph run completes.
   */
  void
  wait();

  /**
   * wait() - Wait for graph to complete for specified AIE cycles.
   *
   * @param cycles
   *  AIE cycles to wait since last run starts.
   *
   * Wait a given AIE cycle since the last graph run and then pause the
   * graph. If graph already runs more than the given cycles, stop the
   * graph immediately.
   *
   * This API with non-zero AIE cycle is for grahp that is running forever
   * or graph that has multi-rate core(s); zero AIE cycle means busy wait
   * until graph is done. It has the same effect as wait() API.
   *
   * The current thread will block until graph run completes or is paused.
   */
  void
  wait(uint64_t cycles);

  /**
   * suspend() - Suspend a running graph.
   *
   * Suspend graph execution.
   */
  void
  suspend();

  /**
   * resume() - Resume a suspended graph.
   *
   * Resume graph execution which was paused by suspend() or wait(cycles) APIs
   */
  void
  resume();

  /**
   * end() - Busy wait for graph to complete and terminate the graph.
   *
   * When terminating, all active processors exit their main thread and
   * disable themselves.
   *
   * The current thread will block until graph is terminated.
   */
  void
  end();

 /**
   * end() - Wait for graph to complete for specified AIE cycles and then
   * terminate the graph.
   *
   * @param cycles
   *  AIE cycles to wait since last run starts.
   *
   * Wait a given AIE cycle since the last graph run and then terminate
   * the graph. If graph already runs more than the given cycles, terminate
   * the graph immediately.
   *
   * This API with non-zero AIE cycle is for graph that is running forever
   * or graph that has multi-rate core(s); zero AIE cycle means busy wait
   * until graph is done. It has the same effect as end() API.
   *
   * The current thread will block until graph is terminated.
   */
  void
  end(uint64_t cycles);

  /**
   * update() - Update graph Run Time Parameters.
   *
   * @param port_name
   *  Hierarchical name of RTP port.
   * @param arg
   *  The argument to set.
   */
  template<typename ArgType>
  void
  update(const std::string& port_name, ArgType&& arg)
  {
    update_port(port_name, &arg, sizeof(arg));
  }

  /**
   * read() - Read graph Run Time Parameters value.
   *
   * @param port_name
   *  Hierarchical name of RTP port.
   * @param arg
   *  The RTP value is written to.
   */
  template<typename ArgType>
  void
  read(const std::string& port_name, ArgType& arg)
  {
    read_port(port_name, &arg, sizeof(arg));
  }

private:
  std::shared_ptr<graph_impl> handle;

  void
  update_port(const std::string& port_name, const void* value, size_t bytes);

  void
  read_port(const std::string& port_name, void* value, size_t bytes);
};

} // namespace xrt

/// @cond
extern "C" {

#endif

/**
 * xrtGraphOpen() - Open a graph and obtain its handle.
 *
 * @handle:       Handle to the device with the graph.
 * @xclbinUUID:   UUID of the xclbin with the specified graph.
 * @graphNmae:    The name of graph to be open.
 * Return:        Handle to representing the graph. NULL for error.
 *
 * An xclbin with the specified graph must have been loaded prior
 * to calling this function.
 */
xrtGraphHandle
xrtGraphOpen(xrtDeviceHandle handle, const xuid_t xclbinUUID, const char *graphName);

/**
 * xrtGraphClose() - Close an open graph.
 *
 * @gh:            Handle to graph previously opened with xrtGraphOpen.
 *
 */
void
xrtGraphClose(xrtGraphHandle gh);

/**
 * xrtGraphReset() - Reset a graph.
 *
 * @gh:            Handle to graph previously opened with xrtGraphOpen.
 * Return:         0 on success, or appropriate error number
 *
 * Note: Reset by disable tiles and enable tile reset
 */
int
xrtGraphReset(xrtGraphHandle gh);

/**
 * xrtGraphTimeStamp() - Get timestamp of a graph. The unit of timestamp is
 *                       AIE Cycle.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          Timestamp in AIE cycle.
 */
uint64_t
xrtGraphTimeStamp(xrtGraphHandle gh);

/**
 * xrtGraphRun() - Start a graph execution
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * @iterations:     The run iteration to update to graph.
 *                  0 for default or previous set iterations
 *                  -1 for run forever
 * Return:          0 on success, or appropriate error number
 *
 * Note: Run by enable tiles and disable tile reset
 */
int
xrtGraphRun(xrtGraphHandle gh, int iterations);

/**
 * xrtGraphWaitDone() - Wait for graph to be done. If the graph is not
 *                      done in a given time, bail out with timeout.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @timeoutMilliSec: Timeout value to wait for graph done.
 *
 * Return:          0 on success, -ETIME on timeout,
 *                  or appropriate error number.
 *
 * Note: Wait for done status of ALL the tiles
 */
int
xrtGraphWaitDone(xrtGraphHandle gh, int timeoutMilliSec);

/**
 * xrtGraphWait() -  Wait a given AIE cycle since the last xrtGraphRun and
 *                   then stop the graph. If cycle is 0, busy wait until graph
 *                   is done. If graph already run more than the given
 *                   cycle, stop the graph immediateley.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @cycle:           AIE cycle should wait since last xrtGraphRun. 0 for
 *                   wait until graph is done.
 *
 * Return:          0 on success, or appropriate error number.
 *
 * Note: This API with non-zero AIE cycle is for graph that is running
 * forever or graph that has multi-rate core(s).
 */
int
xrtGraphWait(xrtGraphHandle gh, uint64_t cycle);

/**
 * xrtGraphSuspend() - Suspend a running graph.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          0 on success, or appropriate error number.
 */
int
xrtGraphSuspend(xrtGraphHandle gh);

/**
 * xrtGraphResume() - Resume a suspended graph.
 *
 * @gh:             Handle to graph previously opened with xrtGraphOpen.
 * Return:          0 on success, or appropriate error number.
 */
int
xrtGraphResume(xrtGraphHandle gh);

/**
 * xrtGraphEnd() - Wait a given AIE cycle since the last xrtGraphRun and
 *                 then end the graph. If cycle is 0, busy wait until graph
 *                 is done before end the graph. If graph already run more
 *                 than the given cycle, stop the graph immediately and end it.
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @cycle:           AIE cycle should wait since last xrtGraphRun. 0 for
 *                   wait until graph is done.
 *
 * Return:          0 on success, or appropriate error number.
 *
 * Note: This API with non-zero AIE cycle is for graph that is running
 * forever or graph that has multi-rate core(s).
 */
int
xrtGraphEnd(xrtGraphHandle gh, uint64_t cycle);

/**
 * xrtGraphUpdateRTP() - Update RTP value of port with hierarchical name
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @hierPathPort:    hierarchial name of RTP port.
 * @buffer:          pointer to the RTP value.
 * @size:            size in bytes of the RTP value.
 *
 * Return:          0 on success, -1 on error.
 */
int
xrtGraphUpdateRTP(xrtGraphHandle gh, const char *hierPathPort, const char *buffer, size_t size);

/**
 * xrtGraphReadRTP() - Read RTP value of port with hierarchical name
 *
 * @gh:              Handle to graph previously opened with xrtGraphOpen.
 * @hierPathPort:    hierarchial name of RTP port.
 * @buffer:          pointer to the buffer that RTP value is copied to.
 * @size:            size in bytes of the RTP value.
 *
 * Return:          0 on success, -1 on error.
 *
 * Note: Caller is reponsible for allocating enough memory for RTP value
 *       being copied to.
 */
int
xrtGraphReadRTP(xrtGraphHandle gh, const char *hierPathPort, char *buffer, size_t size);

/// @endcond

#ifdef __cplusplus
}
#endif

#endif
