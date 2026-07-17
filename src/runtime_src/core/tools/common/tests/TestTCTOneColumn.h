// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef TestTCTOneColumn_h_
#define TestTCTOneColumn_h_

#include "tools/common/TestRunner.h"
#include "xrt/xrt_device.h"

/**
 * Measure average Tile Completion Token (TCT) latency and throughput with one
 * AIE column active.
 *
 * On Strix/PHX (AIE2), the workload uses an xclbin plus ELF and applies a
 * fixed per-ELF token count when deriving metrics from runner elapsed time.
 *
 * On MDS (NPU3 / AIE4), the workload follows the same overall pattern as the
 * df-bw validate test: the runner loads the TCT ELF, executes it repeatedly,
 * and reports elapsed time, average latency, and throughput. Differences from
 * df-bw are:
 *   - Each ELF run transfers a 256 KiB buffer (two 256 KiB bindings in the
 *     recipe/profile).
 *   - The profile runs 1000 iterations; metrics are derived from runner
 *     elapsed time and iteration count (no fixed token/sample assumption).
 *   - Completion is detected via the TCT opcode rather than a MASK poll.
 *
 * The host-side test reports average TCT latency (us) and TCT throughput
 * (TCT/s) from the runner report.
 */
class TestTCTOneColumn : public TestRunner {
  public:
    boost::property_tree::ptree run(const std::shared_ptr<xrt_core::device>&, const xrt_core::archive*) override;

  public:
    TestTCTOneColumn();
};

#endif
