#include "khronos.h"

/*
   Copyright (c) 2011 The Khronos Group Inc.
   Permission is hereby granted, free of charge, to any person obtaining a copy of this
   software and /or associated documentation files (the "Materials "), to deal in the Materials
   without restriction, including without limitation the rights to use, copy,modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Materials, and to permit persons to
   whom the Materials are furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Materials.
   THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE MATERIALS OR THE USE OR OTHER DEALINGS IN
   THE MATERIALS.
 */

namespace khronos {

bool
check_copy_overlap(const size_t src_offset[3],
    const size_t dst_offset[3],
    const size_t region[3],
    size_t row_pitch, size_t slice_pitch)
{
  const size_t src_min[] = {src_offset[0], src_offset[1], src_offset[2]};
  const size_t src_max[] = {src_offset[0] + region[0], src_offset[1] + region[1], src_offset[2] + region[2]};
  const size_t dst_min[] = {dst_offset[0], dst_offset[1], dst_offset[2]};
  const size_t dst_max[] = {dst_offset[0] + region[0], dst_offset[1] + region[1], dst_offset[2] + region[2]};
  // Check for overlap
  bool overlap = true;
  unsigned i;
  for (i=0; i != 3; ++i)
  {
    overlap = overlap && (src_min[i] < dst_max[i])
      && (src_max[i] > dst_min[i]);
  }
  size_t dst_start = dst_offset[2] * slice_pitch + dst_offset[1] * row_pitch + dst_offset[0];
  size_t dst_end = dst_start + (region[2] * slice_pitch + region[1] * row_pitch + region[0]);
  size_t src_start = src_offset[2] * slice_pitch + src_offset[1] * row_pitch + src_offset[0];
  size_t src_end = src_start + (region[2] * slice_pitch + region[1] * row_pitch + region[0]);
  if (!overlap)
  {
    size_t delta_src_x = (src_offset[0] + region[0] > row_pitch) ?
      src_offset[0] + region[0] - row_pitch : 0;
    size_t delta_dst_x = (dst_offset[0] + region[0] > row_pitch) ?
      dst_offset[0] + region[0] - row_pitch : 0;
    if ( (delta_src_x > 0 && delta_src_x > dst_offset[0]) ||
        (delta_dst_x > 0 && delta_dst_x > src_offset[0]) )
    {
      if ( (src_start <= dst_start && dst_start < src_end) ||
          (dst_start <= src_start && src_start < dst_end) )
        overlap = true;
    }
    if (region[2]>1 && row_pitch)
    {
      size_t src_height = slice_pitch / row_pitch;
      size_t dst_height = slice_pitch / row_pitch;
      size_t delta_src_y = (src_offset[1] + region[1] > src_height) ?
        src_offset[1] + region[1] - src_height : 0;
      size_t delta_dst_y = (dst_offset[1] + region[1] > dst_height) ?
        dst_offset[1] + region[1] - dst_height : 0;
      if ( (delta_src_y > 0 && delta_src_y > dst_offset[1]) ||
          (delta_dst_y > 0 && delta_dst_y > src_offset[1]) )
      {
        if ( (src_start <= dst_start && dst_start < src_end) ||
            (dst_start <= src_start && src_start < dst_end) )
          overlap = true;
      }
    }
  }

  return overlap;
}

} // namespace
