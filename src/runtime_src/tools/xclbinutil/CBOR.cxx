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

#include "CBOR.h"

#include <stdexcept>
#include <vector>
#include <boost/format.hpp>


#include "XclBinUtilities.h"
namespace XUtil = XclBinUtilities;

std::string
XclBinUtilities::enum_to_string(MajorTypes const majorType) 
{
  switch (majorType) {
    case MajorTypes::positive_integer:
      return "Positive Integer";
    case MajorTypes::negative_integer:
      return "Negative Integer";
    case MajorTypes::byte_string:
      return "Byte String";
    case MajorTypes::text_string:
      return "Text String";
    case MajorTypes::array_of_items:
      return "Array of Items";
    case MajorTypes::map_of_items:
      return "Map of Items";
    case MajorTypes::semantic_tag:
      return "Semantic Tag";
    case MajorTypes::primitives:
      return "Primitives";
  }

  // Not needed, but some compilers don't know that :^)
  return "";
}


std::string
XclBinUtilities::encode_major_type(const MajorTypes majorType,
                                   const uint64_t count) 
{
  XUtil::TRACE((boost::format("CBOR: [Encode] %s(%d), Count: %d") % enum_to_string(majorType) % static_cast<unsigned int>(majorType) % count).str());

  // This method doesn't support Primitive types
  if (majorType == MajorTypes::primitives)
    throw std::runtime_error("Error: CBOR Major Type Primitive (0b111) is not supported by the encode_major_type() method.");

  // Our working array
  std::vector<uint8_t> byte_array;
  const unsigned int MAX_TINY_SIZE = 23;

  // -- Encode the major type to the first byte
  //   Bits 8, 7, 6 represent Major Type
  byte_array.push_back(static_cast<uint8_t>(majorType) << 5);

  // -- Encode the size of items
  if (count <= MAX_TINY_SIZE) {                              // -- Encode Tiny (bits 5 - 1)
    byte_array[0] |= static_cast<uint8_t>(count & 0x1F);
  } else {                                                  // -- Byte encoding
    uint8_t num_bytes = 3;         // Assume 64 bits

    if (count <= 0xffffffff)       // Less than 32 bits
      num_bytes = 2;

    if (count <= 0xffff)           // Less than 16 bits
      num_bytes = 1;

    if (count <= 0xff)             // Less than 8 bits
      num_bytes = 0;

    // Encode extended payload flags (bits 5 & 4)
    byte_array[0] |= 0x18;

    // Encode extended payload size
    byte_array[0] |= num_bytes;

    // Encode the count size (big endian)
    for (uint8_t shiftCount = 1 << num_bytes; shiftCount != 0; shiftCount--)
      byte_array.push_back(static_cast<uint8_t>((count >> ((shiftCount - 1) * 8)) & 0xff));
  }

  std::string encodedBuf((const char*)&byte_array[0], byte_array.size());

  return encodedBuf;
}

std::string
XclBinUtilities::encode_positive_integer(const uint64_t intValue) 
{
  return encode_major_type(MajorTypes::positive_integer, intValue);
}

std::string
XclBinUtilities::encode_negative_integer(const uint64_t intValue) 
{
  return encode_major_type(MajorTypes::negative_integer, intValue);
}

std::string
XclBinUtilities::encode_text_string(const std::string& text_string) 
{
  std::string encodeBuf = encode_major_type(MajorTypes::text_string, text_string.length());
  encodeBuf += text_string;
  XUtil::TRACE(std::string("CBOR: [Encode] Text String: '") + text_string + "'");

  return encodeBuf;
}

std::string
XclBinUtilities::encode_byte_string(const std::string& byte_string) 
{
  std::string encodeBuf = encode_major_type(MajorTypes::byte_string, byte_string.length());
  encodeBuf += byte_string;

  return encodeBuf;
}


static
void
read_buffer(std::istream& istr, uint8_t* outBuffer, uint64_t size) 
{
  istr.read(reinterpret_cast<char*>(outBuffer), size);

  if (istr.eof())
    throw std::runtime_error("Error: Unexpected end of the CBOR image buffer.");

  if (istr.fail())
    throw std::runtime_error("Error: Unknown error occurred while reading in the CBOR image buffer.");
}


static
uint8_t
get_char(std::istream& istr) 
{
  uint8_t nextChar = 0;
  read_buffer(istr, &nextChar, sizeof(uint8_t));

  return nextChar;
}

std::string
XclBinUtilities::get_string(std::istream& istr, uint64_t size) 
{
  // Reserve and initialize the memory to copy the string into
  std::vector<uint8_t> vecBuffer(size, 0);
  read_buffer(istr, vecBuffer.data(), vecBuffer.size());

  return std::string(static_cast<char*>((void*)vecBuffer.data()), vecBuffer.size());
}

void
XclBinUtilities::get_next_type_and_count(std::istream& istr,
                                         MajorTypes& majorType, uint64_t& count) 
{
  // Make sure we have data that can be examined
  uint8_t commandByte = get_char(istr);

  // -- Get the command
  unsigned int majorTypeValue = commandByte >> 5;
  majorType = static_cast<MajorTypes>(majorTypeValue);

  // -- Determine the count value
  // Are there extended payload bytes (Bits 5 & 4)
  if ((commandByte & 0x18) == 0x18) {
    // Payload size is in Bits 3, 2, 1.
    // Actual size is off by 1.
    unsigned int payloadBytes = 1 << (commandByte & 0x7);

    if (payloadBytes > sizeof(uint64_t))
      throw std::runtime_error((boost::format("Error: Unsupported payload value: 0x%x") % (payloadBytes - 1)).str());

    // Decode the count size (big endian)
    count = 0;

    // Create the integer value
    do {
      uint64_t aByte = get_char(istr);
      count = (count << 8) + aByte;
    } while (--payloadBytes);


  } else {
    count = commandByte & 0x1F;   // Tiny Encoded.  Bits 5 - 1.
  }


  XUtil::TRACE((boost::format("CBOR: [Decode] %s(%d), Count: %d") % enum_to_string(majorType) % majorTypeValue % count).str());
}

