/**
 * Copyright (C) 2021, 2022 Xilinx, Inc
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

// Not yet supported on windows, but it will be soon
#ifndef _WIN32


#include "SectionSmartNic.h"

#include "CBOR.h"
#include "RapidJsonUtilities.h"
#include "ResourcesSmartNic.h"
#include "XclBinUtilities.h"
#include <boost/algorithm/hex.hpp>
#include <boost/format.hpp>
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <filesystem>
#include <fstream>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace XUtil = XclBinUtilities;

// Static Variables / Classes
SectionSmartNic::init SectionSmartNic::initializer;

SectionSmartNic::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(SMARTNIC, "SMARTNIC", boost::factory<SectionSmartNic*>());
  sectionInfo->nodeName = "smartnic";

  sectionInfo->supportedAddFormats.push_back(FormatType::json);
  sectionInfo->supportedAddFormats.push_back(FormatType::raw);

  sectionInfo->supportedDumpFormats.push_back(FormatType::json);
  sectionInfo->supportedDumpFormats.push_back(FormatType::html);
  sectionInfo->supportedDumpFormats.push_back(FormatType::raw);

  addSectionType(std::move(sectionInfo));
}

// ----------------------------------------------------------------------------

void
SectionSmartNic::marshalToJSON(char* _pDataSection,
                               unsigned int _sectionSize,
                               boost::property_tree::ptree& _ptree) const
{
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: CBOR Image");
  XUtil::TRACE_BUF("CBOR", _pDataSection, _sectionSize);

  // Wrap the buffer in an input stream.
  std::string dataBuffer(_pDataSection, _sectionSize);
  std::istringstream iStrBuffer(dataBuffer);

  // Transform the CBOR image to JSON
  rapidjson::Document document;
  XclBinUtilities::read_cbor(iStrBuffer, document);

  XUtil::TRACE_PrintTree("SmartNic: Read CBOR", document);

  // Convert from rapidjson to boost
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  document.Accept(writer);

  std::stringstream ss(buf.GetString());
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss, pt);

  // Add child tree
  _ptree.add_child("smartnic", pt);
}


static
void
read_file_into_buffer(const std::string& fileName,
                      const std::string& fromRelativeDir,
                      std::vector<char>& buffer)
{
  // Build the path to our file of interest
  std::filesystem::path filePath = fileName;

  if (filePath.is_relative()) {
    filePath = fromRelativeDir;
    filePath /= fileName;
  }

  // Open the file
  std::ifstream file(filePath.string(), std::ifstream::in | std::ifstream::binary);
  if (!file.is_open())
    throw std::runtime_error("ERROR: Unable to open the file for reading: " + fileName);

  // Get the file size
  file.seekg(0, file.end);
  std::streamsize fileSize = file.tellg();
  file.seekg(0, file.beg);

  // Resize the buffer and read in the array
  buffer.resize(fileSize);
  file.read(buffer.data(), fileSize);

  // Make sure that the entire buffer was read
  if (file.gcount() != fileSize)
    throw std::runtime_error("ERROR: Input stream for the binary buffer is smaller then the expected size.");
}

static
void rename_file_node(const std::string& fileNodeName,
                      rapidjson::Value& value,
                      rapidjson::Document::AllocatorType& allocator)
{
  // Remove the subname (e.g., _file) from the existing key
  static const std::string removeSubName = "_file";

  std::string newKey = fileNodeName;
  auto index = newKey.find(removeSubName);
  if (index == std::string::npos)
    return;

  newKey.replace(index, removeSubName.size(), "");

  XUtil::TRACE(boost::format("Renaming node '%s' to '%s'") % fileNodeName % newKey);

  // Rename keys by swapping their values
  rapidjson::Value::MemberIterator itrOldKey = value.FindMember(fileNodeName.c_str());
  value.AddMember(rapidjson::Value(newKey.c_str(), allocator).Move(), itrOldKey->value, allocator);
  value.RemoveMember(fileNodeName.c_str());
}

static
void
readAndTransposeByteFiles_recursive(const std::string& scope,
                                    rapidjson::Value::MemberIterator itrObject,
                                    const XclBinUtilities::KeyTypeCollection& keyTypeCollection,
                                    rapidjson::Document::AllocatorType& allocator,
                                    const std::string& relativeFromDir)
{
  XUtil::TRACE(boost::format("BScope: %s") % scope);

  // A dictionary
  if (itrObject->value.IsObject()) {
    std::vector<std::string> renameCollection;
    for (rapidjson::Value::MemberIterator itr = itrObject->value.MemberBegin(); itr != itrObject->value.MemberEnd(); ++itr) {

      // Look "forward" to see if this dictionary contains the node of interest
      std::string currentScope = scope + "::" + itr->name.GetString();
      if (get_expected_type(currentScope, keyTypeCollection) == XUtil::DType::byte_file)
        renameCollection.push_back(std::string(itr->name.GetString()));

      readAndTransposeByteFiles_recursive(scope + "::" + itr->name.GetString(), itr, keyTypeCollection, allocator, relativeFromDir);
    }

    // Now that we are out of that "nasty" Iterator loop, rename the updated keys.
    for (const std::string& oldFileNodeName : renameCollection)
      rename_file_node(oldFileNodeName, itrObject->value, allocator);

    return;
  }

  // An array
  if (itrObject->value.IsArray()) {
    for (auto itr = itrObject->value.Begin(); itr != itrObject->value.End(); ++itr) {
      std::vector<std::string> renameCollection;
      rapidjson::Value& attribute = *itr;

      if (!attribute.IsObject())
        continue;

      for (auto itr2 = attribute.MemberBegin(); itr2 != attribute.MemberEnd(); ++itr2) {

        // Look "forward" to see if this dictionary contains the node of interest
        std::string currentScope = scope + "[]::" + itr2->name.GetString();
        if (get_expected_type(currentScope, keyTypeCollection) == XUtil::DType::byte_file)
          renameCollection.push_back(std::string(itr2->name.GetString()));

        readAndTransposeByteFiles_recursive(scope + "[]::" + itr2->name.GetString(), itr2, keyTypeCollection, allocator, relativeFromDir);
      }

      // Now that we are out of that "nasty" Iterator loop, rename the updated keys.
      for (const std::string& oldFileNodeName : renameCollection)
        rename_file_node(oldFileNodeName, attribute, allocator);
    }

    return;
  }

  // End point String
  if (itrObject->value.IsString()) {
    // Do we have a match
    if (get_expected_type(scope, keyTypeCollection) != XUtil::DType::byte_file)
      return;

    // Read file image from disk
    std::vector<char> buffer;
    read_file_into_buffer(itrObject->value.GetString(), relativeFromDir, buffer);

    // Convert the binary data to hex and re-add
    std::string byteString(buffer.data(), buffer.size());
    byteString = boost::algorithm::hex(byteString);
    itrObject->value.SetString(byteString.data(), byteString.size(), allocator);

    return;
  }
}


static void
readAndTransposeByteFiles(rapidjson::Document& doc,
                          const XclBinUtilities::KeyTypeCollection& keyTypeCollection,
                          const std::string& dirRelativeFrom)
{
  if (!doc.IsObject())
    return;

  for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr)
    readAndTransposeByteFiles_recursive(std::string("#") + itr->name.GetString(), itr, keyTypeCollection, doc.GetAllocator(), dirRelativeFrom);

}

void
SectionSmartNic::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                 std::ostringstream& _buf) const
{
  const std::string nodeName = "smartnic";

  XUtil::TRACE("");
  XUtil::TRACE("SmartNic : Marshalling From JSON");

  // -- Retrieve only the JSON tree associated with the SmartNic section
  boost::property_tree::ptree ptSmartNic = _ptSection.get_child(nodeName);

  // -- Convert from a boost property tree to a rapidjson document
  std::ostringstream buf;
  boost::property_tree::write_json(buf, ptSmartNic, false);

  rapidjson::Document document;
  if (document.Parse(buf.str().c_str()).HasParseError()) {
    throw std::runtime_error("Error: The 'smartnic' JSON format is not valid JSON.");
  }

  // -- Transform the JSON elements to their expected primitive value
  XUtil::KeyTypeCollection keyTypeCollection;
  XUtil::collect_key_types(getSmartNicSchema(), keyTypeCollection);

  XUtil::transform_to_primatives(document, keyTypeCollection);

  // -- Validate the smartnic schema
  XUtil::validate_against_schema(nodeName, document, getSmartNicSchema());

  // -- Read in the byte code
  std::filesystem::path p(getPathAndName());
  std::string fromRelativeDir = p.parent_path().string();
  readAndTransposeByteFiles(document, keyTypeCollection, fromRelativeDir);

  // -- Create the cbor buffer
  std::ostringstream buffer;
  XUtil::write_cbor(document, keyTypeCollection, buffer);

  // Write the contents to given output stream
  std::string aString = buffer.str();
  _buf.write(aString.data(), aString.length());
}



/**
 * Compares two given property trees to validate that they are the same.
 *
 * @param primary - Primary property tree
 * @param secondary - Secondary property tree
 */
static void
validateGenericTree(const boost::property_tree::ptree& primary,
                    const boost::property_tree::ptree& secondary)
{
  if (primary.size() != secondary.size())
    throw std::runtime_error("Error: Size mismatch.");

  // If there are no more child graphs, then we are at a graph end node
  if (primary.empty()) {
    XUtil::TRACE(boost::format("  Primary Data   : '%s'") % primary.data());
    XUtil::TRACE(boost::format("  Secondary Data : '%s'") % secondary.data());

    if (primary.data() != secondary.data()) {
      auto errMsg = boost::format("Error: Data mismatch: P('%s'); S('%s')") % primary.data() % secondary.data();
      throw std::runtime_error(errMsg.str());
    }

    return;
  }

  // Compare the keys (order independent)
  for (const auto& it: primary) {
    const std::string& key = it.first;
    XUtil::TRACE(boost::format("Examining node: '%s'") % key);

    const boost::property_tree::ptree& ptSecondary = secondary.get_child(key);
    validateGenericTree(it.second, ptSecondary);
  }
}

// Call back function signature used when traversing the property trees
using node_sig_ptr = std::function<void(boost::property_tree::ptree&, const boost::property_tree::ptree&)>;

// Graph node to call back function
using NodeCallBackFuncs = std::map<std::string, node_sig_ptr>;


/**
 * Used to traverse a node array.
 *
 * @param nodeName The primary node name of the array
 * @param key      Optional key value that is used to determine array item uniqueness
 * @param ptParent Parent property_tree
 * @param ptAppend Data to append
 * @param nodeCallBackFuncs
 *                 Call back functions for various nodes
 */
static void
merge_node_array(const std::string& nodeName, const std::string& key, boost::property_tree::ptree& ptParent,
                 const boost::property_tree::ptree& ptAppend, const NodeCallBackFuncs& nodeCallBackFuncs);

static void
info_node(boost::property_tree::ptree& ptParent,
          const boost::property_tree::ptree& ptAppend)
{
  static const NodeCallBackFuncs emptyCallBackNodes;
  merge_node_array("info", "", ptParent, ptAppend, emptyCallBackNodes);
}

static void
cam_instances_node(boost::property_tree::ptree& ptParent,
                   const boost::property_tree::ptree& ptAppend)
{
  static const NodeCallBackFuncs emptyCallBackNodes;
  merge_node_array("cam_instances", "name", ptParent, ptAppend, emptyCallBackNodes);
}

static void
messages_node(boost::property_tree::ptree& ptParent,
              const boost::property_tree::ptree& ptAppend)
{
  static const NodeCallBackFuncs emptyCallBackNodes;
  merge_node_array("messages", "name", ptParent, ptAppend, emptyCallBackNodes);
}

static void
resource_classes_node(boost::property_tree::ptree& ptParent,
                      const boost::property_tree::ptree& ptAppend)
{
  static const NodeCallBackFuncs emptyCallBackNodes;
  merge_node_array("resource_classes", "name", ptParent, ptAppend, emptyCallBackNodes);
}

static void
merge_node(boost::property_tree::ptree& ptParent,
           const std::string& appendPath,
           const boost::property_tree::ptree& ptAppend,
           const NodeCallBackFuncs& nodeCallBackFuncs)
{
  XUtil::TRACE(boost::format("Current append path: '%s'") % appendPath);

  // Are we at a graph end node
  if (!appendPath.empty() && ptAppend.empty()) {
    // Check to see if data already exists, if so validate that it isn't changing
    const auto& parentValue = ptParent.get<std::string>(appendPath, "");
    const auto& appendValue = ptAppend.data();

    if (!parentValue.empty()) {
      // Check to see if the data is the same, if not produce an error
      if (parentValue.compare(appendValue) != 0) {
        auto errMsg = boost::format("Error: The JSON path's '%s' existing value is not the same as the value being merged.\n"
                                    "Existing value    : '%s'\n"
                                    "Value being merged: '%s'") % appendPath % parentValue % appendValue;
        throw std::runtime_error(errMsg.str());
      }
    } else {
      // Entry does not exist add it
      ptParent.put(appendPath, ptAppend.data());
    }
    return;
  }

  // Merge the node metadata
  for (const auto& item : ptAppend) {
    // Encode the current path using boost's determinator (e.g., '.')
    const std::string currentPath = appendPath + (appendPath.empty() ? "" : ".") + item.first;

    // Check to see if this node has a callback function, if so call it.
    auto itr = nodeCallBackFuncs.find(currentPath);
    if (itr != nodeCallBackFuncs.end()) {
      // Create a parent node if one doesn't exist
      if (!appendPath.empty() && (ptParent.count(appendPath) == 0)) {
        boost::property_tree::ptree ptEmpty;
        ptParent.add_child(appendPath, ptEmpty);
      }

      // Call the helper function
      itr->second(ptParent.get_child(appendPath), item.second);
      continue;
    }

    // No call back function, this is a generic node
    merge_node(ptParent, currentPath, item.second, nodeCallBackFuncs);
  }
}

static void
merge_node_array(const std::string& nodeName,
                 const std::string& key,
                 boost::property_tree::ptree& ptParent,
                 const boost::property_tree::ptree& ptAppend,
                 const NodeCallBackFuncs& nodeCallBackFuncs)
{
  // Extract the node array into a vector of child property trees
  auto workingNodeArray = XUtil::as_vector<boost::property_tree::ptree>(ptParent, nodeName);

  // Remove this entry.  It will be added later.
  ptParent.erase(nodeName);

  // Merge the new data into the extensions node
  for (const auto& item : ptAppend) {
    bool entryMerged = false;

    // Check to see if a key is needed, if so, used it to find the correct unique entry
    if (!key.empty()) {
      const std::string& keyValue = item.second.get<std::string>(key, "");
      if (keyValue.empty()) {
        auto errMsg = boost::format("Error: Missing key '%s' entry.") % key;
        throw std::runtime_error(errMsg.str());
      }

      // Look for an existing entry that matches the key value (if used0
      for (auto& entry : workingNodeArray) {
        if (keyValue == entry.get<std::string>(key, "")) {
          merge_node(entry, "", item.second, nodeCallBackFuncs);
          entryMerged = true;
          break;
        }
      }
    }

    // No match, add it to the array
    if (entryMerged == false) {
      XUtil::TRACE("New append item.  Adding it to the end of the array.");
      workingNodeArray.push_back(item.second);
    }
  }

  // Rebuild the node array and add it back into the property tree
  boost::property_tree::ptree ptArrayNode;
  for (auto& nodeEntry : workingNodeArray)
    ptArrayNode.push_back({ "", nodeEntry });   // Used to make an array of objects

  ptParent.add_child(nodeName, ptArrayNode);
}



void
SectionSmartNic::appendToSectionMetadata(const boost::property_tree::ptree& _ptAppendData,
                                         boost::property_tree::ptree& _ptToAppendTo)
{
  XUtil::TRACE_PrintTree("To Append To", _ptToAppendTo);
  XUtil::TRACE_PrintTree("Append data", _ptAppendData);

  // Should not happen, but we should double check just in case of a future change :^)
  if (_ptToAppendTo.count("smartnic") == 0)
    throw std::runtime_error("Internal Error: SmartNic destination node not present.");

  boost::property_tree::ptree& ptSmartNic = _ptToAppendTo.get_child("smartnic");

  // Examine the data to be merged
  boost::property_tree::ptree ptEmpty;           // Empty property tree
  for (const auto& it : _ptAppendData) {
    const std::string& sectionName = it.first;
    XUtil::TRACE("");
    XUtil::TRACE("Found Section: '" + sectionName + "'");

    // -- Node: extensions
    if (sectionName == "extensions") {
      try {
        // Call back functions and their nodes that they are associated with
        static NodeCallBackFuncs extensionCallBackNodes = {
          { "info", info_node },
          { "cam_instances", cam_instances_node },
          { "messages", messages_node },
          { "resource_classes", resource_classes_node }
        };

        merge_node_array("extensions", "instance_name", ptSmartNic, it.second, extensionCallBackNodes);

      } catch (std::exception& e) {
        std::string msg = e.what();
        std::cerr << e.what() << std::endl;
        throw std::runtime_error("Error: Merging of the 'extension' node failed.");
      }
      continue;
    }

    // -- Node: softhubs
    if (sectionName == "softhubs") {
      static const NodeCallBackFuncs emptyCallBackNodes;
      merge_node_array("softhubs", "id", ptSmartNic, it.second, emptyCallBackNodes);
      continue;
    }

    // -- Node: schema_version
    if (sectionName == "schema_version") {
      try {
        validateGenericTree(it.second, ptSmartNic.get_child("schema_version"));
      } catch (std::exception& e) {
        std::string msg = e.what();
        std::cerr << e.what() << std::endl;
        throw std::runtime_error("Error: Validating node 'schema_versions'");
      }
      continue;
    }

    // -- Node: <not known>
    throw std::runtime_error(std::string("Error: Unknown node in merging file: '") + sectionName + "'");
  }

  XUtil::TRACE_PrintTree("Final Merge", _ptToAppendTo);
}

#endif
