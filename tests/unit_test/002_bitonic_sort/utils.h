// Copyright (c) 2009-2011 Intel Corporation
// All rights reserved.
// 
// WARRANTY DISCLAIMER
// 
// THESE MATERIALS ARE PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THESE
// MATERIALS, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Intel Corporation is the author of the Materials, and requests that all
// problem reports or change requests be submitted to it directly

#include "CL/cl.h"
//#include <d3d9.h>


#pragma once

const char* OCL_GetErrorString(cl_int error);

#define OCL_ABORT_ON_ERR(x)\
{\
	cl_int __err = x;\
	if( __err != CL_SUCCESS )\
	{\
		printf("OCL: ERROR: %s\n\
AT: %s(%i)\n\
IN: %s\n\n",OCL_GetErrorString(__err),__FILE__,__LINE__,__FUNCTION__);\
		abort();\
	}\
}

#define OCL_RETURN_ON_ERR(x)\
{\
	cl_int __err = x;\
	if( __err != CL_SUCCESS )\
	{\
		printf("OCL: ERROR: %s\n\
AT: %s(%i)\n\
IN: %s\n\n",OCL_GetErrorString(__err),__FILE__,__LINE__,__FUNCTION__);\
		return __err;\
	}\
}

union cl_types
{
	cl_mem mem_ptr;
	cl_sampler sampler_val;

	cl_char c_val;
	cl_char2 c2_val;
	cl_char3 c3_val;
	cl_char4 c4_val;
	cl_char8 c8_val;
	cl_char16 c16_val;
	cl_uchar uc_val;
	cl_uchar2 uc2_val;
	cl_uchar3 uc3_val;
	cl_uchar4 uc4_val;
	cl_uchar8 uc8_val;
	cl_uchar16 uc16_val;
	cl_short s_val;
	cl_short2 s2_val;
	cl_short3 s3_val;
	cl_short4 s4_val;
	cl_short8 s8_val;
	cl_short16 s16_val;
	cl_ushort us_val;
	cl_ushort2 us2_val;
	cl_ushort3 us3_val;
	cl_ushort4 us4_val;
	cl_ushort8 us8_val;
	cl_ushort16 us16_val;
	cl_int i_val;
	cl_int2 i2_val;
	cl_int3 i3_val;
	cl_int4 i4_val;
	cl_int8 i8_val;
	cl_int16 i16_val;
	cl_uint ui_val;
	cl_uint2 ui2_val;
	cl_uint3 ui3_val;
	cl_uint4 ui4_val;
	cl_uint8 ui8_val;
	cl_uint16 ui16_val;
	cl_long l_val;
	cl_long2 l2_val;
	cl_long3 l3_val;
	cl_long4 l4_val;
	cl_long8 l8_val;
	cl_long16 l16_val;
	cl_ulong ul_val;
	cl_ulong2 ul2_val;
	cl_ulong3 ul3_val;
	cl_ulong4 ul4_val;
	cl_ulong8 ul8_val;
	cl_ulong16 ul16_val;
	cl_half h_val;
	cl_float f_val;
	cl_float2 f2_val;
	cl_float4 f3_val;
	cl_float4 f4_val;
	cl_float8 f8_val;
	cl_float16 f16_val;
	cl_double d_val;
	cl_double2 d2_val;
	cl_double3 d3_val;
	cl_double4 d4_val;
	cl_double8 d8_val;
	cl_double16 d16_val;
};

union cl_types_ptr
{
	cl_mem *mem_ptr;
	cl_sampler *sampler_val;

	cl_char *c_val;
	cl_char2 *c2_val;
	cl_char3 *c3_val;
	cl_char4 *c4_val;
	cl_char8 *c8_val;
	cl_char16 *c16_val;
	cl_uchar *uc_val;
	cl_uchar2 *uc2_val;
	cl_uchar3 *uc3_val;
	cl_uchar4 *uc4_val;
	cl_uchar8 *uc8_val;
	cl_uchar16 *uc16_val;
	cl_short *s_val;
	cl_short2 *s2_val;
	cl_short3 *s3_val;
	cl_short4 *s4_val;
	cl_short8 *s8_val;
	cl_short16 *s16_val;
	cl_ushort *us_val;
	cl_ushort2 *us2_val;
	cl_ushort3 *us3_val;
	cl_ushort4 *us4_val;
	cl_ushort8 *us8_val;
	cl_ushort16 *us16_val;
	cl_int *i_val;
	cl_int2 *i2_val;
	cl_int3 *i3_val;
	cl_int4 *i4_val;
	cl_int8 *i8_val;
	cl_int16 *i16_val;
	cl_uint *ui_val;
	cl_uint2 *ui2_val;
	cl_uint3 *ui3_val;
	cl_uint4 *ui4_val;
	cl_uint8 *ui8_val;
	cl_uint16 *ui16_val;
	cl_long *l_val;
	cl_long2 *l2_val;
	cl_long3 *l3_val;
	cl_long4 *l4_val;
	cl_long8 *l8_val;
	cl_long16 *l16_val;
	cl_ulong *ul_val;
	cl_ulong2 *ul2_val;
	cl_ulong3 *ul3_val;
	cl_ulong4 *ul4_val;
	cl_ulong8 *ul8_val;
	cl_ulong16 *ul16_val;
	cl_half *h_val;
	cl_float *f_val;
	cl_float2 *f2_val;
	cl_float4 *f3_val;
	cl_float4 *f4_val;
	cl_float8 *f8_val;
	cl_float16 *f16_val;
	cl_double *d_val;
	cl_double2 *d2_val;
	cl_double3 *d3_val;
	cl_double4 *d4_val;
	cl_double8 *d8_val;
	cl_double16 *d16_val;
};

struct OCL_Environment_Desc
{
	//initialize everything to 0/NULL
	OCL_Environment_Desc(){memset(this,0,sizeof(OCL_Environment_Desc));}
	
	char*							sPlatformName; 

	cl_device_type					deviceType;
	cl_command_queue_properties		cmdQueueProps;
	cl_context_properties*			ctxProps;
	bool							devOnlyContext;	//don't create new platform context
	bool							intel_dx9_media_sharing;	
//	IDirect3DDevice9Ex*  pD3DD9;
};

// Single device and command queue
struct OCL_DeviceAndQueue
{
	//initialize everything to 0/NULL
	OCL_DeviceAndQueue(){memset(this,0,sizeof(OCL_DeviceAndQueue));}

	cl_device_id			mID;
	cl_command_queue		mCmdQueue;

	cl_context				mContext;

	cl_bool					mbImageSupport;
	char*					sDeviceExtensions;
	char*					sDeviceName;

	cl_int init(cl_context ctx, cl_device_id id);
	cl_int init(cl_context ctx, cl_device_id id, cl_command_queue_properties cmdProps);

	cl_int destroy();
};

// Single device and command queue
struct OCL_Platform
{
public:
	//initialize everything to 0/NULL
	OCL_Platform(){memset(this,0,sizeof(OCL_Platform));}

	cl_platform_id			mID;
	cl_context				mContext;
	cl_uint					uiNumDevices;
	char*					sPlatformName;
	char*					sPlatformExtensions;

	OCL_DeviceAndQueue*		mpDevices;

	cl_int init(cl_platform_id id);
	cl_int init(cl_platform_id id, OCL_Environment_Desc desc);


	cl_int destroy();
};


cl_kernel createKernelFromString(	cl_context* context,
									OCL_DeviceAndQueue* cl_devandqueue,
									const char* codeString, 
									const char* kernelName, 
									const char* options, 
									cl_program* programOut,
									cl_int* err);

cl_kernel createKernelFromFile(		cl_context* context,
									OCL_DeviceAndQueue* cl_devandqueue,
									const char* fileName, 
									const char* kernelName, 
									const char* options, 
									cl_program* programOut,
									cl_int* err);


void rand_clfloatn(void* out, size_t type_size, float max);
void line_clfloatn(void* out, float frand, size_t type_size);

cl_mem createRandomFloatVecBuffer(	cl_context* context,
									cl_mem_flags flags,
									size_t atomic_size,
									cl_uint num,
									cl_int *errcode_ret,
									float randmax = 1.0f);


cl_int fillRandomFloatVecBuffer(	cl_command_queue* cmdqueue,
								cl_mem* buffer,
								size_t atomic_size,
								cl_uint num,
								cl_event *ev = NULL,
								float randmax = 1.0f );


char *ReadSources(const char *fileName);

cl_platform_id GetIntelOCLPlatform();

void BuildFailLog( cl_program program, cl_device_id device_id );

bool SaveImageAsBMP ( unsigned int* ptr, int width, int height, const char* fileName);

