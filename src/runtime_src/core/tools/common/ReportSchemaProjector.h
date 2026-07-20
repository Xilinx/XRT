// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#ifndef ReportSchemaProjector_h_
#define ReportSchemaProjector_h_

#include "Report.h"

#include <boost/property_tree/ptree.hpp>

// Projects the canonical xrt-smi property-tree superset onto a frozen JSON ABI.
class ReportSchemaProjector {
 public:
  static bool emitsJsonDocument(Report::SchemaVersion schemaVersion);

  static boost::property_tree::ptree makeSchemaVersionNode(Report::SchemaVersion schemaVersion,
                                                           bool useJsonVersionNaming);

  static boost::property_tree::ptree project(Report::SchemaVersion target,
                                             const boost::property_tree::ptree& canonicalSuperset);

 private:
  static boost::property_tree::ptree projectLatest(const boost::property_tree::ptree& canonicalSuperset);
  static boost::property_tree::ptree project20202(const boost::property_tree::ptree& canonicalSuperset);
};

#endif
