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

#ifndef xocl_api_image_h
#define xocl_api_image_h

/**
 * data required for images
 * */
#include <CL/cl.h>

namespace xocl { namespace images {
//This should eventually have
//any xilinx h/w related image mapping.
enum class xlnx_image_type {
    XLNX_ALL_FORMATS,
    XLNX_UNSUPPORTED_FORMAT
};

const uint32_t cl_image_order[] = {
  CL_R, CL_A, CL_RG, CL_RA, CL_RGB, CL_RGBA, CL_BGRA, CL_ARGB,
  CL_INTENSITY, CL_LUMINANCE, CL_Rx, CL_RGx, CL_RGBx
};

const uint32_t cl_image_type[] = {
  CL_SNORM_INT8, CL_SNORM_INT16, CL_UNORM_INT8, CL_UNORM_INT16,
  CL_UNORM_SHORT_565, CL_UNORM_SHORT_555, CL_UNORM_INT_101010,
  CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32,
  CL_UNSIGNED_INT8, CL_UNSIGNED_INT16, CL_UNSIGNED_INT32,
  CL_HALF_FLOAT, CL_FLOAT
};

xlnx_image_type get_image_supported_format(const cl_image_format* fmt, cl_mem_flags flags) {
    //TODO: flags is not used currently.
    const uint32_t order = fmt->image_channel_order;
    const uint32_t type = fmt->image_channel_data_type;
    xlnx_image_type supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
    //For now all xilinx hardware maps to xlnx_image_type::XLNX_ALL_FORMATS
    switch (order) {
	case CL_R: 
	    switch (type) {
		case CL_UNORM_INT8:
		case CL_UNORM_INT16:
		case CL_SNORM_INT8:
		case CL_SNORM_INT16:
		case CL_SIGNED_INT8:
		case CL_SIGNED_INT16:
		case CL_SIGNED_INT32:
		case CL_UNSIGNED_INT8:
		case CL_UNSIGNED_INT16:
		case CL_UNSIGNED_INT32:
		case CL_HALF_FLOAT:
		case CL_FLOAT:
		    supported_type = xlnx_image_type::XLNX_ALL_FORMATS;
		    break;
		default:
		    supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
		    break;
	    }
	    break;
	    //case CL_DEPTH:
	    //    switch (type) {
	    //        case CL_UNORM_INT16:
	    //        case CL_FLOAT:
	    //            supported_type = xlnx_image_type::XLNX_ALL_FORMATS;
	    //            break;
	    //        default:
	    //            supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
	    //            break;
	    //    }
	    //    break;
	case CL_RG:
	    switch (type) {
		case CL_UNORM_INT8:
		case CL_UNORM_INT16:
		case CL_SNORM_INT8:
		case CL_SNORM_INT16:
		case CL_SIGNED_INT8:
		case CL_SIGNED_INT16:
		case CL_SIGNED_INT32:
		case CL_UNSIGNED_INT8:
		case CL_UNSIGNED_INT16:
		case CL_UNSIGNED_INT32:
		case CL_HALF_FLOAT:
		case CL_FLOAT:
		    supported_type = xlnx_image_type::XLNX_ALL_FORMATS;
		    break;
		default:
		    supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
		    break;
	    }
	    break;
	case CL_RGBA:
	    switch (type) {
		case CL_UNORM_INT8:
		case CL_UNORM_INT16:
		case CL_SNORM_INT8:
		case CL_SNORM_INT16:
		case CL_SIGNED_INT8:
		case CL_SIGNED_INT16:
		case CL_SIGNED_INT32:
		case CL_UNSIGNED_INT8:
		case CL_UNSIGNED_INT16:
		case CL_UNSIGNED_INT32:
		case CL_HALF_FLOAT:
		case CL_FLOAT:
		    supported_type = xlnx_image_type::XLNX_ALL_FORMATS;
		    break;
		default:
		    supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
		    break;
	    }
	    break;
	case CL_BGRA:
	    switch (type) {
		case CL_UNORM_INT8:
		    supported_type = xlnx_image_type::XLNX_ALL_FORMATS;
		    break;
		default:
		    supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
		    break;
	    }
	    break;
	    //	  case CL_sRGBA:
	    //	      switch (type) {
	    //		  case CL_UNORM_INT8:
	    //		      supported_type = xlnx_image_type::XLNX_ALL_FORMATS;
	    //		      break;
	    //		  default:
	    //		      supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
	    //		      break;
	    //	      }
	    //	      break;
	default:
	    supported_type = xlnx_image_type::XLNX_UNSUPPORTED_FORMAT;
	    break;
    }

    return supported_type ;
  }
}} //images, xocl


#endif


