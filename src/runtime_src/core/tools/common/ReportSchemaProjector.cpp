// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportSchemaProjector.h"

#include "core/common/time.h"

bool
ReportSchemaProjector::emitsJsonDocument(Report::SchemaVersion schemaVersion)
{
  switch (schemaVersion) {
  case Report::SchemaVersion::json_latest:
  case Report::SchemaVersion::json_20202:
    return true;
  default:
    return false;
  }
}

boost::property_tree::ptree
ReportSchemaProjector::makeSchemaVersionNode(Report::SchemaVersion schemaVersion,
                                             bool useJsonVersionNaming)
{
  boost::property_tree::ptree node;
  node.put("schema", Report::getSchemaOutputLabel(schemaVersion, useJsonVersionNaming));
  node.put("creation_date", xrt_core::timestamp());
  return node;
}

boost::property_tree::ptree
ReportSchemaProjector::projectLatest(const boost::property_tree::ptree& canonicalSuperset)
{
  return canonicalSuperset;
}

boost::property_tree::ptree
ReportSchemaProjector::project20202(const boost::property_tree::ptree& canonicalSuperset)
{
  // Frozen legacy ABI: identical to latest.
  return canonicalSuperset;
}

boost::property_tree::ptree
ReportSchemaProjector::project(Report::SchemaVersion target,
                               const boost::property_tree::ptree& canonicalSuperset)
{
  switch (target) {
  case Report::SchemaVersion::json_latest:
    return projectLatest(canonicalSuperset);
  case Report::SchemaVersion::json_20202:
    return project20202(canonicalSuperset);
  default:
    return canonicalSuperset;
  }
}
