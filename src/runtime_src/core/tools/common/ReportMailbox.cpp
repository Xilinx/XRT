/**
 * Copyright (C) 2021 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportMailbox.h"
#include "core/common/query_requests.h"

//from mailboxproto.h
static std::map <int, std::string> enum_string_map = {
  { 0, "Unknown" },
  { 1, "Test msg ready" },
  { 2, "Test msg fetch" },
  { 3, "Lock bitstream" },
  { 4, "Unlock bitstream" },
  { 5, "Hot reset" },
  { 6, "Firewall trip" },
  { 7, "Download xclbin kaddr" },
  { 8, "Download xclbin" },
  { 9, "Reclock" },
  { 10, "Peer data read" },
  { 11, "User probe" },
  { 12, "Mgmt state" },
  { 13, "Change shell" },
  { 14, "Reprogram shell" },
  { 15, "P2P bar addr" }
};

static boost::property_tree::ptree
parse_mailbox_requests(const std::vector<std::string>& blob)
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree pt_requests;
  for(const auto& val : blob) {
    if(val.find("raw bytes") != std::string::npos) {
      auto raw_pos = val.substr(val.find(':') + 1);
      auto raw_bytes = std::stoll(raw_pos);
      pt.put("raw_bytes", raw_bytes);
      continue;
    }

    boost::property_tree::ptree pt_req;
    //find request type
    auto req_pos = val.find('[') + 1;
    auto type = std::stoi(val.substr(req_pos));
    pt_req.put("description", enum_string_map[type]);
    
    //find number of msgs
    auto msg_pos = val.find(':') + 1;
    auto count = std::stoi(val.substr(msg_pos));
    pt_req.put("msg_count", count);

    pt_requests.push_back( std::make_pair("", pt_req) );
  }
  pt.put_child("requests", pt_requests);
  return pt;
}

void
ReportMailbox::getPropertyTreeInternal( const xrt_core::device * device,
                                         boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
}

void 
ReportMailbox::getPropertyTree20202( const xrt_core::device * device,
                                       boost::property_tree::ptree &pt) const
{
  // There can only be 1 root node
  pt.add_child("mailbox", parse_mailbox_requests(xrt_core::device_query<xrt_core::query::mailbox_metrics>(device)));
}


void 
ReportMailbox::writeReport(const xrt_core::device * device,
                            const std::vector<std::string> & /*_elementsFilter*/,
                            std::iostream & output) const
{
  boost::property_tree::ptree pt;
  boost::property_tree::ptree empty_ptree;
  try {
    getPropertyTreeInternal(device, pt);
  } catch (...) {}

  output << "Mailbox\n";
  if (pt.empty()) {
    output << "  Information unavailable" << std::endl; 
    return;
  }
  boost::property_tree::ptree& mailbox = pt.get_child("mailbox.requests", empty_ptree);
  output << boost::format("  %-22s : %s Bytes\n") % "Total bytes received" % pt.get<std::string>("mailbox.raw_bytes");
  for(auto& kv : mailbox) {
    boost::property_tree::ptree& pt_temp = kv.second;
    output << boost::format("  %-22s : %-2d\n") % pt_temp.get<std::string>("description") % pt_temp.get<int>("msg_count");
  }
  output << std::endl;

}


