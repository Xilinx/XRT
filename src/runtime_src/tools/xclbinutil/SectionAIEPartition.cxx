/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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

#include "SectionAIEPartition.h"

#include "XclBinUtilities.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/functional/factory.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>

namespace XUtil = XclBinUtilities;
namespace fs = std::filesystem;

// -------------------------------------------------------------------------

// Static Variables / Classes
SectionAIEPartition::init SectionAIEPartition::initializer;

SectionAIEPartition::init::init()
{
  auto sectionInfo = std::make_unique<SectionInfo>(AIE_PARTITION, "AIE_PARTITION", boost::factory<SectionAIEPartition*>());
  sectionInfo->nodeName = "aie_partition";
  sectionInfo->supportsSubSections = true;
  sectionInfo->supportsIndexing = true;

  // There is only one-subsection that is supported.  By default it is not named.
  sectionInfo->subSections.push_back("");

  sectionInfo->supportedAddFormats.push_back(FormatType::json);
  sectionInfo->supportedDumpFormats.push_back(FormatType::json);

  addSectionType(std::move(sectionInfo));
}

// -------------------------------------------------------------------------

class SectionHeap {
 public:
  SectionHeap() = delete;
  SectionHeap(uint64_t heapSectionOffset)
  {
    if (XUtil::bytesToAlign(heapSectionOffset) != 0)
      throw std::runtime_error("Error: HeapSectionOffset is not aligned to 8 bytes");

    m_heapSectionOffset = heapSectionOffset;
  }

  void write(const char* pBuffer, size_t size, bool align = true)
  {
    if ((pBuffer != nullptr) && size)
      m_heapBuffer.write(pBuffer, size);

    if (align)
      XUtil::alignBytes(m_heapBuffer, 8);
  }

  uint64_t getNextBufferOffset()
  {
    // Get the current size
    m_heapBuffer.seekp(0, std::ios_base::end);
    uint64_t bufSize = (uint64_t)m_heapBuffer.tellp();

    // And add it to the buffer offset.
    return bufSize + m_heapSectionOffset;
  }

  void writeHeapToStream(std::ostringstream& osStream)
  {
    const auto& sHeap = m_heapBuffer.str();
    osStream.write(sHeap.c_str(), sHeap.size());
  }

 protected:
  uint64_t m_heapSectionOffset;
  std::ostringstream m_heapBuffer;
};

// ----------------------------------------------------------------------------

bool
SectionAIEPartition::subSectionExists(const std::string& /*sSubSectionName*/) const
{
  // No buffer no subsections
  return  (m_pBuffer != nullptr);
}

// -------------------------------------------------------------------------

static const std::vector<std::pair<std::string, CDO_Type>> CTTypes = {
  { "UNKNOWN", CT_UNKNOWN },
  { "PRIMARY", CT_PRIMARY },
  { "LITE", CT_LITE },
  { "PRE_POST", CT_PREPOST }
};

static const std::string&
getCDOTypeStr(CDO_Type eCDOType)
{
  auto iter = std::find_if(CTTypes.begin(), CTTypes.end(), [&](const auto& entry) {return entry.second == eCDOType;});

  if (iter == CTTypes.end())
    return getCDOTypeStr(CT_UNKNOWN);

  return iter->first;
}

static CDO_Type
getCDOTypesEnum(const std::string& sTypeName)
{
  auto iter = std::find_if(CTTypes.begin(), CTTypes.end(), [&](const auto& entry) {return boost::iequals(entry.first, sTypeName);});

  if (iter == CTTypes.end())
    return CT_UNKNOWN;

  return iter->second;
}

// -------------------------------------------------------------------------

static void
process_PDI_uuid(const boost::property_tree::ptree& ptPDI,
                 aie_pdi& aieParitionPDI)
{
  XUtil::TRACE("Processing PDI UUID");

  auto uuid = ptPDI.get<std::string>("uuid", "");
  if (uuid.empty())
    throw std::runtime_error("Error: The PDI element is missing the 'uuid' node.");

  // Clean up the UUID
  boost::replace_all(uuid, "0x", "");  // Remove all "0x"
  boost::replace_all(uuid, "-", "");   // Remove all minus signs

  if (uuid.size() < (2 * sizeof(aie_pdi::uuid))) // Add leading zeros
    uuid.insert(0, (2 * sizeof(aie_pdi::uuid) - uuid.size()), '0');

  if (uuid.size() > (2 * sizeof(aie_pdi::uuid)))
    throw std::runtime_error("Error: The UUID node value is larger then the storage size for this value.");

  // Write out the value
  XUtil::hexStringToBinaryBuffer(uuid, reinterpret_cast<unsigned char*>(aieParitionPDI.uuid), sizeof(aie_pdi::uuid));
}

// -------------------------------------------------------------------------

static void
read_file_into_buffer(const std::string& fileName,
                      const fs::path& fromRelativeDir,
                      std::vector<char>& buffer)
{
  // Build the path to our file of interest
  fs::path filePath = fileName;

  if (filePath.is_relative()) {
    filePath = fromRelativeDir;
    filePath /= fileName;
  }

  XUtil::TRACE(boost::format("Reading in the file: '%s'") % filePath.string());

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

// -------------------------------------------------------------------------

static void
process_PDI_file(const boost::property_tree::ptree& ptAIEPartitionPDI,
                 const fs::path& relativeFromDir,
                 aie_pdi& aiePartitionPDI,
                 SectionHeap& heap)
{
  XUtil::TRACE("Processing PDI Files");

  const auto& fileName = ptAIEPartitionPDI.get<std::string>("file_name", "");
  if (fileName.empty())
    throw std::runtime_error("Error: Missing PDI file name node.");

  // Read file image from disk
  std::vector<char> buffer;
  read_file_into_buffer(fileName, relativeFromDir, buffer);

  // Store file image in the heap
  aiePartitionPDI.pdi_image.size = static_cast<decltype(aiePartitionPDI.pdi_image.size)>(buffer.size());
  aiePartitionPDI.pdi_image.offset = static_cast<decltype(aiePartitionPDI.pdi_image.offset)>(heap.getNextBufferOffset());
  heap.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

// -------------------------------------------------------------------------

static void
process_pre_cdo_groups(const boost::property_tree::ptree& ptAIECDOGroup,
                       cdo_group& aieCDOGroup,
                       SectionHeap& heap)
{
  XUtil::TRACE("Processing Pre CDO Groups");

  std::vector<std::string> preCDOGroups = XUtil::as_vector_simple<std::string>(ptAIECDOGroup, "pre_cdo_groups");
  aieCDOGroup.pre_cdo_groups.size = static_cast<decltype(aieCDOGroup.pre_cdo_groups.size)>(preCDOGroups.size());

  // It is O.K. not to have any CDO groups
  if (aieCDOGroup.pre_cdo_groups.size == 0)
    return;

  // Record where the CDO group array starts.
  aieCDOGroup.pre_cdo_groups.offset = static_cast<decltype(aieCDOGroup.pre_cdo_groups.offset)>(heap.getNextBufferOffset());

  // Write out the CDO value to the heap.
  for (const auto& element : preCDOGroups) {
    uint64_t preGroupID = std::strtoul(element.c_str(), NULL, 0);
    heap.write(reinterpret_cast<const char*>(&preGroupID), sizeof(decltype(preGroupID)), false /*align*/);
  }

  // Align the heap to the next 64-bit word.
  heap.write(nullptr, 0);
}

// -------------------------------------------------------------------------

static void
process_PDI_cdo_groups(const boost::property_tree::ptree& ptAIEPartitionPDI,
                       aie_pdi& aiePDI,
                       SectionHeap& heap)
{
  XUtil::TRACE("Processing CDO Groups");

  std::vector<boost::property_tree::ptree> ptCDOs = XUtil::as_vector<boost::property_tree::ptree>(ptAIEPartitionPDI, "cdo_groups");
  aiePDI.cdo_groups.size = static_cast<decltype(aiePDI.cdo_groups.size)>(ptCDOs.size());

  if (aiePDI.cdo_groups.size == 0)
    throw std::runtime_error("Error: There are no cdo groups in the PDI node AIE Partition section.");

  // Examine each of the CDO groups
  std::vector<cdo_group> vCDOs;
  for (const auto& element : ptCDOs) {
    cdo_group aieCDOGroup = { };

    // Name
    auto name = element.get<std::string>("name", "");
    aieCDOGroup.mpo_name = static_cast<decltype(aieCDOGroup.mpo_name)>(heap.getNextBufferOffset());
    heap.write(reinterpret_cast<const char*>(name.c_str()), name.size() + 1 /*Null CharL*/);

    // Type
    aieCDOGroup.cdo_type = static_cast<decltype(aieCDOGroup.cdo_type)>(getCDOTypesEnum(element.get<std::string>("type", "")));

    // PDI ID
    auto sPDIValue = element.get<std::string>("pdi_id", "");
    if (sPDIValue.empty())
      throw std::runtime_error("Error: PDI ID node value not found");

    aieCDOGroup.pdi_id = std::strtoul(sPDIValue.c_str(), NULL, 0);

    // DPU Function IDs (optional)
    std::vector<std::string> dpuKernelIDs = XUtil::as_vector_simple<std::string>(element, "dpu_kernel_ids");
    aieCDOGroup.dpu_kernel_ids.size = static_cast<decltype(aieCDOGroup.dpu_kernel_ids.size)>(dpuKernelIDs.size());

    if (aieCDOGroup.dpu_kernel_ids.size != 0) {
      // Record where the dpu kernel IDs array starts
      aieCDOGroup.dpu_kernel_ids.offset = static_cast<decltype(aieCDOGroup.dpu_kernel_ids.offset)>(heap.getNextBufferOffset());

      // Write out the DPU values to the heap.
      for (const auto& kernelID : dpuKernelIDs) {
        uint64_t dpuKernelID = std::strtoul(kernelID.c_str(), NULL, 0);
        heap.write(reinterpret_cast<const char*>(&dpuKernelID), sizeof(decltype(dpuKernelID)), false /*align*/);
      }

      // Align the heap to the next 64-bit word.
      heap.write(nullptr, 0);
    }

    // PRE CDO Groups (optional)
    process_pre_cdo_groups(element, aieCDOGroup, heap);

    // Finished processing the element.  Save it away.
    vCDOs.push_back(aieCDOGroup);
  }

  // Write out the CDO group array.  Note: The CDO group element is 64-bit aligned.
  aiePDI.cdo_groups.offset = static_cast<decltype(aiePDI.cdo_groups.offset)>(heap.getNextBufferOffset());
  for (const auto& element : vCDOs)
    heap.write(reinterpret_cast<const char*>(&element), sizeof(cdo_group));
}

// -------------------------------------------------------------------------

static void
process_PDIs(const boost::property_tree::ptree& ptAIEPartition,
             const fs::path& relativeFromDir,
             aie_partition& aiePartitionHdr,
             SectionHeap& heap)
{
  XUtil::TRACE("Processing PDIs");

  std::vector<boost::property_tree::ptree> ptPDIs = XUtil::as_vector<boost::property_tree::ptree>(ptAIEPartition, "PDIs");
  aiePartitionHdr.aie_pdi.size = static_cast<decltype(aiePartitionHdr.aie_pdi.size)>(ptPDIs.size());

  if (aiePartitionHdr.aie_pdi.size == 0)
    throw std::runtime_error("Error: There are no PDI nodes in the AIE Partition section.");

  // Examine each of the PDI entries
  std::vector<aie_pdi> vPDIs;
  for (const auto& element : ptPDIs) {
    aie_pdi aiePartitionPDI = { };

    process_PDI_uuid(element, aiePartitionPDI);
    process_PDI_file(element, relativeFromDir, aiePartitionPDI, heap);
    process_PDI_cdo_groups(element, aiePartitionPDI, heap);

    // Finished processing the element.  Save it away.
    vPDIs.push_back(aiePartitionPDI);
  }

  // Write out the PDI array.  Note: The PDI elements are 64-bit aligned.
  aiePartitionHdr.aie_pdi.offset = static_cast<decltype(aiePartitionHdr.aie_pdi.offset)>(heap.getNextBufferOffset());
  for (const auto& element : vPDIs)
    heap.write(reinterpret_cast<const char*>(&element), sizeof(aie_pdi));
}

// -------------------------------------------------------------------------

static void
process_partition_info(const boost::property_tree::ptree& ptAIEPartition,
                       aie_partition_info& partitionInfo,
                       SectionHeap& heap)
{
  XUtil::TRACE("Processing partition info");
  static const boost::property_tree::ptree ptEmpty;

  // Get the information regarding the relocatable partitions
  const auto& ptPartition = ptAIEPartition.get_child("partition", ptEmpty);
  if (ptPartition.empty())
    throw std::runtime_error("Error: The AIE partition is missing the 'partition' node.");

  partitionInfo = { };

  // Parse the column's width
  partitionInfo.column_width = ptPartition.get<uint16_t>("column_width", 0);
  if (partitionInfo.column_width == 0)
    throw std::runtime_error("Error: Missing AIE partition column width");

  // Parse the start columns
  std::vector<uint16_t> startColumns = XUtil::as_vector_simple<uint16_t>(ptPartition, "start_columns");
  partitionInfo.start_columns.size = static_cast<decltype(partitionInfo.start_columns.size)>(startColumns.size());

  if (partitionInfo.start_columns.size == 0)
    throw std::runtime_error("Error: Missing start columns for the AIE partition.");

  // Write out the 16-bit array.
  partitionInfo.start_columns.offset = static_cast<decltype(partitionInfo.start_columns.offset)>(heap.getNextBufferOffset());
  // XUtil::TRACE(boost::str(boost::format("  start_column (0x%lx)") % partitionInfo.start_columns.offset));
  for (const auto element : startColumns)
    heap.write(reinterpret_cast<const char*>(&element), sizeof(uint16_t), false /*align*/);

  // Align the heap to the next 64 bit word
  heap.write(nullptr, 0, true /*align*/);
}

// -------------------------------------------------------------------------

static void
createAIEPartitionImage(const std::string& sectionIndexName,
                        const fs::path& relativeFromDir,
                        std::istream& iStream,
                        std::ostringstream& osBuffer)
{
  errno = 0;       // Reset any strtoul errors that might have occurred elsewhere

  // Get the boost property tree
  iStream.clear();
  iStream.seekg(0);
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(iStream, pt);
  XUtil::TRACE_PrintTree("AIE_PARTITION", pt);

  const boost::property_tree::ptree& ptAIEPartition = pt.get_child("aie_partition");
  aie_partition aie_partitionHdr = { };

  // Initialized the start of the section heap
  SectionHeap heap(sizeof(aie_partition));

  // Name
  aie_partitionHdr.mpo_name = static_cast<decltype(aie_partitionHdr.mpo_name)>(heap.getNextBufferOffset());
  heap.write(sectionIndexName.c_str(), sectionIndexName.size() + 1 /*Null char*/);

  // TOPs
  aie_partitionHdr.operations_per_cycle = ptAIEPartition.get<uint32_t>("operations_per_cycle", 0);
  aie_partitionHdr.inference_fingerprint = ptAIEPartition.get<uint64_t>("inference_fingerprint", 0);
  aie_partitionHdr.pre_post_fingerprint = ptAIEPartition.get<uint64_t>("pre_post_fingerprint", 0);

  // kernel_commit_id (modeled after mpo_name)
  aie_partitionHdr.kernel_commit_id = static_cast<decltype(aie_partitionHdr.kernel_commit_id)>(heap.getNextBufferOffset());
  auto sKernelCommitId = ptAIEPartition.get<std::string>("kernel_commit_id", "");
  // XUtil::TRACE(boost::str(boost::format("  kernel_commit_id (0x%lx): '%s'") % aie_partitionHdr.kernel_commit_id % sKernelCommitId));
  heap.write(sKernelCommitId.c_str(), sKernelCommitId.size() + 1 /*Null char*/);

  //  Process the nodes
  process_partition_info(ptAIEPartition, aie_partitionHdr.info, heap);
  process_PDIs(ptAIEPartition, relativeFromDir, aie_partitionHdr, heap);

  // Write out the contents of the section
  osBuffer.write(reinterpret_cast<const char*>(&aie_partitionHdr), sizeof(aie_partitionHdr));
  heap.writeHeapToStream(osBuffer);
}

// -------------------------------------------------------------------------

void
SectionAIEPartition::readSubPayload(const char* pOrigDataSection,
                                    unsigned int /*origSectionSize*/,
                                    std::istream& iStream,
                                    const std::string& sSubSectionName,
                                    Section::FormatType eFormatType,
                                    std::ostringstream& buffer) const
{
  // Only default (e.g. empty) sub sections are supported
  if (!sSubSectionName.empty()) {
    auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s")
        % sSubSectionName % getSectionKindAsString();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Some basic DRC checks
  if (pOrigDataSection != nullptr) {
    std::string errMsg = "ERROR: AIE Partition section already exists.";
    throw std::runtime_error(errMsg);
  }

  if (eFormatType != Section::FormatType::json) {
    std::string errMsg = "ERROR: AIE Parition only supports the JSON format.";
    throw std::runtime_error(errMsg);
  }

  // Get the JSON's file parent directory.  This is used later to any
  // relative PDI images that might need need to be read in.
  fs::path p(getPathAndName());
  const auto relativeFromDir = p.parent_path();

  createAIEPartitionImage(getSectionIndexName(), relativeFromDir, iStream, buffer);
}

// -------------------------------------------------------------------------

static void
populate_partition_info(const char* pBase,
                        const aie_partition_info& aiePartitionInfo,
                        boost::property_tree::ptree& ptAiePartition)
{
  XUtil::TRACE("Populating Partition Info");
  boost::property_tree::ptree ptPartitionInfo;

  // Column Width
  ptPartitionInfo.put("column_width", (boost::format("%d") % aiePartitionInfo.column_width).str());

  // Start Columns
  boost::property_tree::ptree ptStartColumnArray;
  const uint16_t* columnArray = reinterpret_cast<const uint16_t*>(pBase + aiePartitionInfo.start_columns.offset);
  for (uint32_t index = 0; index < aiePartitionInfo.start_columns.size; index++) {
    boost::property_tree::ptree ptElement;
    ptElement.put("", (boost::format("%d") % columnArray[index]).str());
    ptStartColumnArray.push_back({ "", ptElement });
  }
  ptPartitionInfo.add_child("start_columns", ptStartColumnArray);

  ptAiePartition.add_child("partition", ptPartitionInfo);
}

// -------------------------------------------------------------------------
static void
populate_pre_cdo_groups(const char* pBase,
                        const cdo_group& aieCDOGroup,
                        boost::property_tree::ptree& ptCDOGroup)
{
  XUtil::TRACE("Populating PRE CDO groups");

  // If there is nothing to process, don't create the entry
  if (aieCDOGroup.pre_cdo_groups.size == 0)
    return;

  boost::property_tree::ptree ptPreCDOGroupArray;

  const uint64_t* aiePreCDOGroupArray = reinterpret_cast<const uint64_t*>(pBase + aieCDOGroup.pre_cdo_groups.offset);
  for (uint32_t index = 0; index < aieCDOGroup.pre_cdo_groups.size; index++) {
    const uint64_t& element = aiePreCDOGroupArray[index];
    boost::property_tree::ptree ptElement;

    ptElement.put("", (boost::format("0x%x") % element));

    ptPreCDOGroupArray.push_back({ "", ptElement });
  }

  ptCDOGroup.add_child("pre_cdo_groups", ptPreCDOGroupArray);
}

// -------------------------------------------------------------------------
static void
populate_cdo_groups(const char* pBase,
                    const aie_pdi& aiePDI,
                    boost::property_tree::ptree& ptAiePDI)
{
  XUtil::TRACE("Populating CDO groups");
  boost::property_tree::ptree ptCDOGroupArray;

  const cdo_group* aieCDOGroupArray = reinterpret_cast<const cdo_group*>(pBase + aiePDI.cdo_groups.offset);
  for (uint32_t index = 0; index < aiePDI.cdo_groups.size; index++) {
    const cdo_group& element = aieCDOGroupArray[index];
    boost::property_tree::ptree ptElement;

    // Name
    auto sName = reinterpret_cast<const char*>(pBase + element.mpo_name);
    ptElement.put("name", sName);
    XUtil::TRACE("Populating CDO group: " + std::string(sName));

    // Type
    if ((CDO_Type)element.cdo_type != CT_UNKNOWN)
      ptElement.put("type", getCDOTypeStr((CDO_Type)element.cdo_type));

    // PDI ID
    ptElement.put("pdi_id", (boost::format("0x%x") % element.pdi_id).str());

    // DPU Kernel IDs
    if (element.dpu_kernel_ids.size) {
      boost::property_tree::ptree ptDPUKernelIDs;
      const uint64_t* kernelIDsArray = reinterpret_cast<const uint64_t*>(pBase + element.dpu_kernel_ids.offset);
      for (uint32_t kernelIDindex = 0; kernelIDindex < element.dpu_kernel_ids.size; kernelIDindex++) {
        boost::property_tree::ptree ptID;
        ptID.put("", (boost::format("0x%x") % kernelIDsArray[kernelIDindex]).str());
        ptDPUKernelIDs.push_back({ "", ptID });
      }
      ptElement.add_child("dpu_kernel_ids", ptDPUKernelIDs);
    }

    // Pre cdo groups
    populate_pre_cdo_groups(pBase, element, ptElement);

    // Add the cdo group element to the array
    ptCDOGroupArray.push_back({ "", ptElement });
  }

  ptAiePDI.add_child("cdo_groups", ptCDOGroupArray);
}

// -------------------------------------------------------------------------

static void
write_pdi_image(const char* pBase,
                const aie_pdi& aiePDI,
                const std::string& fileName,
                const fs::path& relativeToDir)
{
  fs::path filePath = relativeToDir;
  filePath /= fileName;

  XUtil::TRACE(boost::format("Creating PDI Image: %s") % filePath.string());

  std::fstream oPDIFile;
  oPDIFile.open(filePath.string(), std::ifstream::out | std::ifstream::binary);
  if (!oPDIFile.is_open()) {
    const auto errMsg = boost::format("ERROR: Unable to open the file for writing: %s") % filePath.string();
    throw std::runtime_error(errMsg.str());
  }

  oPDIFile.write(reinterpret_cast<const char*>(pBase + aiePDI.pdi_image.offset), aiePDI.pdi_image.size);
}

// -------------------------------------------------------------------------
static void
populate_PDIs(const char* pBase,
              const fs::path& relativeToDir,
              const aie_partition& aiePartition,
              boost::property_tree::ptree& ptAiePartition)
{
  XUtil::TRACE("Populating DPI Array");
  boost::property_tree::ptree ptPDIArray;

  const aie_pdi* aiePdiArray = reinterpret_cast<const aie_pdi*>(pBase + aiePartition.aie_pdi.offset);
  for (uint32_t index = 0; index < aiePartition.aie_pdi.size; index++) {
    const aie_pdi& element = aiePdiArray[index];
    boost::property_tree::ptree ptElement;

    // UUID
    ptElement.put("uuid", XUtil::getUUIDAsString(element.uuid));

    // Partition Image
    std::string fileName = XUtil::getUUIDAsString(element.uuid) + ".pdi";
    write_pdi_image(pBase, element, fileName, relativeToDir);
    ptElement.put("file_name", fileName);

    // CDO Groups
    populate_cdo_groups(pBase, element, ptElement);

    // Add the PDI element to the array
    ptPDIArray.push_back({ "", ptElement });
  }

  ptAiePartition.add_child("PDIs", ptPDIArray);
}

// -------------------------------------------------------------------------

static void
writeAIEPartitionImage(const char* pBuffer,
                       unsigned int bufferSize,
                       const fs::path& relativeToDir,
                       std::ostream& oStream)
{
  XUtil::TRACE("AIE_PARTITION : Creating JSON IMAGE");

  // Do we have enough room to overlay the header structure
  if (bufferSize < sizeof(aie_partition)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the aie_partition structure (%d)") % bufferSize % sizeof(aie_partition);
    throw std::runtime_error(boost::str(errMsg));
  }

  auto pHdr = reinterpret_cast<const aie_partition*>(pBuffer);

  // Name
  boost::property_tree::ptree ptAiePartition;
  ptAiePartition.put("name", pBuffer + pHdr->mpo_name);

  // TOPs
  ptAiePartition.put("operations_per_cycle", (boost::format("%d") % pHdr->operations_per_cycle).str());
  ptAiePartition.put("inference_fingerprint", (boost::format("%d") % pHdr->inference_fingerprint).str());
  ptAiePartition.put("pre_post_fingerprint", (boost::format("%d") % pHdr->pre_post_fingerprint).str());

  // kernel_commit_id (modeled after mpo_name)
  // in order to be backward compatible with old xclbin which doesn't have
  // kernel_commit_id, we should make sure the offset is NOT 0
  auto sKernelCommitId = "";
  if (pHdr->kernel_commit_id != 0) {
    sKernelCommitId = reinterpret_cast<const char*>(pBuffer + pHdr->kernel_commit_id);
  } else {
    XUtil::TRACE(boost::format("Open an existing xclbin: kernel_commit_id is 0x%lx") % pHdr->kernel_commit_id);
  }
  ptAiePartition.put("kernel_commit_id", sKernelCommitId);

  // Partition info
  populate_partition_info(pBuffer, pHdr->info, ptAiePartition);

  // PDIs
  populate_PDIs(pBuffer, relativeToDir, *pHdr, ptAiePartition);

  // Write out the built property tree
  boost::property_tree::ptree ptRoot;
  ptRoot.put_child("aie_partition", ptAiePartition);

  XUtil::TRACE_PrintTree("root", ptRoot);
  boost::property_tree::write_json(oStream, ptRoot);
}

// -------------------------------------------------------------------------

void
SectionAIEPartition::writeSubPayload(const std::string& sSubSectionName,
                                     FormatType eFormatType,
                                     std::fstream& oStream) const
{
  // Some basic DRC checks
  if (m_pBuffer == nullptr) {
    std::string errMsg = "ERROR: Vendor Metadata section does not exist.";
    throw std::runtime_error(errMsg);
  }

  if (!sSubSectionName.empty()) {
    auto errMsg = boost::format("ERROR: Subsection '%s' not support by section '%s")
                                 % sSubSectionName % getSectionKindAsString();
    throw std::runtime_error(boost::str(errMsg));
  }

  // Some basic DRC checks
  if (eFormatType != Section::FormatType::json) {
    std::string errMsg = "ERROR: AIE Partition section only supports the JSON format.";
    throw std::runtime_error(errMsg);
  }

  fs::path p(getPathAndName());
  const auto relativeToDir = p.parent_path();

  writeAIEPartitionImage(m_pBuffer, m_bufferSize, relativeToDir, oStream);
}

// -------------------------------------------------------------------------

void
SectionAIEPartition::readXclBinBinary(std::istream& iStream,
                                      const axlf_section_header& sectionHeader)
{
  Section::readXclBinBinary(iStream, sectionHeader);

  XUtil::TRACE("Determining AIE_PARTITION Section Name");

  // Do we have enough room to overlay the header structure
  if (m_bufferSize < sizeof(aie_partition)) {
    auto errMsg = boost::format("ERROR: Segment size (%d) is smaller than the size of the aie_partition structure (%d)")
                                % m_bufferSize % sizeof(aie_partition);
    throw std::runtime_error(boost::str(errMsg));
  }

  auto pHdr = reinterpret_cast<const aie_partition*>(m_pBuffer);

  // Name
  std::string name = m_pBuffer + pHdr->mpo_name;
  XUtil::TRACE(std::string("Successfully read in the AIE_PARTITION section: '") + name + "' ");
  Section::m_sIndexName = name;
}

// -------------------------------------------------------------------------

