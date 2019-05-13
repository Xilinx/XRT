/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <CL/opencl.h>
#include "xocl/config.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/memory.h"
#include "detail/memory.h"
#include "detail/context.h"

#include <cstdlib>
#include "plugin/xdp/profile.h"

namespace {

// Hack to determine if a context is associated with exactly one
// device.  Additionally, in emulation mode, the device must be
// active, e.g. loaded through a call to loadBinary.
//
// This works around a problem where clCreateBuffer is called in
// emulation mode before clCreateProgramWithBinary->loadBinary has
// been called.  The call to loadBinary can end up switching the
// device from swEm to hwEm.
//
// In non emulation mode it is sufficient to check that the context
// has only one device.
static xocl::device*
singleContextDevice(cl_context context)
{
  auto device = xocl::xocl(context)->get_device_if_one();
  if (!device)
    return nullptr;

  static bool emulation = std::getenv("XCL_EMULATION_MODE");
  return (!emulation || device->is_active())
    ? device
    : nullptr;
}

} // namespace

namespace xocl {

static void
validImageFormatOrError(const cl_image_format* image_format)
{
  // CL_INVALID_IMAGE_FORMAT_DESCRIPTOR if values specified in
  // image_format are not valid or if image_format is NULL.
  if (!image_format)
    throw error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR,"image_format is nullptr");

  auto type = image_format->image_channel_data_type;
  auto order = image_format->image_channel_order;

  switch (order) {
  // CL_INTENSITY This format can only be used if channel data type =
  // CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16,
  // CL_HALF_FLOAT, or CL_FLOAT.

  // CL_LUMINANCE This format can only be used if channel data type =
  // CL_UNORM_INT8, CL_UNORM_INT16, CL_SNORM_INT8, CL_SNORM_INT16,
  // CL_HALF_FLOAT, or CL_FLOAT.
  case CL_INTENSITY:
  case CL_LUMINANCE:
    if (type != CL_UNORM_INT8 && type != CL_UNORM_INT16 &&
        type != CL_SNORM_INT8 && type != CL_SNORM_INT16 &&
        type != CL_HALF_FLOAT && type != CL_FLOAT)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_INTENSITY or CL_LUMINANCE");
    break;

#if 0
  // CL_DEPTH This format can only be used if channel data type =
  // CL_UNORM_INT16 or CL_FLOAT.
  case CL_DEPTH:
    if (type != CL_UNORM_INT16 && type != CL_FLOAT)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_DEPTH");
    break;
#endif

  // CL_RGB or CL_RGBx These formats can only be used if channel
  // data type = CL_UNORM_SHORT_565, CL_UNORM_SHORT_555 or
  // CL_UNORM_INT101010.
  case CL_RGB:
  case CL_RGBx:
    if (type != CL_UNORM_SHORT_555 && type != CL_UNORM_SHORT_565 &&
        type != CL_UNORM_INT_101010)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_RGB or CL_RGBx");
    break;

#if 0
  // CL_sRGB, CL_sRGBx, CL_sRGBA, CL_sBGRA These formats can only be
  // used if channel data type = CL_UNORM_INT8.
  case CL_sRGB:
  case CL_sRGBx:
  case CL_sRGBA:
  case CL_sBGRA:
    if (type != CL_UNORM_INT8)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_sRGB, CL_sRGBx, CL_sRGBA, or CL_sBGRA");
    break;
#endif

  // CL_ARGB, CL_BGRA, CL_ABGR These formats can only be used if
  // channel data type = CL_UNORM_INT8, CL_SNORM_INT8,
  // CL_SIGNED_INT8 or CL_UNSIGNED_INT8.
  case CL_ARGB:
  case CL_BGRA:
    //  case CL_ABGR:
    if (type != CL_UNORM_INT8 && type != CL_SIGNED_INT8 &&
        type != CL_SNORM_INT8 && type != CL_UNSIGNED_INT8)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_ARGB, CL_BGRA, or CL_ABGR");
    break;

  // CL_DEPTH_STENCIL This format can only be used if channel data
  // type = CL_UNORM_INT24 or CL_FLOAT (applies if the
  // cl_khr_gl_depth_images extension is enabled).
#if 0  // (applcies if the cl_khr_gl_depth_images extension is enabled).
  case CL_DEPTH_STENCIL:
    if (type != CL_UNORM_INT24 && type != CL_FLOAT)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_DEPTH_STENCIL");
    break;
#endif
  }

  switch (type) {
    // CL_UNORM_SHORT_565 Represents a normalized 5-6-5 3-channel RGB
    // image. The channel order must be CL_RGB or CL_RGBx.

    // CL_UNORM_SHORT_555 Represents a normalized x-5-5-5 4-channel
    // xRGB image. The channel order must be CL_RGB or CL_RGBx.

    // CL_UNORM_INT_101010 Represents a normalized x-10-10-10
    // 4-channel xRGB image. The channel order must be CL_RGB or
    // CL_RGBx.
  case CL_UNORM_SHORT_565:
  case CL_UNORM_SHORT_555:
  case CL_UNORM_INT_101010:
    if (order != CL_RGB && order != CL_RGBx)
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, or CL_UNORM_INT_101010");
    break;
  }
}

static void
validImageDescriptorOrError(const cl_image_desc*   image_desc,
                            void*                  host_ptr)
{
  auto type = image_desc->image_type;
  auto width = image_desc->image_width;
  auto height = image_desc->image_height;
  auto depth = image_desc->image_depth;
  auto array_size = image_desc->image_array_size;
  auto row_pitch = image_desc->image_row_pitch;
  auto slice_pitch = image_desc->image_slice_pitch;
  auto num_mip_levels = image_desc->num_mip_levels;
  auto num_samples = image_desc->num_samples;
  auto mem_object = image_desc->buffer;

  //  image_type Describes the image type and must be either
  //  CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_BUFFER,
  //  CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D,
  //  CL_MEM_OBJECT_IMAGE2D_ARRAY, or CL_MEM_OBJECT_IMAGE3D.
  if (type != CL_MEM_OBJECT_IMAGE1D && type != CL_MEM_OBJECT_IMAGE1D_BUFFER &&
      type != CL_MEM_OBJECT_IMAGE1D_ARRAY && type != CL_MEM_OBJECT_IMAGE2D &&
      type != CL_MEM_OBJECT_IMAGE2D_ARRAY && type != CL_MEM_OBJECT_IMAGE3D)
    throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->type");

  // image_width The width of the image in pixels.
  // For a 2D image and image array, the image width must be a value
  // >= 1 and ≤ CL_DEVICE_IMAGE2D_MAX_WIDTH.
  if (type == CL_MEM_OBJECT_IMAGE2D || type == CL_MEM_OBJECT_IMAGE2D_ARRAY)
    if (width < 1 /* || width > CL_DEVICE_IMAGE2D_MAX_WIDTH */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_width");

  // For a 3D image, the image width must be a value ≥ 1 and ≤
  // CL_DEVICE_IMAGE3D_MAX_WIDTH.
  if (type == CL_MEM_OBJECT_IMAGE3D)
    if (width < 1 /* || width > CL_DEVICE_IMAGE3D_MAX_WIDTH */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_width");

  // For a 1D image buffer, the image width must be a value ≥ 1 and ≤
  // CL_DEVICE_IMAGE_MAX_BUFFER_SIZE.
  if (type == CL_MEM_OBJECT_IMAGE1D_BUFFER)
    if (width < 1 /* || width > CL_DEVICE_IMAGE_MAX_BUFFER_SIZE */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_width");

  // For a 1D image and 1D image array, the image width must be a
  // value ≥ 1 and ≤ CL_DEVICE_IMAGE2D_MAX_WIDTH.
  if (type == CL_MEM_OBJECT_IMAGE1D || type == CL_MEM_OBJECT_IMAGE1D_ARRAY)
    if (width < 1 /* || width > CL_DEVICE_IMAGE2D_MAX_WIDTH */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_width");

  // image_height The height of the image in pixels. This is only used
  // if the image is a 2D or 3D image, or a 2D image array.
  // For a 2D image or image array, the image height must be a value ≥
  // 1 and ≤ CL_DEVICE_IMAGE2D_MAX_HEIGHT.
  if (type == CL_MEM_OBJECT_IMAGE2D || type == CL_MEM_OBJECT_IMAGE2D_ARRAY)
    if (height < 1 /* || width > CL_DEVICE_IMAGE2D_MAX_HEIGHT */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_height");

  // For a 3D image, the image height must be a value ≥ 1 and ≤
  // CL_DEVICE_IMAGE3D_MAX_HEIGHT.
  if (type == CL_MEM_OBJECT_IMAGE3D)
    if (height < 1 /* || width > CL_DEVICE_IMAGE3D_MAX_HEIGHT */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_height");

  // image_depth The depth of the image in pixels. This is only used
  // if the image is a 3D image and must be a value ≥ 1 and ≤
  // CL_DEVICE_IMAGE3D_MAX_DEPTH
  if (type == CL_MEM_OBJECT_IMAGE3D)
    if (depth < 1 /* || width > CL_DEVICE_IMAGE3D_MAX_DEPTH */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_depth");

  // image_array_size The number of images in the image array. This is
  // only used if the image is a 1D or 2D image array. The values for
  // image_array_size, if specified, must be a value ≥ 1 and ≤
  // CL_DEVICE_IMAGE_MAX_ARRAY_SIZE.
  if (type == CL_MEM_OBJECT_IMAGE1D_ARRAY || type == CL_MEM_OBJECT_IMAGE2D_ARRAY)
    if (array_size < 1 /* || array_size > CL_DEVICE_IMAGE_MAX_ARRAY_SIZE */)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_array_size");

  // image_row_pitch The scan-line pitch in bytes. This must be 0 if
  // host_ptr is NULL and can be either 0 or ≥ image_width * size of
  // element in bytes if host_ptr is not NULL. If host_ptr is not NULL
  // and image_row_pitch = 0, image_row_pitch is calculated as
  // image_width * size of element in bytes. If image_row_pitch is not
  // 0, it must be a multiple of the image element size in bytes. For
  // a 2D image created from a buffer, the pitch specified (or
  // computed if pitch specified is 0) must be a multiple of the
  // maximum of the CL_DEVICE_IMAGE_PITCH_ALIGNMENT value for all
  // devices in the context associated with image_desc->mem_object and
  // that support images.
  if (!host_ptr && row_pitch)
    throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_row_pitch");
  /* ??? */

  // image_slice_pitch The size in bytes of each 2D slice in the 3D
  // image or the size in bytes of each image in a 1D or 2D image
  // array. This must be 0 if host_ptr is NULL. If host_ptr is not
  // NULL, image_slice_pitch can be either 0 or ≥ image_row_pitch *
  // image_height for a 2D image array or 3D image and can be either 0
  // or ≥ image_row_pitch for a 1D image array. If host_ptr is not
  // NULL and image_slice_pitch = 0, image_slice_pitch is calculated
  // as image_row_pitch * image_height for a 2D image array or 3D
  // image and image_row_pitch for a 1D image array. If
  // image_slice_pitch is not 0, it must be a multiple of the
  // image_row_pitch.
  if (!host_ptr && slice_pitch)
    throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->image_slice_pitch");
  /* ??? */


  // num_mip_level, num_samples Must be 0.
  if (num_mip_levels || num_samples)
    throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR,"bad image_desc->num_mip_levels or num_samples");

  // mem_object Refers to a valid buffer or image memory
  // object. mem_object can be a buffer memory object if image_type is
  // CL_MEM_OBJECT_IMAGE1D_BUFFER or CL_MEM_OBJECT_IMAGE2D (To create
  // a 2D image from a buffer object that share the data store between
  // the image and buffer object) . mem_object can be a image object
  // if image_type is CL_MEM_OBJECT_IMAGE2D (To create an image object
  // from another image object that share the data store between these
  // image objects). Otherwise it must be NULL. The image pixels are
  // taken from the memory object’s data store. When the contents of
  // the specified memory object’s data store are modified, those
  // changes are reflected in the contents of the image object and
  // vice-versa at corresponding sychronization points. For a 1D image
  // buffer object, the image_width * size of element in bytes must be
  // ≤ size of buffer object data store. For a 2D image created from a
  // buffer, the image_row_pitch * image_height must be ≤ size of
  // buffer object data store. For an image object created from
  // another image object, the values specified in the image
  // descriptor except for mem_object must match the image descriptor
  // information associated with mem_object.
  // mem_object can be null
  if(mem_object)
    detail::memory::validOrError(mem_object);
  /* ??? */
}

static void
validOrError(cl_context             context,
             cl_mem_flags           flags,
             const cl_image_format* image_format,
             const cl_image_desc*   image_desc,
             void*                  host_ptr,
             cl_int*                errcode_ret)

{
  if( !xocl::config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context.
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if values specified in flags are not valid.
  detail::memory::validOrError(flags);

  // CL_INVALID_IMAGE_FORMAT_DESCRIPTOR if values specified in
  // image_format are not valid or if image_format is NULL.
  validImageFormatOrError(image_format);

  // CL_INVALID_IMAGE_FORMAT_DESCRIPTOR if a 2D image is created from
  // a buffer and the row pitch and base address alignment does not
  // follow the rules described for creating a 2D image from a buffer.
  /* ??? */

  // CL_INVALID_IMAGE_FORMAT_DESCRIPTOR if a 2D image is created from
  // a 2D image object and the rules described above are not followed.
  /* ??? */

  // CL_INVALID_IMAGE_DESCRIPTOR if values specified in image_desc are
  // not valid or if image_desc is NULL.
  validImageDescriptorOrError(image_desc,host_ptr);

  // CL_INVALID_IMAGE_SIZE if image dimensions specified in image_desc
  // exceed the maximum image dimensions described in the table of
  // allowed values for param_name for clGetDeviceInfo for all devices
  // in context.
  /* ??? */

  // CL_INVALID_HOST_PTR if host_ptr is NULL and CL_MEM_USE_HOST_PTR
  // or CL_MEM_COPY_HOST_PTR are set in flags or if host_ptr is not
  // NULL but CL_MEM_COPY_HOST_PTR or CL_MEM_USE_HOST_PTR are not set
  // in flags.
  detail::memory::validHostPtrOrError(flags,host_ptr);

  // CL_INVALID_VALUE if an image buffer is being created and the
  // buffer object was created with CL_MEM_WRITE_ONLY and flags
  // specifies CL_MEM_READ_WRITE or CL_MEM_READ_ONLY, or if the buffer
  // object was created with CL_MEM_READ_ONLY and flags specifies
  // CL_MEM_READ_WRITE or CL_MEM_WRITE_ONLY, or if flags specifies
  // CL_MEM_USE_HOST_PTR or CL_MEM_ALLOC_HOST_PTR or
  // CL_MEM_COPY_HOST_PTR.

  // CL_INVALID_VALUE if an image buffer is being created or an image
  // is being created from another memory object (image or buffer) and
  // the mem_object object was created with CL_MEM_HOST_WRITE_ONLY and
  // flags specifies CL_MEM_HOST_READ_ONLY, or if mem_object was
  // created with CL_MEM_HOST_READ_ONLY and flags specifies
  // CL_MEM_HOST_WRITE_ONLY, or if mem_object was created with
  // CL_MEM_HOST_NO_ACCESS and flags specifies CL_MEM_HOST_READ_ONLY
  // or CL_MEM_HOST_WRITE_ONLY.
  /* ??? */

  // CL_IMAGE_FORMAT_NOT_SUPPORTED if the image_format is not
  // supported.
  /* ??? */

  // CL_INVALID_OPERATION if there are no devices in context that
  // support images (i.e. CL_DEVICE_IMAGE_SUPPORT (specified in the
  // table of OpenCL Device Queries for clGetDeviceInfo) is CL_FALSE).
  /* ??? */
}

static unsigned
getBytesPerPixel(const cl_image_format *format)
{
  unsigned bpp = 0;
  const uint32_t type = format->image_channel_data_type;
  const uint32_t order = format->image_channel_order;

  switch(type) {
  case CL_SNORM_INT8:
    bpp = 1;
    break;
  case CL_SNORM_INT16:
    bpp = 2;
    break;
  case CL_UNORM_INT8:
    bpp = 1;
    break;
  case CL_UNORM_INT16:
    bpp = 2;
    break;
    //Next 3 cases are dealt as one case.
  case CL_UNORM_SHORT_565:
  case CL_UNORM_SHORT_555:
    bpp = 2;
    break;
  case CL_UNORM_INT_101010:
    bpp = 4;
    break;
  case CL_SIGNED_INT8:
    bpp = 1;
    break;
  case CL_SIGNED_INT16:
    bpp = 2;
    break;
  case CL_SIGNED_INT32:
    bpp = 4;
    break;
  case CL_UNSIGNED_INT8:
    bpp = 1;
    break;
  case CL_UNSIGNED_INT16:
    bpp = 2;
    break;
  case CL_UNSIGNED_INT32:
    bpp = 4;
    break;
  case CL_HALF_FLOAT:
    bpp = 2;
    break;
  case CL_FLOAT:
    bpp = 4;
    break;
  default:
    throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "clCreateImage");
  }

  switch (order) {
    case CL_R:
    case CL_Rx:
    case CL_A:
      break;
    case CL_INTENSITY:
    case CL_LUMINANCE:
      break;
    case CL_RA:
    case CL_RGx:
    case CL_RG:
      bpp *= 2;
      break;
    case CL_RGB:
    case CL_RGBx:
      break;
    case CL_RGBA:
      bpp *= 4;
      break;
    case CL_ARGB:
    case CL_BGRA:
      bpp *= 4;
      break;
    default:
      throw xocl::error(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, "clCreateImage");
  }

  return bpp;
}

static cl_mem
mkImageCore (cl_context context,
	cl_mem_flags flags,
	const cl_image_format * format,
	const cl_image_desc * desc,
	const cl_mem_object_type image_type,
	size_t w,
	size_t h,
	size_t depth,
	size_t row_pitch,
	size_t slice_pitch,
	void * user_ptr,
	cl_mem buffer,        //image2D created from buffer
	cl_int *     errcode_ret)
{
    if (xocl::config::api_checks()) {
	if(w==0)
	    throw xocl::error(CL_INVALID_IMAGE_SIZE, "clCreateImage");

	if(h==0) {
	    if((image_type != CL_MEM_OBJECT_IMAGE1D &&
			image_type != CL_MEM_OBJECT_IMAGE1D_ARRAY &&
			image_type != CL_MEM_OBJECT_IMAGE1D_BUFFER))
		throw xocl::error(CL_INVALID_IMAGE_SIZE, "clCreateImage");
	}
    }

    unsigned bpp = getBytesPerPixel(format);
    size_t sz = 0 /* this is the size of the buffer we need.*/, adjusted_h = 0;
    size_t adjusted_row_pitch = 0, adjusted_slice_pitch = 0;

    if(image_type == CL_MEM_OBJECT_IMAGE1D) {
	h = 1;
	depth = 1;
	if (user_ptr && row_pitch == 0)
	    row_pitch = bpp*w;
    }
    if(image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
	throw xocl::error( CL_IMAGE_FORMAT_NOT_SUPPORTED, "clCreateImage");
    }
    if(image_type == CL_MEM_OBJECT_IMAGE2D) {
	if(buffer)
	    throw xocl::error(CL_IMAGE_FORMAT_NOT_SUPPORTED, "clCreateImage");
	depth = 1;
	if (user_ptr && row_pitch == 0)
	    row_pitch = bpp*w;
    }
    if ( (image_type == CL_MEM_OBJECT_IMAGE3D) ||
	    (image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) ||
	    (image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY))
    {
	if(image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
	    h = 1;
	}
	if (user_ptr && row_pitch == 0)
	    row_pitch = bpp*w;
	if (user_ptr && slice_pitch == 0)
	    slice_pitch = row_pitch*h;
    }

    adjusted_row_pitch = w*bpp;
    if(adjusted_row_pitch < row_pitch && (user_ptr) != nullptr)
	adjusted_row_pitch = row_pitch;

    //Till we have native h/w support.
    if(adjusted_h==0)
	adjusted_h = h;

    //Initialize the size.
    sz = adjusted_row_pitch * adjusted_h * depth;

    if(user_ptr)
	throw xocl::error(CL_IMAGE_FORMAT_NOT_SUPPORTED, "clCreateImage: Image1D buffer");

    //cxt, flags, sz, w, h, depth , row, slice, "image_type", *format, xlnx_fmt, bpp
    auto ubuffer = std::make_unique<xocl::image>(xocl::xocl(context),flags,sz,w,h,depth,
	    adjusted_row_pitch,adjusted_slice_pitch,bpp,image_type,*format,user_ptr);

    cl_mem image = ubuffer.get();

    if (image_type == CL_MEM_OBJECT_IMAGE1D || image_type == CL_MEM_OBJECT_IMAGE2D
	    || image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER)
	adjusted_slice_pitch = 0;

    /* Copy the data if required */
    if (flags & CL_MEM_COPY_HOST_PTR && user_ptr) {
	//TODO: copy image.
	throw xocl::error(CL_IMAGE_FORMAT_NOT_SUPPORTED, "clCreateImage, copy host ptr");
    }

    if (flags & CL_MEM_USE_HOST_PTR && user_ptr) {
	throw xocl::error(CL_IMAGE_FORMAT_NOT_SUPPORTED, "clCreateImage, use host ptr");
    }

    //set fields in cl_buffer
    //
    unsigned memExtension = 0;
    xocl::xocl(image)->set_ext_flags(memExtension);

    // allocate device buffer object if context has only one device
    // and if this is not a progvar (clCreateProgramWithBinary)
    if (!(flags & CL_MEM_PROGVAR)) {
	if (auto device = singleContextDevice(context)) {
	    xocl::xocl(image)->get_buffer_object(device);
	}
    }

    xocl::assign(errcode_ret,CL_SUCCESS);
    return ubuffer.release();
}

static cl_mem
mkImageFromBuffer(cl_context             context,
                  cl_mem_flags           flags,
                  const cl_image_format* format,
                  const cl_image_desc*   desc,
                  cl_int*                errcode_ret)
{
  //This will call mkImageCore() function after modifying the arguments of desc.
  throw xocl::error(CL_IMAGE_FORMAT_NOT_SUPPORTED, "clCreateImage, buffer type");
  return nullptr;
}

static cl_mem
mkImage(cl_context             context,
	cl_mem_flags           flags,
	const cl_image_format* format,
	const cl_image_desc*   desc,
	void*                  host_ptr,
	cl_int*                errcode_ret)
{
  switch (desc->image_type) {
  case CL_MEM_OBJECT_IMAGE1D:
  case CL_MEM_OBJECT_IMAGE3D:
    return mkImageCore(context,flags,format,desc,desc->image_type,
                       desc->image_width,desc->image_height,desc->image_depth,
                       desc->image_row_pitch, desc->image_slice_pitch,
                       host_ptr,nullptr,errcode_ret);
  case CL_MEM_OBJECT_IMAGE2D:
    if(desc->buffer)
      return mkImageFromBuffer(context,flags,format,desc,errcode_ret);
    else
      return mkImageCore(context,flags,format,desc,desc->image_type,
                         desc->image_width,desc->image_height,desc->image_depth,
                         desc->image_row_pitch, desc->image_slice_pitch,
                         host_ptr,nullptr,errcode_ret);
  case CL_MEM_OBJECT_IMAGE1D_ARRAY:
  case CL_MEM_OBJECT_IMAGE2D_ARRAY:
    return mkImageCore(context,flags,format,desc,desc->image_type,
                       desc->image_width,desc->image_height,desc->image_depth,
                       desc->image_row_pitch, desc->image_slice_pitch,
                       host_ptr,nullptr,errcode_ret);
  case CL_MEM_OBJECT_IMAGE1D_BUFFER:
    return mkImageFromBuffer(context,flags,format,desc,errcode_ret);
    break;
  case CL_MEM_OBJECT_BUFFER:
  default:
    assert(0);
  }
  return nullptr;
}

static cl_mem
clCreateImage(cl_context             context,
              cl_mem_flags           flags,
              const cl_image_format* image_format,
              const cl_image_desc*   image_desc,
              void*                  host_ptr,
              cl_int*                errcode_ret)
{
  //validOrError(context,flags,image_format,image_desc,host_ptr,errcode_ret);

  // xlnx: ignore host_ptr in validation
  validOrError(context,flags & ~(CL_MEM_USE_HOST_PTR|CL_MEM_COPY_HOST_PTR),image_format,image_desc,nullptr,errcode_ret);

  void* user_ptr = nullptr;
  if (flags & CL_MEM_USE_HOST_PTR) {
    user_ptr = host_ptr;
    throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR, "clCreateImage: use host ptr is not supported");
  } else {
    if (flags & (CL_MEM_COPY_HOST_PTR | CL_MEM_ALLOC_HOST_PTR))
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR, "clCreateImage: unsupported host_ptr flags");
    if (flags & CL_MEM_COPY_HOST_PTR)
      throw xocl::error(CL_INVALID_IMAGE_DESCRIPTOR, "clCreateImage: unsupported host_ptr flags");
  }

  return mkImage(context,flags,image_format,image_desc,user_ptr,errcode_ret);
}

} // xocl

cl_mem
clCreateImage(cl_context              context,
              cl_mem_flags            flags,
              const cl_image_format*  image_format,
              const cl_image_desc*    image_desc,
              void*                   host_ptr,
              cl_int*                 errcode_ret)
{
    try {
      PROFILE_LOG_FUNCTION_CALL;
      return xocl::clCreateImage
        (context,flags,image_format,image_desc,host_ptr,errcode_ret);
    }
    catch (const xrt::error& ex) {
	xocl::send_exception_message(ex.what());
	xocl::assign(errcode_ret,ex.get_code());
    }
    catch (const std::exception& ex) {
	xocl::send_exception_message(ex.what());
	xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
    }
    return nullptr;
}
