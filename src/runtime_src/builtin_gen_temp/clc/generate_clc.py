#!/usr/bin/python

#not supported :
#double versions clang currently throwing "requires cl_khr_fp64" extension error
#upsample "_ mangled"
#fract mod sincos "pointer"

import sys


clstub=0
header=0

outstr = [""]

if(len(sys.argv) > 1):
  if(sys.argv[1] == "header" ):
    header=1;
  else:
    header=0;


outstr.append("/*")
outstr.append(" Copyright 2013 Xilinx, Inc. All rights reserved.")
outstr.append(" This file contains confidential and proprietary information")
outstr.append(" of Xilinx, Inc. and is protected under U.S. and")
outstr.append(" international copyright and other intellectual property")
outstr.append(" laws.")
outstr.append("")
outstr.append(" DISCLAIMER")
outstr.append(" This disclaimer is not a license and does not grant any")
outstr.append(" rights to the materials distributed herewith. Except as")
outstr.append(" otherwise provided in a valid license issued to you by")
outstr.append(" Xilinx, and to the maximum extent permitted by applicable")
outstr.append(" law: (1) THESE MATERIALS ARE MADE AVAILABLE \"AS IS\" AND")
outstr.append(" WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES")
outstr.append(" AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING")
outstr.append(" BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-")
outstr.append(" INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and")
outstr.append(" (2) Xilinx shall not be liable (whether in contract or tort,")
outstr.append(" including negligence, or under any other theory of")
outstr.append(" liability) for any loss or damage of any kind or nature")
outstr.append(" related to, arising under or in connection with these")
outstr.append(" materials, including for any direct, or any indirect,")
outstr.append(" special, incidental, or consequential loss or damage")
outstr.append(" (including loss of data, profits, goodwill, or any type of")
outstr.append(" loss or damage suffered as a result of any action brought")
outstr.append(" by a third party) even if such damage or loss was")
outstr.append(" reasonably foreseeable or Xilinx had been advised of the")
outstr.append(" possibility of the same.")
outstr.append("")
outstr.append(" CRITICAL APPLICATIONS")
outstr.append(" Xilinx products are not designed or intended to be fail-")
outstr.append(" safe, or for use in any application requiring fail-safe")
outstr.append(" performance, such as life-support or safety devices or")
outstr.append(" systems, Class III medical devices, nuclear facilities,")
outstr.append(" applications related to the deployment of airbags, or any")
outstr.append(" other applications that could lead to death, personal")
outstr.append(" injury, or severe property or environmental damage")
outstr.append(" (individually and collectively, \"Critical")
outstr.append(" Applications\"\). Customer assumes the sole risk and")
outstr.append(" liability of any use of Xilinx products in Critical")
outstr.append(" Applications, subject only to applicable laws and")
outstr.append(" regulations governing limitations on product liability.")
outstr.append("")
outstr.append(" THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS")
outstr.append(" PART OF THIS FILE AT ALL TIMES.")
outstr.append("*/")
outstr.append("")
outstr.append("#ifndef _CLC_H_")
outstr.append("#define _CLC_H_")
outstr.append("\n");                                                
outstr.append("#define __xilinx__")
outstr.append("#define global __global")
outstr.append("#define local __local")
outstr.append("#define constant __constant")
outstr.append("#define private __private")
outstr.append("//SPIR Specification")
outstr.append("")
outstr.append("//SPIR section 2.1.1 Built-in Scalar Data Types")
outstr.append("typedef unsigned char uchar;")
outstr.append("typedef unsigned short ushort;")
outstr.append("typedef unsigned int uint;")
outstr.append("typedef unsigned long ulong;")
outstr.append("//typedef struct spirhalf* spirhalf;              //todo")
outstr.append("")
#outstr.append("//SPIR section 2.1.1.1 The size_t data type")
#outstr.append("/*")
#outstr.append("typedef struct spirsize_t* size_t;")
#outstr.append("typedef struct spirptrdiff_t* ptrdiff_t;")
#outstr.append("typedef struct spirintptr_t* intptr_t;")
#outstr.append("typedef struct spiruintptr_t* uintptr_t;")
#outstr.append("*/")
#outstr.append("//non SPIR compliant implementation of 2.1.1.1")
outstr.append("#if defined(__SPIR32__)")
outstr.append("typedef unsigned int size_t;")
outstr.append("typedef unsigned int ptrdiff_t;")
outstr.append("typedef int intptr_t;")
outstr.append("typedef unsigned int uintptr_t;")
outstr.append("#elif defined(__SPIR64__)")
outstr.append("typedef unsigned long long size_t;")
outstr.append("typedef unsigned long long ptrdiff_t;")
outstr.append("typedef long long intptr_t;")
outstr.append("typedef unsigned long long uintptr_t;")
outstr.append("#else")
outstr.append("#error \"must compile using spir or spir64 target\"")
outstr.append("#endif")

def openclvectortype(x) :
  global outstr
  outstr.append("typedef __attribute__(( ext_vector_type(2) )) __attribute__ ((aligned(2*sizeof("+x+")))) "+x+" "+x+"2;")
  outstr.append("typedef __attribute__(( ext_vector_type(3) )) __attribute__ ((aligned(4*sizeof("+x+")))) "+x+" "+x+"3;")
  outstr.append("typedef __attribute__(( ext_vector_type(4) )) __attribute__ ((aligned(4*sizeof("+x+")))) "+x+" "+x+"4;")
  outstr.append("typedef __attribute__(( ext_vector_type(8) )) __attribute__ ((aligned(8*sizeof("+x+")))) "+x+" "+x+"8;")
  outstr.append("typedef __attribute__(( ext_vector_type(16) )) __attribute__ ((aligned(16*sizeof("+x+")))) "+x+" "+x+"16;")
  return 

#openclvectortype(bool)                                //todo
openclvectortype("char")
openclvectortype("uchar")
openclvectortype("short")
openclvectortype("ushort")
openclvectortype("int")
openclvectortype("uint")
openclvectortype("long")
openclvectortype("ulong")
openclvectortype("float")
#openclvectortype(half)                                //todo
#openclvectortype(double)                              //todo

outstr.append("typedef void* reserve_id_t;")
outstr.append("")                                                                                  
outstr.append("//SPIR section 2.10.1.1 Synchronization Functions")
outstr.append("#define CLK_LOCAL_MEM_FENCE      0")
outstr.append("#define CLK_GLOBAL_MEM_FENCE     1")
outstr.append("")
outstr.append("//OpenCL Section 6.10 Preprocessor Directives and Macros")
outstr.append("//__FILE__ defined by GNU CPP")
outstr.append("//__LINE__ defined by GNU CPP")
outstr.append("#define __OPENCL_VERSION__ CL_VERSION_1_2")
outstr.append("#define CL_VERSION_1_0 100")
outstr.append("#define CL_VERSION_1_1 110")
outstr.append("#define CL_VERSION_1_2 120")
outstr.append("#define __OPENCL_C_VERSION__ CL_VERSION_1_2")
outstr.append("#define __ENDIAN_LITTLE__       1")
outstr.append("#define __kernel_exec(X,typen)   __kernel __attribute__((work_group_size_hint(X,1,1)))\\")
outstr.append("                                          __attribute__(vec_type_hint(typen))")
outstr.append("#define kernel_exec(X,typen) __kernel_exec(X,typen)")
outstr.append("//define __IMAGE_SUPPORT__      unsupported")
outstr.append("//define __FAST_RELAXED_MATH__  unsupported")
outstr.append("")
outstr.append("//OpenCL Section 6.12.3 Integer Function numeric constants")
outstr.append("#define CHAR_BIT 8")
outstr.append("#define CHAR_MAX SCHAR_MAX")
outstr.append("#define CHAR_MIN SCHAR_MIN")
outstr.append("#define INT_MAX 2147483647")
outstr.append("#define INT_MIN (-2147483647 - 1)")
outstr.append("#define LONG_MAX 0x7fffffffffffffffL")
outstr.append("#define LONG_MIN (-0x7fffffffffffffffL -1)")
outstr.append("#define SCHAR_MAX 127")
outstr.append("#define SCHAR_MIN (-127 - 1)")
outstr.append("#define SHRT_MAX 32767")
outstr.append("#define SHRT_MIN (-32767 -1)")
outstr.append("#define UCHAR_MAX 255")
outstr.append("#define USHRT_MAX 65535")
outstr.append("#define UINT_MAX 0xffffffff")
outstr.append("#define ULONG_MAX 0xffffffffffffffffUL")
outstr.append("")
outstr.append("//OpenCL Section 6.12.2.1 Floating-point macros and pragmas")
outstr.append("#define FLT_DIG 6")
outstr.append("#define FLT_MANT_DIG 24")
outstr.append("#define FLT_MAX_10_EXP +38")
outstr.append("#define FLT_MAX_EXP +128")
outstr.append("#define FLT_MIN_10_EXP -37")
outstr.append("#define FLT_MIN_EXP -125")
outstr.append("#define FLT_RADIX 2")
outstr.append("#define FLT_MAX 0x1.fffffep127f")
outstr.append("#define FLT_MIN 0x1.0p-126f")
outstr.append("#define FLT_EPSILON 0x1.0p-23f")
outstr.append("#define FP_ILOGB0 (-2147483647 - 1)")
outstr.append("#define FP_ILOGBNAN (-2147483647 - 1)")
outstr.append("")
outstr.append("//OpenCL Section 6.12.2 Math functions P251")
outstr.append("//The following constants were generated using generateclconstants")
outstr.append("#define M_E_F 2.71828174591064f")
outstr.append("#define M_LOG2E_F 1.44269502162933f")
outstr.append("#define M_LOG10E_F 0.434294492006302f")
outstr.append("#define M_LN2_F 0.6931471824646f")
outstr.append("#define M_LN10_F 2.30258512496948f")
outstr.append("#define M_PI_F 3.14159274101257f")
outstr.append("#define M_PI_2_F 1.57079637050629f")
outstr.append("#define M_PI_4_F 0.785398185253143f")
outstr.append("#define M_1_PI_F 0.318309873342514f")
outstr.append("#define M_2_PI_F 0.636619746685028f")
outstr.append("#define M_2_SQRTPI_F 1.1283792257309f")
outstr.append("#define M_SQRT2_F 1.41421353816986f")
outstr.append("#define M_SQRT1_2_F 0.70710676908493f")
outstr.append("#define MAXFLOAT 3.40282346638529e+38f")
outstr.append("")
outstr.append("//from include/bits/huge_valf.h")
outstr.append("#define HUGE_VALF \\")
outstr.append("  (__extension__                                                             \\")
outstr.append("   ((union { unsigned __l __attribute__((__mode__(__SI__))); float __d; })   \\")
outstr.append("    { __l: 0x7f800000UL }).__d)")
outstr.append("#define INFINITY HUGE_VALF")
outstr.append("//from include/bits/nan.h")
outstr.append("# define NAN \\")
outstr.append("  (__extension__                                                              \\")
outstr.append("   ((union { unsigned __l __attribute__ ((__mode__ (__SI__))); float __d; })  \\")
outstr.append("    { __l: 0x7fc00000UL }).__d)")
outstr.append("//from include/bits/huge_val.h")
outstr.append("# define HUGE_VAL \\")
outstr.append("  (__extension__                                                              \\")
outstr.append("   ((union { unsigned __l __attribute__((__mode__(__DI__))); double __d; })   \\")
outstr.append("    { __l: 0x7ff0000000000000ULL }).__d)")
outstr.append("")

outstr.append("int printf(__constant char *format, ...);")
outstr.append( "uint get_work_dim(void);")
outstr.append( "size_t get_global_size(uint dimindx);")
outstr.append( "size_t get_global_id(uint dimindx);")
outstr.append( "size_t get_local_size(uint dimindx);")
outstr.append( "size_t get_local_id(uint dimindx);")
outstr.append( "size_t get_num_groups(uint dimindx);")
outstr.append( "size_t get_group_id(uint dimindx);")
outstr.append( "size_t get_global_offset(uint dimindx);")
outstr.append("")

#builtin generation

as_types=     ["char","uchar","short","ushort","int","uint","long","ulong","float","double"]
as_typesbits= [8     ,8      ,16     ,16      ,32   ,32    ,64    ,64     ,32     ,64]

def builtin_rv_v(returngentype,gentype,name,clstub) :
 global outstr;
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( returngentype + " __attribute__ ((always_inline)) __attribute__((overloadable)) "+name+" ("+gentype+" f)" + term)

def builtin_rv_v_vp(returngentype,gentype,name,nonparamspace,clstub) :
 global outstr
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( returngentype + " __attribute__ ((always_inline)) __attribute__((overloadable)) "+name+" ("+gentype+" f, " + nonparamspace + " " +  gentype +" *iptr)" + term)

def builtin_rv_v_v(returngentype,gentype1,gentype2,name,clstub) :
 global outstr;
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( returngentype + " __attribute__ ((always_inline)) __attribute__((overloadable)) " + name + " (" + gentype1 + " f," + gentype2 + " g)" + term)

def builtin_rv_v_v_v(returngentype,gentype1,gentype2,gentype3,name,clstub) :
 global outstr;
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( returngentype + " __attribute__ ((always_inline)) __attribute__((overloadable)) "+name+" ("+gentype1+" f, "+gentype2+" g,"+gentype3+" h)" + term)

def builtin_rv_v_v_v_evt(returngentype,gentype1,gentype2,gentype3,gentype4,name,clstub) :
 global outstr;
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( returngentype + " __attribute__ ((always_inline)) __attribute__((overloadable)) "+name+" ("+gentype1+" f, "+gentype2+" g,"+gentype3+" h, "+gentype4+" evt)" + term)

def builtin_rv_v_v_v_v_evt(returngentype,gentype1,gentype2,gentype3,gentype4,gentype5,name,clstub) :
 global outstr;
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( returngentype + " __attribute__ ((always_inline)) __attribute__((overloadable)) "+name+" ("+gentype1+" f, "+gentype2+" g,"+gentype3+" h, "+gentype4+" i, "+gentype5+" evt)" + term)



#builtin_rgentypef_gentypef
# return type |     | first parameter type

#1 argument
def builtin_rgentypef_gentypef(name,clstub) :
  builtin_rv_v("float","float",name,clstub)
  builtin_rv_v("float2","float2",name,clstub)
  builtin_rv_v("float3","float3",name,clstub)
  builtin_rv_v("float4","float4",name,clstub)
  builtin_rv_v("float8","float8",name,clstub)
  builtin_rv_v("float16","float16",name,clstub)

def builtin_rgentyped_gentyped(name,clstub) :
  builtin_rv_v("double","double",name,clstub)
  builtin_rv_v("double2","double2",name,clstub)
  builtin_rv_v("double3","double3",name,clstub)
  builtin_rv_v("doubel4","double4",name,clstub)
  builtin_rv_v("double8","double8",name,clstub)
  builtin_rv_v("doubel16","double16",name,clstub)

def builtin_rgentypef_gentypeuint(name,clstub) :
  builtin_rv_v("float","uint",name,clstub)
  builtin_rv_v("float2","uint2",name,clstub)
  builtin_rv_v("float3","uint3",name,clstub)
  builtin_rv_v("float4","uint4",name,clstub)
  builtin_rv_v("float8","uint8",name,clstub)
  builtin_rv_v("float16","uint16",name,clstub)

def builtin_rgentyped_gentypeuint(name,clstub) :
  builtin_rv_v("double","uint",name,clstub)
  builtin_rv_v("double2","uint2",name,clstub)
  builtin_rv_v("double3","uint3",name,clstub)
  builtin_rv_v("double4","uint4",name,clstub)
  builtin_rv_v("double8","uint8",name,clstub)
  builtin_rv_v("double16","uint16",name,clstub)

def builtin_math_rgentype_gentype(name,clstub) :
  builtin_rgentypef_gentypef(name,clstub)
  #builtin_rgentyped_gentyped(name,clstub)

def builtin_math_rgentype_gentypeuint(name,clstub) :
  builtin_rgentypef_gentypeuint(name,clstub)
  #builtin_rgentyped_gentypeuint(name,clstub)


def builtin_common_rgentype_gentype(name,clstub) :
  builtin_math_rgentype_gentype(name,clstub)

def builtin_integer_rgentype_gentype(name,clstub) :
  builtin_rv_v("char","char",name,clstub)
  builtin_rv_v("char2","char2",name,clstub)
  builtin_rv_v("char3","char3",name,clstub)
  builtin_rv_v("char4","char4",name,clstub)
  builtin_rv_v("char8","char8",name,clstub)
  builtin_rv_v("char16","char16",name,clstub)
  builtin_rv_v("uchar","uchar",name,clstub)
  builtin_rv_v("uchar2","uchar2",name,clstub)
  builtin_rv_v("uchar3","uchar3",name,clstub)
  builtin_rv_v("uchar4","uchar4",name,clstub)
  builtin_rv_v("uchar8","uchar8",name,clstub)
  builtin_rv_v("uchar16","uchar16",name,clstub)
  builtin_rv_v("short","short",name,clstub)
  builtin_rv_v("short2","short2",name,clstub)
  builtin_rv_v("short3","short3",name,clstub)
  builtin_rv_v("short4","short4",name,clstub)
  builtin_rv_v("short8","short8",name,clstub)
  builtin_rv_v("short16","short16",name,clstub)
  builtin_rv_v("ushort","ushort",name,clstub)
  builtin_rv_v("ushort2","ushort2",name,clstub)
  builtin_rv_v("ushort3","ushort3",name,clstub)
  builtin_rv_v("ushort4","ushort4",name,clstub)
  builtin_rv_v("ushort8","ushort8",name,clstub)
  builtin_rv_v("ushort16","ushort16",name,clstub)
  builtin_rv_v("int","int",name,clstub)
  builtin_rv_v("int2","int2",name,clstub)
  builtin_rv_v("int3","int3",name,clstub)
  builtin_rv_v("int4","int4",name,clstub)
  builtin_rv_v("int8","int8",name,clstub)
  builtin_rv_v("int16","int16",name,clstub)
  builtin_rv_v("uint","uint",name,clstub)
  builtin_rv_v("uint2","uint2",name,clstub)
  builtin_rv_v("uint3","uint3",name,clstub)
  builtin_rv_v("uint4","uint4",name,clstub)
  builtin_rv_v("uint8","uint8",name,clstub)
  builtin_rv_v("uint16","uint16",name,clstub)
  builtin_rv_v("long","long",name,clstub)
  builtin_rv_v("long2","long2",name,clstub)
  builtin_rv_v("long3","long3",name,clstub)
  builtin_rv_v("long4","long4",name,clstub)
  builtin_rv_v("long8","long8",name,clstub)
  builtin_rv_v("long16","long16",name,clstub)
  builtin_rv_v("ulong","ulong",name,clstub)
  builtin_rv_v("ulong2","ulong2",name,clstub)
  builtin_rv_v("ulong3","ulong3",name,clstub)
  builtin_rv_v("ulong4","ulong4",name,clstub)
  builtin_rv_v("ulong8","ulong8",name,clstub)
  builtin_rv_v("ulong16","ulong16",name,clstub)


def builtin_integer_rugentype_gentype(name,clstub) :
  builtin_rv_v("uchar","char",name,clstub)
  builtin_rv_v("uchar2","char2",name,clstub)
  builtin_rv_v("uchar3","char3",name,clstub)
  builtin_rv_v("uchar4","char4",name,clstub)
  builtin_rv_v("uchar8","char8",name,clstub)
  builtin_rv_v("uchar16","char16",name,clstub)
  builtin_rv_v("uchar","uchar",name,clstub)
  builtin_rv_v("uchar2","uchar2",name,clstub)
  builtin_rv_v("uchar3","uchar3",name,clstub)
  builtin_rv_v("uchar4","uchar4",name,clstub)
  builtin_rv_v("uchar8","uchar8",name,clstub)
  builtin_rv_v("uchar16","uchar16",name,clstub)
  builtin_rv_v("ushort","short",name,clstub)
  builtin_rv_v("ushort2","short2",name,clstub)
  builtin_rv_v("ushort3","short3",name,clstub)
  builtin_rv_v("ushort4","short4",name,clstub)
  builtin_rv_v("ushort8","short8",name,clstub)
  builtin_rv_v("ushort16","short16",name,clstub)
  builtin_rv_v("ushort","ushort",name,clstub)
  builtin_rv_v("ushort2","ushort2",name,clstub)
  builtin_rv_v("ushort3","ushort3",name,clstub)
  builtin_rv_v("ushort4","ushort4",name,clstub)
  builtin_rv_v("ushort8","ushort8",name,clstub)
  builtin_rv_v("ushort16","ushort16",name,clstub)
  builtin_rv_v("uint","int",name,clstub)
  builtin_rv_v("uint2","int2",name,clstub)
  builtin_rv_v("uint3","int3",name,clstub)
  builtin_rv_v("uint4","int4",name,clstub)
  builtin_rv_v("uint8","int8",name,clstub)
  builtin_rv_v("uint16","int16",name,clstub)
  builtin_rv_v("uint","uint",name,clstub)
  builtin_rv_v("uint2","uint2",name,clstub)
  builtin_rv_v("uint3","uint3",name,clstub)
  builtin_rv_v("uint4","uint4",name,clstub)
  builtin_rv_v("uint8","uint8",name,clstub)
  builtin_rv_v("uint16","uint16",name,clstub)
  builtin_rv_v("ulong","long",name,clstub)
  builtin_rv_v("ulong2","long2",name,clstub)
  builtin_rv_v("ulong3","long3",name,clstub)
  builtin_rv_v("ulong4","long4",name,clstub)
  builtin_rv_v("ulong8","long8",name,clstub)
  builtin_rv_v("ulong16","long16",name,clstub)
  builtin_rv_v("ulong","ulong",name,clstub)
  builtin_rv_v("ulong2","ulong2",name,clstub)
  builtin_rv_v("ulong3","ulong3",name,clstub)
  builtin_rv_v("ulong4","ulong4",name,clstub)
  builtin_rv_v("ulong8","ulong8",name,clstub)
  builtin_rv_v("ulong16","ulong16",name,clstub)

def builtin_geometric_rtype_gentypef(type1,name,clstub) :
  builtin_rv_v(type1,"float",name,clstub)
  builtin_rv_v(type1,"float2",name,clstub)
  builtin_rv_v(type1,"float3",name,clstub)
  builtin_rv_v(type1,"float4",name,clstub)
  builtin_rv_v(type1,"float8",name,clstub)
  builtin_rv_v(type1,"float16",name,clstub)

def builtin_geometric_rtype_gentyped(type11,name,clstub) :
  builtin_rv_v(type1,"double",name,clstub)
  builtin_rv_v(type1,"double2",name,clstub)
  builtin_rv_v(type1,"double3",name,clstub)
  builtin_rv_v(type1,"double4",name,clstub)
  builtin_rv_v(type1,"double8",name,clstub)
  builtin_rv_v(type1,"double16",name,clstub)

def builtin_relational_rintn_floatn(name,clstub) :
  for typei in ["float"]:
    builtin_rv_v("int",typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v("int"+str(vectori),typei+str(vectori),name,clstub)

def builtin_relational_rlongn_doublen(name,clstub) :
  for typei in ["double"]:
    builtin_rv_v("long",typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v("long"+str(vectori),typei+str(vectori),name,clstub)

def builtin_relational_rint_igentype(name,clstub) :
  for typei in ["char","short","int","long"]:
    builtin_rv_v("int",typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v("int",typei+str(vectori),name,clstub)




#2 arguments

def builtin_rgentypef_gentypef_gentypef(name,clstub) :
  builtin_rv_v_v("float","float","float",name,clstub)
  builtin_rv_v_v("float2","float2","float2",name,clstub)
  builtin_rv_v_v("float3","float3","float3",name,clstub)
  builtin_rv_v_v("float4","float4","float4",name,clstub)
  builtin_rv_v_v("float8","float8","float8",name,clstub)
  builtin_rv_v_v("float16","float16","float16",name,clstub)

def builtin_rgentyped_gentyped_gentyped(name,clstub) :
  builtin_rv_v_v("double","double","double",name,clstub)
  builtin_rv_v_v("double2","double2","double2",name,clstub)
  builtin_rv_v_v("double3","double3","double3",name,clstub)
  builtin_rv_v_v("doubel4","double4","double4",name,clstub)
  builtin_rv_v_v("double8","double8","double8",name,clstub)
  builtin_rv_v_v("doubel16","double16","double16",name,clstub)

def builtin_rgentypef_gentypef_gentypeint(name,clstub) :
  builtin_rv_v_v("float","float","int",name,clstub)
  builtin_rv_v_v("float2","float2","int2",name,clstub)
  builtin_rv_v_v("float3","float3","int3",name,clstub)
  builtin_rv_v_v("float4","float4","int4",name,clstub)
  builtin_rv_v_v("float8","float8","int8",name,clstub)
  builtin_rv_v_v("float16","float16","int16",name,clstub)

def builtin_rgentyped_gentyped_gentypeint(name,clstub) :
  builtin_rv_v_v("double","double","int",name,clstub)
  builtin_rv_v_v("double2","double2","int2",name,clstub)
  builtin_rv_v_v("double3","double3","int3",name,clstub)
  builtin_rv_v_v("doubel4","double4","int4",name,clstub)
  builtin_rv_v_v("double8","double8","int8",name,clstub)
  builtin_rv_v_v("doubel16","double16","int16",name,clstub)

def builtin_rgentypefvectorsonly_gentypefvectorsonly_int(name,clstub) :
  builtin_rv_v_v("float2","float2","int",name,clstub)
  builtin_rv_v_v("float3","float3","int",name,clstub)
  builtin_rv_v_v("float4","float4","int",name,clstub)
  builtin_rv_v_v("float8","float8","int",name,clstub)
  builtin_rv_v_v("float16","float16","int",name,clstub)

def builtin_math_rgentype_gentype_gentype(name,clstub) :
  builtin_rgentypef_gentypef_gentypef(name,clstub)
  #builtin_rgentyped_gentyped_gentyped(name,clstub)

def builtin_math_rgentype_gentype_intgentype(name,clstub) :
  builtin_rgentypef_gentypef_gentypeint(name,clstub)
  #builtin_rgentyped_gentyped_gentypeint(name,clstub)

def builtin_math_rgentypevectorsonly_gentypevectorsonly_int(name,clstub) :
  builtin_rgentypefvectorsonly_gentypefvectorsonly_int(name,clstub)
  #builtin_rgentyped_gentyped_gentypeint(name,clstub)

def builtin_math_rgentypef_gentypef_type(type1,name,clstub) :
  builtin_rv_v_v("float","float",type1,name,clstub)
  builtin_rv_v_v("float2","float2",type1,name,clstub)
  builtin_rv_v_v("float3","float3",type1,name,clstub)
  builtin_rv_v_v("float4","float4",type1,name,clstub)
  builtin_rv_v_v("float8","float8",type1,name,clstub)
  builtin_rv_v_v("float16","float16",type1,name,clstub)


def builtin_common_rgentype_gentype_gentype(name,clstub):
  builtin_math_rgentype_gentype_gentype(name,clstub)

def builtin_common_rgentypef_gentypef_type(type1,name,clstub) :
  builtin_rv_v_v("float","float",type1,name,clstub)
  builtin_rv_v_v("float2","float2",type1,name,clstub)
  builtin_rv_v_v("float3","float3",type1,name,clstub)
  builtin_rv_v_v("float4","float4",type1,name,clstub)
  builtin_rv_v_v("float8","float8",type1,name,clstub)
  builtin_rv_v_v("float16","float16",type1,name,clstub)

def builtin_common_rgentyped_gentyped_type(type1,name,clstub) :
  builtin_rv_v_v("double","double",type1,name,clstub)
  builtin_rv_v_v("double2","double2",type1,name,clstub)
  builtin_rv_v_v("double3","double3",type1,name,clstub)
  builtin_rv_v_v("double4","double4",type1,name,clstub)
  builtin_rv_v_v("double8","double8",type1,name,clstub)
  builtin_rv_v_v("double16","double16",type1,name,clstub)

def builtin_common_rgentypef_type_gentypef(type1,name,clstub) :
  builtin_rv_v_v("float",type1,"float",name,clstub)
  builtin_rv_v_v("float2",type1,"float2",name,clstub)
  builtin_rv_v_v("float3",type1,"float3",name,clstub)
  builtin_rv_v_v("float4",type1,"float4",name,clstub)
  builtin_rv_v_v("float8",type1,"float8",name,clstub)
  builtin_rv_v_v("float16",type1,"float16",name,clstub)

def builtin_common_rgentyped_type_gentyped(type1,name,clstub) :
  builtin_rv_v_v("double",type1,"double",name,clstub)
  builtin_rv_v_v("double2",type1,"double2",name,clstub)
  builtin_rv_v_v("double3",type1,"double3",name,clstub)
  builtin_rv_v_v("double4",type1,"double4",name,clstub)
  builtin_rv_v_v("double8",type1,"double8",name,clstub)
  builtin_rv_v_v("double16",type1,"double16",name,clstub)

def builtin_integer_rgentype_gentype_gentype(name,clstub) :
  builtin_rv_v_v("char","char","char",name,clstub)
  builtin_rv_v_v("char2","char2","char2",name,clstub)
  builtin_rv_v_v("char3","char3","char3",name,clstub)
  builtin_rv_v_v("char4","char4","char4",name,clstub)
  builtin_rv_v_v("char8","char8","char8",name,clstub)
  builtin_rv_v_v("char16","char16","char16",name,clstub)
  builtin_rv_v_v("uchar","uchar","uchar",name,clstub)
  builtin_rv_v_v("uchar2","uchar2","uchar2",name,clstub)
  builtin_rv_v_v("uchar3","uchar3","uchar3",name,clstub)
  builtin_rv_v_v("uchar4","uchar4","uchar4",name,clstub)
  builtin_rv_v_v("uchar8","uchar8","uchar8",name,clstub)
  builtin_rv_v_v("uchar16","uchar16","uchar16",name,clstub)
  builtin_rv_v_v("short","short","short",name,clstub)
  builtin_rv_v_v("short2","short2","short2",name,clstub)
  builtin_rv_v_v("short3","short3","short3",name,clstub)
  builtin_rv_v_v("short4","short4","short4",name,clstub)
  builtin_rv_v_v("short8","short8","short8",name,clstub)
  builtin_rv_v_v("short16","short16","short16",name,clstub)
  builtin_rv_v_v("ushort","ushort","ushort",name,clstub)
  builtin_rv_v_v("ushort2","ushort2","ushort2",name,clstub)
  builtin_rv_v_v("ushort3","ushort3","ushort3",name,clstub)
  builtin_rv_v_v("ushort4","ushort4","ushort4",name,clstub)
  builtin_rv_v_v("ushort8","ushort8","ushort8",name,clstub)
  builtin_rv_v_v("ushort16","ushort16","ushort16",name,clstub)
  builtin_rv_v_v("int","int","int",name,clstub)
  builtin_rv_v_v("int2","int2","int2",name,clstub)
  builtin_rv_v_v("int3","int3","int3",name,clstub)
  builtin_rv_v_v("int4","int4","int4",name,clstub)
  builtin_rv_v_v("int8","int8","int8",name,clstub)
  builtin_rv_v_v("int16","int16","int16",name,clstub)
  builtin_rv_v_v("uint","uint","uint",name,clstub)
  builtin_rv_v_v("uint2","uint2","uint2",name,clstub)
  builtin_rv_v_v("uint3","uint3","uint3",name,clstub)
  builtin_rv_v_v("uint4","uint4","uint4",name,clstub)
  builtin_rv_v_v("uint8","uint8","uint8",name,clstub)
  builtin_rv_v_v("uint16","uint16","uint16",name,clstub)
  builtin_rv_v_v("long","long","long",name,clstub)
  builtin_rv_v_v("long2","long2","long2",name,clstub)
  builtin_rv_v_v("long3","long3","long3",name,clstub)
  builtin_rv_v_v("long4","long4","long4",name,clstub)
  builtin_rv_v_v("long8","long8","long8",name,clstub)
  builtin_rv_v_v("long16","long16","long16",name,clstub)
  builtin_rv_v_v("ulong","ulong","ulong",name,clstub)
  builtin_rv_v_v("ulong2","ulong2","ulong2",name,clstub)
  builtin_rv_v_v("ulong3","ulong3","ulong3",name,clstub)
  builtin_rv_v_v("ulong4","ulong4","ulong4",name,clstub)
  builtin_rv_v_v("ulong8","ulong8","ulong8",name,clstub)
  builtin_rv_v_v("ulong16","ulong16","ulong16",name,clstub)

def builtin_integer_rugentype_gentype_gentype(name,clstub) :
  builtin_rv_v_v("uchar","char","char",name,clstub)
  builtin_rv_v_v("uchar2","char2","char2",name,clstub)
  builtin_rv_v_v("uchar3","char3","char3",name,clstub)
  builtin_rv_v_v("uchar4","char4","char4",name,clstub)
  builtin_rv_v_v("uchar8","char8","char8",name,clstub)
  builtin_rv_v_v("uchar16","char16","char16",name,clstub)
  builtin_rv_v_v("uchar","uchar","uchar",name,clstub)
  builtin_rv_v_v("uchar2","uchar2","uchar2",name,clstub)
  builtin_rv_v_v("uchar3","uchar3","uchar3",name,clstub)
  builtin_rv_v_v("uchar4","uchar4","uchar4",name,clstub)
  builtin_rv_v_v("uchar8","uchar8","uchar8",name,clstub)
  builtin_rv_v_v("uchar16","uchar16","uchar16",name,clstub)
  builtin_rv_v_v("ushort","short","short",name,clstub)
  builtin_rv_v_v("ushort2","short2","short2",name,clstub)
  builtin_rv_v_v("ushort3","short3","short3",name,clstub)
  builtin_rv_v_v("ushort4","short4","short4",name,clstub)
  builtin_rv_v_v("ushort8","short8","short8",name,clstub)
  builtin_rv_v_v("ushort16","short16","short16",name,clstub)
  builtin_rv_v_v("ushort","ushort","ushort",name,clstub)
  builtin_rv_v_v("ushort2","ushort2","ushort2",name,clstub)
  builtin_rv_v_v("ushort3","ushort3","ushort3",name,clstub)
  builtin_rv_v_v("ushort4","ushort4","ushort4",name,clstub)
  builtin_rv_v_v("ushort8","ushort8","ushort8",name,clstub)
  builtin_rv_v_v("ushort16","ushort16","ushort16",name,clstub)
  builtin_rv_v_v("uint","int","int",name,clstub)
  builtin_rv_v_v("uint2","int2","int2",name,clstub)
  builtin_rv_v_v("uint3","int3","int3",name,clstub)
  builtin_rv_v_v("uint4","int4","int4",name,clstub)
  builtin_rv_v_v("uint8","int8","int8",name,clstub)
  builtin_rv_v_v("uint16","int16","int16",name,clstub)
  builtin_rv_v_v("uint","uint","uint",name,clstub)
  builtin_rv_v_v("uint2","uint2","uint2",name,clstub)
  builtin_rv_v_v("uint3","uint3","uint3",name,clstub)
  builtin_rv_v_v("uint4","uint4","uint4",name,clstub)
  builtin_rv_v_v("uint8","uint8","uint8",name,clstub)
  builtin_rv_v_v("uint16","uint16","uint16",name,clstub)
  builtin_rv_v_v("ulong","long","long",name,clstub)
  builtin_rv_v_v("ulong2","long2","long2",name,clstub)
  builtin_rv_v_v("ulong3","long3","long3",name,clstub)
  builtin_rv_v_v("ulong4","long4","long4",name,clstub)
  builtin_rv_v_v("ulong8","long8","long8",name,clstub)
  builtin_rv_v_v("ulong16","long16","long16",name,clstub)
  builtin_rv_v_v("ulong","ulong","ulong",name,clstub)
  builtin_rv_v_v("ulong2","ulong2","ulong2",name,clstub)
  builtin_rv_v_v("ulong3","ulong3","ulong3",name,clstub)
  builtin_rv_v_v("ulong4","ulong4","ulong4",name,clstub)
  builtin_rv_v_v("ulong8","ulong8","ulong8",name,clstub)
  builtin_rv_v_v("ulong16","ulong16","ulong16",name,clstub)

def builtin_rgentypef_gentypef_gentypefptr(name,nonparamspace,clstub) :
  builtin_rv_v_v("float","float",nonparamspace+" float *",name,clstub)
  builtin_rv_v_v("float2","float2",nonparamspace+" float2 *",name,clstub)
  builtin_rv_v_v("float3","float3",nonparamspace+" float3 *",name,clstub)
  builtin_rv_v_v("float4","float4",nonparamspace+" float4 *",name,clstub)
  builtin_rv_v_v("float8","float8",nonparamspace+" float8 *",name,clstub)
  builtin_rv_v_v("float16","float16",nonparamspace+" float16 *",name,clstub)

def builtin_rgentyped_gentyped_gentypedptr(name,nonparamspace,clstub) :
  builtin_rv_v_vp("double","double","double *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double2","double2","double2 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double3","double3","double3 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double4""double4","double4 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double8","double8","double8 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double16","double16","double16 *",name,nonparamspace,clstub)

def builtin_math_rgentype_gentype_gentypeptr(name,nonparamspace,clstub) :
  builtin_rgentypef_gentypef_gentypefptr(name,nonparamspace,clstub)
#  builtin_rgentyped_gentyped_gentypedptr(name,nonparamspace,clstub)

def builtin_rgentypef_gentypef_gentypeintptr(name,nonparamspace,clstub) :
  builtin_rv_v_v("float","float",nonparamspace+" int *",name,clstub)
  builtin_rv_v_v("float2","float2",nonparamspace+" int2 *",name,clstub)
  builtin_rv_v_v("float3","float3",nonparamspace+" int3 *",name,clstub)
  builtin_rv_v_v("float4","float4",nonparamspace+" int4 *",name,clstub)
  builtin_rv_v_v("float8","float8",nonparamspace+" int8 *",name,clstub)
  builtin_rv_v_v("float16","float16",nonparamspace+" int16 *",name,clstub)

def builtin_rgentyped_gentyped_gentypeintptr(name,nonparamspace,clstub) :
  builtin_rv_v_vp("double","double","int *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double2","double2","int2 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double3","double3","int3 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double4""double4","int4 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double8","double8","int8 *",name,nonparamspace,clstub)
  builtin_rv_v_vp("double16","double16","int16 *",name,nonparamspace,clstub)

def builtin_math_rgentype_gentype_gentypeintptr(name,nonparamspace,clstub) :
  builtin_rgentypef_gentypef_gentypeintptr(name,nonparamspace,clstub)
#  builtin_rgentyped_gentyped_gentypeintptr(name,nonparamspace,clstub)



def builtin_fastinteger_rgentype_gentype_gentype(name,clstub) :
  builtin_rv_v_v("int","int","int",name,clstub)
  builtin_rv_v_v("int2","int2","int2",name,clstub)
  builtin_rv_v_v("int3","int3","int3",name,clstub)
  builtin_rv_v_v("int4","int4","int4",name,clstub)
  builtin_rv_v_v("int8","int8","int8",name,clstub)
  builtin_rv_v_v("int16","int16","int16",name,clstub)
  builtin_rv_v_v("uint","uint","uint",name,clstub)
  builtin_rv_v_v("uint2","uint2","uint2",name,clstub)
  builtin_rv_v_v("uint3","uint3","uint3",name,clstub)
  builtin_rv_v_v("uint4","uint4","uint4",name,clstub)
  builtin_rv_v_v("uint8","uint8","uint8",name,clstub)
  builtin_rv_v_v("uint16","uint16","uint16",name,clstub)

def builtin_geometric_rtype_gentypef_gentypef(type1,name,clstub) :
  builtin_rv_v_v(type1,"float","float",name,clstub)
  builtin_rv_v_v(type1,"float2","float2",name,clstub)
  builtin_rv_v_v(type1,"float3","float3",name,clstub)
  builtin_rv_v_v(type1,"float4","float4",name,clstub)
  builtin_rv_v_v(type1,"float8","float8",name,clstub)
  builtin_rv_v_v(type1,"float16","float16",name,clstub)

def builtin_geometric_rtype_gentyped_gentyped(type1,name,clstub) :
  builtin_rv_v_v(type1,"double","double",name,clstub)
  builtin_rv_v_v(type1,"double2","double2",name,clstub)
  builtin_rv_v_v(type1,"double3","double3",name,clstub)
  builtin_rv_v_v(type1,"double4","double4",name,clstub)
  builtin_rv_v_v(type1,"double8","double8",name,clstub)
  builtin_rv_v_v(type1,"double16","double16",name,clstub)
 
def builtin_async_rvoid_gentypeconstglobalp_sizet(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    builtin_rv_v_v("void","const __global "+typei+" *","const __local "+typei+" *","size_t","size_t","event_t",name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v_v_evt("event_t","__global "+typei+str(vectori)+" *","const __local "+typei+str(vectori)+" *","size_t","size_t","event_t",name,clstub)


def builtin_vload_rgentypen_sizet_gentypeconstglobalp(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __global "+typei+" *",name+str(vectori),clstub)
 
def builtin_vload_rgentypen_sizet_gentypeconstlocalp(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __local "+typei+" *",name+str(vectori),clstub)

def builtin_vload_rgentypen_sizet_gentypeconstconstantp(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __constant "+typei+" *",name+str(vectori),clstub)
 
def builtin_vload_rgentypen_sizet_gentypeconstprivatep(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __private "+typei+" *",name+str(vectori),clstub)


def builtin_vload_half_rgentypen_sizet_constglobalhalfp(name,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __global half*",name+str(vectori),clstub)
 
def builtin_vload_half_rgentypen_sizet_constlocalhalfp(name,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __local half *",name+str(vectori),clstub)

def builtin_vload_half_rgentypen_sizet_constconstanthalfp(name,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __constant half *",name+str(vectori),clstub)
 
def builtin_vload_half_rgentypen_sizet_constprivatehalfp(name,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v(typei+str(vectori),"size_t","const __private half *",name+str(vectori),clstub)

def builtin_prefetch_rvoid_constglobalp_sizet(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    builtin_rv_v_v("void","const __global "+typei+"*","size_t",name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v("void","const __global "+typei+str(vectori)+"*","size_t",name,clstub)

def builtin_relational_rintn_floatn_floatn(name,clstub) :
  for typei in ["float"]:
    builtin_rv_v_v("int",typei,typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v("int"+str(vectori),typei+str(vectori),typei+str(vectori),name,clstub)

def builtin_relational_rlongn_doublen_doublen(name,clstub) :
  for typei in ["double"]:
    builtin_rv_v_v("int",typei,typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v("long"+str(vectori),typei+str(vectori),typei+std(vectori),name,clstub)



# 3 arguements

def builtin_rgentypef_gentypef_gentypef_gentypef(name,clstub) :
  builtin_rv_v_v_v("float","float","float","float",name,clstub)
  builtin_rv_v_v_v("float2","float2","float2","float2",name,clstub)
  builtin_rv_v_v_v("float3","float3","float3","float3",name,clstub)
  builtin_rv_v_v_v("float4","float4","float4","float4",name,clstub)
  builtin_rv_v_v_v("float8","float8","float8","float8",name,clstub)
  builtin_rv_v_v_v("float16","float16","float16","float16",name,clstub)

def builtin_rgentyped_gentyped_gentyped_gentyped(name,clstub) :
  builtin_rv_v_v_v("double","double","double","double",name,clstub)
  builtin_rv_v_v_v("double2","double2","double2","double2",name,clstub)
  builtin_rv_v_v_v("double3","double3","double3","double3",name,clstub)
  builtin_rv_v_v_v("double4","double4","double4","double4",name,clstub)
  builtin_rv_v_v_v("double8","double8","double8","double8",name,clstub)
  builtin_rv_v_v_v("double16","double16","double16","double16",name,clstub)

def builtin_rgentypef_gentypef_gentypef_gentypeintptr(name,nonparamspace,clstub) :
  builtin_rv_v_v_v("float","float","float",nonparamspace+" int *",name,clstub)
  builtin_rv_v_v_v("float2","float2","float2",nonparamspace+" int2 *",name,clstub)
  builtin_rv_v_v_v("float3","float3","float3",nonparamspace+" int3 *",name,clstub)
  builtin_rv_v_v_v("float4","float4","float4",nonparamspace+" int4 *",name,clstub)
  builtin_rv_v_v_v("float8","float8","float8",nonparamspace+" int8 *",name,clstub)
  builtin_rv_v_v_v("float16","float16","float16",nonparamspace+" int16 *",name,clstub)

def builtin_rgentyped_gentyped_gentyped_gentypeintptr(name,nonparamspace,clstub) :
  builtin_rv_v_v_v("double","double","double",nonparamspace+" int *",name,clstub)
  builtin_rv_v_v_v("double2","double2","double2",nonparamspace+" int2 *",name,clstub)
  builtin_rv_v_v_v("double3","double3","double3",nonparamspace+" int3 *",name,clstub)
  builtin_rv_v_v_v("double4","double4","double4",nonparamspace+" int4 *",name,clstub)
  builtin_rv_v_v_v("double8","double8","double8",nonparamspace+" int8 *",name,clstub)
  builtin_rv_v_v_v("double16","double16","double16",nonparamspace+" int16 *",name,clstub)

def builtin_math_rgentype_gentype_gentype_gentype(name,clstub) :
  builtin_rgentypef_gentypef_gentypef_gentypef(name,clstub)
  #builtin_rgentyped_gentyped_gentyped_gentyped(name,clstub)

def builtin_math_rgentype_gentype_gentype_gentypeintptr(name,nonparamspace,clstub) :
  builtin_rgentypef_gentypef_gentypef_gentypeintptr(name,nonparamspace,clstub)
#  builtin_rgentyped_gentyped_gentypeintptr(name,nonparamspace,clstub)

def duplicatefree_builtin_common_rgentype_gentype_gentype_gentype(name,clstub):
  builtin_math_rgentype_gentype_gentype_gentype(name,clstub)

def builtin_common_rgentype_gentype_gentype_gentype(name,clstub):
  builtin_math_rgentype_gentype_gentype_gentype(name,clstub)

def builtin_common_rgentypef_gentypef_type_type(type1,type2,name,clstub):
  builtin_rv_v_v_v("float","float",type1,type2,name,clstub)
  builtin_rv_v_v_v("float2","float2",type1,type2,name,clstub)
  builtin_rv_v_v_v("float3","float3",type1,type2,name,clstub)
  builtin_rv_v_v_v("float4","float4",type1,type2,name,clstub)
  builtin_rv_v_v_v("float8","float8",type1,type2,name,clstub)
  builtin_rv_v_v_v("float16","float16",type1,type2,name,clstub)

def builtin_common_rgentyped_gentyped_type_type(type1,type2,name,clstub):
  builtin_rv_v_v_v("double","double",type1,type2,name,clstub)
  builtin_rv_v_v_v("double2","double2",type1,type2,name,clstub)
  builtin_rv_v_v_v("double3","double3",type1,type2,name,clstub)
  builtin_rv_v_v_v("double4","double4",type1,type2,name,clstub)
  builtin_rv_v_v_v("double8","double8",type1,type2,name,clstub)
  builtin_rv_v_v_v("double16","double16",type1,type2,name,clstub)

def builtin_common_rgentypef_type_type_gentypef(type1,type2,name,clstub):
  builtin_rv_v_v_v("float",type1,type2,"float",name,clstub)
  builtin_rv_v_v_v("float2",type1,type2,"float2",name,clstub)
  builtin_rv_v_v_v("float3",type1,type2,"float3",name,clstub)
  builtin_rv_v_v_v("float4",type1,type2,"float4",name,clstub)
  builtin_rv_v_v_v("float8",type1,type2,"float8",name,clstub)
  builtin_rv_v_v_v("float16",type1,type2,"float16",name,clstub)

def builtin_common_rgentyped_type_type_gentyped(type1,type2,name,clstub):
  builtin_rv_v_v_v("double",type1,type2,"double",name,clstub)
  builtin_rv_v_v_v("double2",type1,type2,"double2",name,clstub)
  builtin_rv_v_v_v("double3",type1,type2,"double3",name,clstub)
  builtin_rv_v_v_v("double4",type1,type2,"double4",name,clstub)
  builtin_rv_v_v_v("double8",type1,type2,"double8",name,clstub)
  builtin_rv_v_v_v("double16",type1,type2,"double16",name,clstub)

def builtin_common_rgentypef_gentypef_gentypef_type(type1,name,clstub):
  builtin_rv_v_v_v("float","float","float",type1,name,clstub)
  builtin_rv_v_v_v("float2","float2","float2",type1,name,clstub)
  builtin_rv_v_v_v("float3","float3","float3",type1,name,clstub)
  builtin_rv_v_v_v("float4","float4","float4",type1,name,clstub)
  builtin_rv_v_v_v("float8","float8","float8",type1,name,clstub)
  builtin_rv_v_v_v("float16","float16","float16",type1,name,clstub)

def builtin_common_rgentyped_gentyped_gentyped_type(type1,name,clstub):
  builtin_rv_v_v_v("double","double","double",type1,name,clstub)
  builtin_rv_v_v_v("double2","double2","double2",type1,name,clstub)
  builtin_rv_v_v_v("double3","double3","double3",type1,name,clstub)
  builtin_rv_v_v_v("double4","double4","double4",type1,name,clstub)
  builtin_rv_v_v_v("double8","double8","double8",type1,name,clstub)
  builtin_rv_v_v_v("double16","double16","double16",type1,name,clstub)

def builtin_integer_rgentype_gentype_gentype_gentype(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong"]:
    builtin_rv_v_v_v(typei,typei,typei,typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v(typei+str(vectori),typei+str(vectori),typei+str(vectori),typei+str(vectori),name,clstub)


def builtin_integer_rgentype_gentype_sgentype_sgentype(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong"]:
    builtin_rv_v_v_v(typei,typei,typei,typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v(typei+str(vectori),typei+str(vectori),typei,typei,name,clstub)

def builtin_fastinteger_rgentype_gentype_gentype_gentype(name,clstub) :
  for typei in ["int","uint"]:
    builtin_rv_v_v_v(typei,typei,typei,typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v(typei+str(vectori),typei+str(vectori),typei+str(vectori),typei+str(vectori),name,clstub)

def builtin_vstore_rvoid_gentypen_sizet_gentypeglobalp(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v("void",typei+str(vectori),"size_t","__global "+typei+" *",name+str(vectori),clstub)
 
def builtin_vstore_rvoid_gentypen_sizet_gentypelocalp(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v("void",typei+str(vectori),"size_t","__local "+typei+" *",name+str(vectori),clstub)

def builtin_vstore_rvoid_gentypen_sizet_gentypeprivatep(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v("void",typei+str(vectori),"size_t","__private "+typei+" *",name+str(vectori),clstub)


def builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp(name,nameextension,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v("void",typei+str(vectori),"size_t","__global half*",name+str(vectori)+nameextension,clstub)
 
def builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp(name,nameextension,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v("void",typei+str(vectori),"size_t","__local half*",name+str(vectori)+nameextension,clstub)

def builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep(name,nameextension,clstub) :
  for typei in ["float"]:
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v("void",typei+str(vectori),"size_t","__private half*",name+str(vectori)+nameextension,clstub)
 
def builtin_relational_rgentype_gentype_gentype_gentype(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    #,"double"]:
    builtin_rv_v_v_v(typei,typei,typei,typei,name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v(typei+str(vectori),typei+str(vectori),typei+str(vectori),typei+str(vectori),name,clstub)

#special rules for select
#gentype select(gentype a, gentype b, igentype c)
#gentype select(gentype a, gentype b, ugentype c)
#where igentype must have the same number of elements and bits as gentype
def builtin_relational_select_gentype_gentype_igentype(name,clstub) :
  as_igentypes = ["char", "short", "int", "long"]
  as_igentypesbits = [8,16,32,64]
  for gentype in range(9):
    for igentype in range(4):
      if(as_typesbits[gentype]==as_igentypesbits[igentype]) :
        builtin_rv_v_v_v(as_types[gentype],as_types[gentype],as_types[gentype],as_igentypes[igentype],name,clstub)
        for vectori in [2,3,4,8,16] :
          builtin_rv_v_v_v(as_types[gentype]+str(vectori),as_types[gentype]+str(vectori),as_types[gentype]+str(vectori),as_igentypes[igentype]+str(vectori),name,clstub)

def builtin_relational_select_gentype_gentype_ugentype(name,clstub) :
  as_igentypes = ["uchar", "ushort", "uint", "ulong"]
  as_igentypesbits = [8,16,32,64]
  for gentype in range(9):
    for igentype in range(4):
      if(as_typesbits[gentype]==as_igentypesbits[igentype]) :
        builtin_rv_v_v_v(as_types[gentype],as_types[gentype],as_types[gentype],as_igentypes[igentype],name,clstub)
        for vectori in [2,3,4,8,16] :
          builtin_rv_v_v_v(as_types[gentype]+str(vectori),as_types[gentype]+str(vectori),as_types[gentype]+str(vectori),as_igentypes[igentype]+str(vectori),name,clstub)

# 4 arguements

def builtin_async_reventt_gentypelocalp_gentypeconstglobalp_sizet_eventt(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    builtin_rv_v_v_v_evt("event_t","__local "+typei+" *","const __global "+typei+" *","size_t","event_t",name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v_evt("event_t","__local "+typei+str(vectori)+" *","const __global "+typei+str(vectori)+" *","size_t","event_t",name,clstub)

def builtin_async_reventt_gentypeglobalp_gentypeconstlocalp_sizet_eventt(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    builtin_rv_v_v_v_evt("event_t","__global "+typei+" *","const __local "+typei+" *","size_t","event_t",name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v_evt("event_t","__global "+typei+str(vectori)+" *","const __local "+typei+str(vectori)+" *","size_t","event_t",name,clstub)

# 5 arguements

def builtin_async_reventt_gentypelocalp_gentypeconstglobalp_sizet_sizet_eventt(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    builtin_rv_v_v_v_v_evt("event_t","__local "+typei+" *","const __global "+typei+" *","size_t","size_t","event_t",name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v_v_evt("event_t","__local "+typei+str(vectori)+" *","const __global "+typei+str(vectori)+" *","size_t","size_t","event_t",name,clstub)

def builtin_async_reventt_gentypeglobalp_gentypeconstlocalp_sizet_sizet_eventt(name,clstub) :
  for typei in ["char","uchar","short","ushort","int","uint","long","ulong","float"]:
    builtin_rv_v_v_v_v_evt("event_t","__global "+typei+" *","const __local "+typei+" *","size_t","size_t","event_t",name,clstub)
    for vectori in [2,3,4,8,16]:
      builtin_rv_v_v_v_v_evt("event_t","__global "+typei+str(vectori)+" *","const __local "+typei+str(vectori)+" *","size_t","size_t","event_t",name,clstub)


#Section 6.2.4.2 Reinterpreting Types Using as_type() and as_typen()
#All data types described in tables 6.1 and 6.2 except bool, half and void
#size_t ptrdiff_t intptr_t uintptr_t not supported
#8 bit          char, uchar
#32 bit         int, uint, float
#64 bit         long, ulong, double
#as_type + as_type bits defined above
as_vectorext= ["","","2","3","4","","","","8","","","","","","","","16"]
for fromtype in range(9) :
  for totype in range(9) :
   for fromvectori in [1,2,3,4,8,16]:
      for tovectori in [1,2,3,4,8,16]:
        if((as_typesbits[fromtype]*fromvectori) == (as_typesbits[totype]*tovectori)) and (fromtype!=totype) :
          builtin_rv_v(as_types[totype]+as_vectorext[tovectori],as_types[fromtype]+as_vectorext[fromvectori],"as_"+as_types[totype]+as_vectorext[tovectori],clstub);
        #as_type3(vec4) also allowed
        if(((fromvectori==4 and tovectori==3) or (fromvectori==3 and tovectori==4)) and (as_typesbits[fromtype]==as_typesbits[totype])) :
          builtin_rv_v(as_types[totype]+as_vectorext[tovectori],as_types[fromtype]+as_vectorext[fromvectori],"as_"+as_types[totype]+as_vectorext[tovectori],clstub);

#Section 6.2.3 Explicit Conversions
#All data types described in tables 6.1 and 6.2 except bool, half, size_t, ptrdiff_t, intptr_t, uintptr_t and void
#The number of elements in the source and destination must match
#standard conversions
convert_types=     ["char","uchar","short","ushort","int","uint","long","ulong","float","double"]
convert_vectorext= ["","","2","3","4","","","","8","","","","","","","","16"]
for fromtype in range(9) :
  for totype in range(9) :
   for fromvectori in [1,2,3,4,8,16]:
      for tovectori in [1,2,3,4,8,16]:
        if(fromvectori==tovectori):
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori],clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_rte",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_rtz",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_rtp",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_rtn",clstub);
#conversions with saturation (no conversions to floating point)
convert_types=     ["char","uchar","short","ushort","int","uint","long","ulong","float","double"]
convert_vectorext= ["","","2","3","4","","","","8","","","","","","","","16"]
for fromtype in range(9) :
  for totype in range(8) :
   for fromvectori in [1,2,3,4,8,16]:
      for tovectori in [1,2,3,4,8,16]:
        if(fromvectori==tovectori):
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_sat",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_sat_rte",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_sat_rtz",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_sat_rtp",clstub);
          builtin_rv_v(convert_types[totype]+convert_vectorext[tovectori],convert_types[fromtype]+convert_vectorext[fromvectori],"convert_"+convert_types[totype]+convert_vectorext[tovectori]+"_sat_rtn",clstub);
#conversion rounding mode unsupported


#
#Section 6.12.2 Math Functions

outstr.append("")
outstr.append(" //OpenCL 1.2 Section 6.12.2 Math Functions")
outstr.append(" //Table 6.8")
outstr.append(" //\"gentype\" functions can take")
outstr.append(" //float, float2, float3, float4, float8, float16")
outstr.append(" //double, double2, double3, double4, double8, double16")

builtin_math_rgentype_gentype("acos",clstub) 
builtin_math_rgentype_gentype("acosh",clstub)  
builtin_math_rgentype_gentype("acospi",clstub)  
builtin_math_rgentype_gentype("asin",clstub)
builtin_math_rgentype_gentype("asinh",clstub)
builtin_math_rgentype_gentype("asinpi",clstub)  
builtin_math_rgentype_gentype("atan",clstub)  
builtin_math_rgentype_gentype("atanpi",clstub)  
builtin_math_rgentype_gentype_gentype("atan2",clstub)  
builtin_math_rgentype_gentype("atanh",clstub)  
builtin_math_rgentype_gentype_gentype("atan2pi",clstub)  
builtin_math_rgentype_gentype("cbrt",clstub)  
builtin_math_rgentype_gentype("ceil",clstub)  
builtin_math_rgentype_gentype_gentype("copysign",clstub)  
builtin_math_rgentype_gentype("cos",clstub)
builtin_math_rgentype_gentype("cosh",clstub)
builtin_math_rgentype_gentype("cospi",clstub)  
builtin_math_rgentype_gentype("erfc",clstub)  
builtin_math_rgentype_gentype("erf",clstub)  
builtin_math_rgentype_gentype("exp",clstub)  
builtin_math_rgentype_gentype("exp2",clstub)  
builtin_math_rgentype_gentype("exp10",clstub)  
builtin_math_rgentype_gentype("expm1",clstub)  
builtin_math_rgentype_gentype("fabs",clstub)  
builtin_math_rgentype_gentype_gentype("fdim",clstub)  
builtin_math_rgentype_gentype("floor",clstub)  
builtin_math_rgentype_gentype_gentype_gentype("fma",clstub)  
builtin_math_rgentype_gentype_gentype("fmax",clstub)
builtin_math_rgentypef_gentypef_type("float","fmax",clstub)
builtin_math_rgentype_gentype_gentype("fmin",clstub)
builtin_math_rgentypef_gentypef_type("float","fmin",clstub)
builtin_math_rgentype_gentype_gentype("fmod",clstub)  
builtin_math_rgentype_gentype_gentypeptr("fract","__global",clstub)  
builtin_math_rgentype_gentype_gentypeptr("fract","__local",clstub)  
builtin_math_rgentype_gentype_gentypeptr("fract","__private",clstub)  
builtin_math_rgentype_gentype_gentypeintptr("frexp","__global",clstub)  
builtin_math_rgentype_gentype_gentypeintptr("frexp","__local",clstub)  
builtin_math_rgentype_gentype_gentypeintptr("frexp","__private",clstub)  
builtin_math_rgentype_gentype_gentype("hypot",clstub)  
builtin_relational_rintn_floatn("ilogb",clstub)
builtin_math_rgentype_gentype_intgentype("ldexp",clstub)  
builtin_math_rgentypevectorsonly_gentypevectorsonly_int("ldexp",clstub)  
builtin_math_rgentype_gentype("lgamma",clstub)  
builtin_math_rgentype_gentype_gentypeintptr("lgamma_r","__global",clstub)  
builtin_math_rgentype_gentype_gentypeintptr("lgamma_r","__local",clstub)  
builtin_math_rgentype_gentype_gentypeintptr("lgamma_r","__private",clstub)  
builtin_math_rgentype_gentype("log",clstub)  
builtin_math_rgentype_gentype("log2",clstub)  
builtin_math_rgentype_gentype("log10",clstub)  
builtin_math_rgentype_gentype("log1p",clstub)  
builtin_math_rgentype_gentype("logb",clstub)  
builtin_math_rgentype_gentype_gentype_gentype("mad",clstub)  
builtin_math_rgentype_gentype_gentype("maxmag",clstub)  
builtin_math_rgentype_gentype_gentype("minmag",clstub)  
builtin_math_rgentype_gentype_gentypeptr("modf","__global",clstub)  
builtin_math_rgentype_gentype_gentypeptr("modf","__local",clstub)  
builtin_math_rgentype_gentype_gentypeptr("modf","__private",clstub)  
builtin_math_rgentype_gentypeuint("nan",clstub)  
builtin_math_rgentype_gentype_gentype("nextafter",clstub)  
builtin_math_rgentype_gentype_gentype("pow",clstub)  
builtin_math_rgentype_gentype_intgentype("pown",clstub)  
builtin_math_rgentype_gentype_gentype("powr",clstub)  
builtin_math_rgentype_gentype_gentype("remainder",clstub)  
builtin_math_rgentype_gentype_gentype_gentypeintptr("remquo","__global",clstub)
builtin_math_rgentype_gentype_gentype_gentypeintptr("remquo","__local",clstub)
builtin_math_rgentype_gentype_gentype_gentypeintptr("remquo","__private",clstub)
builtin_math_rgentype_gentype("rint",clstub)  
builtin_math_rgentype_gentype_intgentype("rootn",clstub)  
builtin_math_rgentype_gentype("round",clstub)  
builtin_math_rgentype_gentype("rsqrt",clstub)  
builtin_math_rgentype_gentype("sin",clstub)  
builtin_math_rgentype_gentype_gentypeptr("sincos","__global",clstub)  
builtin_math_rgentype_gentype_gentypeptr("sincos","__local",clstub)  
builtin_math_rgentype_gentype_gentypeptr("sincos","__private",clstub)  
builtin_math_rgentype_gentype("sinh",clstub)  
builtin_math_rgentype_gentype("sinpi",clstub)  
builtin_math_rgentype_gentype("sqrt",clstub)  
builtin_math_rgentype_gentype("tan",clstub)  
builtin_math_rgentype_gentype("tanh",clstub)  
builtin_math_rgentype_gentype("tanpi",clstub)  
builtin_math_rgentype_gentype("tgamma",clstub)  
builtin_math_rgentype_gentype("trunc",clstub)



outstr.append("")
outstr.append("//OpenCL 1.2 Section 6.12.2 Math Functions Half")
outstr.append("//Table 6.9")
outstr.append("//\"gentype\" functions can take")
outstr.append("//float, float2, float3, float4, float8, float16")

builtin_rgentypef_gentypef("half_cos",clstub)
builtin_rgentypef_gentypef_gentypef("half_divide",clstub)
builtin_rgentypef_gentypef("half_exp",clstub)
builtin_rgentypef_gentypef("half_exp2",clstub)
builtin_rgentypef_gentypef("half_exp10",clstub)
builtin_rgentypef_gentypef("half_log",clstub)
builtin_rgentypef_gentypef("half_log2",clstub)
builtin_rgentypef_gentypef("half_log10",clstub)
builtin_rgentypef_gentypef_gentypef("half_powr",clstub)
builtin_rgentypef_gentypef("half_recip",clstub)
builtin_rgentypef_gentypef("half_rsqrt",clstub)
builtin_rgentypef_gentypef("half_sin",clstub)
builtin_rgentypef_gentypef("half_sqrt",clstub)
builtin_rgentypef_gentypef("half_tan",clstub)
builtin_rgentypef_gentypef("native_cos",clstub)
builtin_rgentypef_gentypef_gentypef("native_divide",clstub)
builtin_rgentypef_gentypef("native_exp",clstub)
builtin_rgentypef_gentypef("native_exp2",clstub)
builtin_rgentypef_gentypef("native_exp10",clstub)
builtin_rgentypef_gentypef("native_log",clstub)
builtin_rgentypef_gentypef("native_log2",clstub)
builtin_rgentypef_gentypef("native_log10",clstub)
builtin_rgentypef_gentypef_gentypef("native_powr",clstub)
builtin_rgentypef_gentypef("native_recip",clstub)
builtin_rgentypef_gentypef("native_rsqrt",clstub)
builtin_rgentypef_gentypef("native_sin",clstub)
builtin_rgentypef_gentypef("native_sqrt",clstub)
builtin_rgentypef_gentypef("native_tan",clstub)


#Integer Functions
#OpenCL 1.2 Section 6.12.3 
#Table 6.10                                                                            \n\
#"gentype" functions can take                                                          \n\
#char, char{2|3|4|8|16}                                                                \n\
#uchar, uchar{2,|3|4|8|16}                                                             \n\
#short, ushort{2|3|4|8|16}                                                             \n\
#int, int{2|3|4|8|16}                                                                  \n\
#uint, uint{2|3|4|8|16}                                                                \n\
#long, long{2|3|4|8|!6}                                                                \n\
#ulong, ulong{2|3|4|8|16}                                                              \n\

builtin_integer_rugentype_gentype("abs",clstub)
builtin_integer_rugentype_gentype_gentype("abs_diff",clstub)
builtin_integer_rgentype_gentype_gentype("add_sat",clstub)
builtin_integer_rgentype_gentype_gentype("hadd",clstub)
builtin_integer_rgentype_gentype_gentype("rhadd",clstub)
builtin_integer_rgentype_gentype_gentype_gentype("clamp",clstub)
builtin_integer_rgentype_gentype_sgentype_sgentype("clamp",clstub)
builtin_integer_rgentype_gentype("clz",clstub)
builtin_integer_rgentype_gentype_gentype_gentype("mad_hi",clstub)
builtin_integer_rgentype_gentype_gentype_gentype("mad_sat",clstub)
builtin_integer_rgentype_gentype_gentype("max",clstub)
builtin_integer_rgentype_gentype_gentype("min",clstub)
builtin_integer_rgentype_gentype_gentype("mul_hi",clstub)
builtin_integer_rgentype_gentype_gentype("rotate",clstub)
builtin_integer_rgentype_gentype_gentype("sub_sat",clstub)
builtin_rv_v_v("short","char","uchar","upsample",clstub)
builtin_rv_v_v("ushort","uchar","uchar","upsample",clstub)
builtin_rv_v_v("short2","char2","uchar2","upsample",clstub)
builtin_rv_v_v("ushort2","uchar2","uchar2","upsample",clstub)
builtin_rv_v_v("short3","char3","uchar3","upsample",clstub)
builtin_rv_v_v("ushort3","uchar3","uchar3","upsample",clstub)
builtin_rv_v_v("short4","char4","uchar4","upsample",clstub)
builtin_rv_v_v("ushort4","uchar4","uchar4","upsample",clstub)
builtin_rv_v_v("short8","char8","uchar8","upsample",clstub)
builtin_rv_v_v("ushort8","uchar8","uchar8","upsample",clstub)
builtin_rv_v_v("short16","char16","uchar16","upsample",clstub)
builtin_rv_v_v("ushort16","uchar16","uchar16","upsample",clstub)
builtin_rv_v_v("int","short","ushort","upsample",clstub)
builtin_rv_v_v("uint","ushort","ushort","upsample",clstub)
builtin_rv_v_v("int2","short2","ushort2","upsample",clstub)
builtin_rv_v_v("uint2","ushort2","ushort2","upsample",clstub)
builtin_rv_v_v("int3","short3","ushort3","upsample",clstub)
builtin_rv_v_v("uint3","ushort3","ushort3","upsample",clstub)
builtin_rv_v_v("int4","short4","ushort4","upsample",clstub)
builtin_rv_v_v("uint4","ushort4","ushort4","upsample",clstub)
builtin_rv_v_v("int8","short8","ushort8","upsample",clstub)
builtin_rv_v_v("uint8","ushort8","ushort8","upsample",clstub)
builtin_rv_v_v("int16","short16","ushort16","upsample",clstub)
builtin_rv_v_v("uint16","ushort16","ushort16","upsample",clstub)
builtin_rv_v_v("long","int","uint","upsample",clstub)
builtin_rv_v_v("ulong","uint","uint","upsample",clstub)
builtin_rv_v_v("long2","int2","uint2","upsample",clstub)
builtin_rv_v_v("ulong2","uint2","uint2","upsample",clstub)
builtin_rv_v_v("long3","int3","uint3","upsample",clstub)
builtin_rv_v_v("ulong3","uint3","uint3","upsample",clstub)
builtin_rv_v_v("long4","int4","uint4","upsample",clstub)
builtin_rv_v_v("ulong4","uint4","uint4","upsample",clstub)
builtin_rv_v_v("long8","int8","uint8","upsample",clstub)
builtin_rv_v_v("ulong8","uint8","uint8","upsample",clstub)
builtin_rv_v_v("long16","int16","uint16","upsample",clstub)
builtin_rv_v_v("ulong16","uint16","uint16","upsample",clstub)

builtin_integer_rgentype_gentype("popcount",clstub)

#Fast Integer operations
#Table 6.11                                                                            \n\
#int, int{2|3|4|8|16}                                                                  \n\
#uint, uint{2|3|4|8|16}                                                                \n\
builtin_fastinteger_rgentype_gentype_gentype_gentype("mad24",clstub)
builtin_fastinteger_rgentype_gentype_gentype("mul24",clstub)

#Common Functions
#OpenCL 1.2 Section 6.12.4 
#Table 6.12
#"gentype" functions can take                                                          \n\
#float, float{2|3|4|8|16}                                                              \n\
#double,double{2|3|4|8|16}                                                             \n\
#"gentypef" functions can take                                                         \n\
#float, float{2|3|4|8|16}                                                              \n\
#"gentyped" functions can take                                                         \n\
#float, float{2|3|4|8|16}                                                              \n\
builtin_common_rgentype_gentype_gentype_gentype("clamp",clstub)
builtin_common_rgentypef_gentypef_type_type("float","float","clamp",clstub)
#builtin_common_rgentyped_gentyped_type_type("float","float","clamp",clstub)
builtin_common_rgentype_gentype("degrees",clstub)
builtin_common_rgentype_gentype_gentype("max",clstub)
builtin_common_rgentypef_gentypef_type("float","max",clstub)
#builtin_common_rgentyped_gentyped_type("double","max",clstub)
builtin_common_rgentype_gentype_gentype("min",clstub)
builtin_common_rgentypef_gentypef_type("float","min",clstub)
#builtin_common_rgentyped_gentyped_type("double","min",clstub)
builtin_common_rgentype_gentype_gentype_gentype("mix",clstub)
builtin_common_rgentypef_gentypef_gentypef_type("float","mix",clstub)
#builtin_common_rgentyped_gentyped_gentyped_type("double","mix",clstub)
builtin_common_rgentype_gentype("radians",clstub)
builtin_common_rgentype_gentype_gentype("step",clstub)
builtin_common_rgentypef_type_gentypef("float","step",clstub)
#builtin_common_rgentyped_type_gentyped("double","step",clstub)
builtin_common_rgentype_gentype_gentype_gentype("smoothstep",clstub)
builtin_common_rgentypef_type_type_gentypef("float","float","smoothstep",clstub)
#builtin_common_rgentyped_type_type_gentyped("double","double","smoothstep",clstub)
builtin_common_rgentype_gentype("sign",clstub)


#Geometric Functions
#OpenCL 1.2 Section 6.12.5 
#Table 6.13
#"gentypef" functions can take                                                         
#float, float{2|3|4|8|16}                                                              
#"gentyped" functions can take                                                         
#float, float{2|3|4|8|16}                                                              
outstr.append( "//OpenCL 1.2 Section 6.12.5 Geometric Functions\n")
builtin_rv_v_v("float4","float4","float4","cross",clstub)
builtin_rv_v_v("float3","float3","float3","cross",clstub)
#builtin_rv_v_v("double4","double4","double4","cross",clstub)
#builtin_rv_v_v("double3","double3","double3","cross",clstub)
builtin_geometric_rtype_gentypef_gentypef("float","dot",clstub)
#builtin_geometric_rtype_gentyped_gentyped("double","dot",clstub)
builtin_geometric_rtype_gentypef_gentypef("float","distance",clstub)
#builtin_geometric_rtype_gentyped_gentyped("double","distance",clstub)
builtin_geometric_rtype_gentypef("float","length",clstub)
#builtin_geometric_rtype_gentyped("double","length",clstub)
builtin_rgentypef_gentypef("normalize",clstub)
#builtin_rgentyped_gentyped("normalize",clstub)
builtin_geometric_rtype_gentypef_gentypef("float","fast_distance",clstub)
#builtin_geometric_rtype_gentyped_gentyped("double","fast_distance",clstub)
builtin_geometric_rtype_gentypef("float","fast_length",clstub)
builtin_rgentypef_gentypef("fast_normalize",clstub)

#Relational Functions
#OpenCL 1.2 Section 6.12.6
#Table 6.13
for op in ["isfinite","isinf","isnan","isnormal","signbit"] :
  builtin_relational_rintn_floatn(op,clstub)
#  builtin_relational_rlong_doublen(op,clstub)
for op in ["isequal","isnotequal","isgreater","isgreaterequal","isless","islessequal","islessgreater","isordered","isunordered"] :
  builtin_relational_rintn_floatn_floatn(op,clstub)
#  builtin_relational_rlong_doublen_rdoublen(op,clstub)
for op in ["any","all"] :
  builtin_relational_rint_igentype(op,clstub)
builtin_relational_rgentype_gentype_gentype_gentype("bitselect",clstub)
#special rules for select
builtin_relational_select_gentype_gentype_igentype("select",clstub)
builtin_relational_select_gentype_gentype_ugentype("select",clstub)






#Vector Data Load and Store Functions                       
#OpenCL 1.2 Section 6.12.7 
#doubles unsupported 
builtin_vload_rgentypen_sizet_gentypeconstglobalp("vload",clstub);
builtin_vload_rgentypen_sizet_gentypeconstlocalp("vload",clstub);
builtin_vload_rgentypen_sizet_gentypeconstconstantp("vload",clstub);
builtin_vload_rgentypen_sizet_gentypeconstprivatep("vload",clstub);
builtin_vstore_rvoid_gentypen_sizet_gentypeglobalp("vstore",clstub);
builtin_vstore_rvoid_gentypen_sizet_gentypelocalp("vstore",clstub);
builtin_vstore_rvoid_gentypen_sizet_gentypeprivatep("vstore",clstub);

#vload{n}_half, vstore{n}{rouding}_half
def builtin_vload_half_singlebuiltin(space, vectorwidth):
  outstr.append("float" + vectorwidth + " __attribute__ ((always_inline)) __attribute__((overloadable)) vload_half" + vectorwidth + "(size_t offset, const " + space + " half *p);")
  outstr.append("float" + vectorwidth + " __attribute__ ((always_inline)) __attribute__((overloadable)) vloada_half" + vectorwidth + "(size_t offset, const " + space + " half *p);")

def builtin_vload_half_type(space):
  builtin_vload_half_singlebuiltin(space, "")
  builtin_vload_half_singlebuiltin(space, "2")
  builtin_vload_half_singlebuiltin(space, "3")
  builtin_vload_half_singlebuiltin(space, "4")
  builtin_vload_half_singlebuiltin(space, "8")
  builtin_vload_half_singlebuiltin(space, "16")

builtin_vload_half_type("__global")
builtin_vload_half_type("__local")
builtin_vload_half_type("__private")
builtin_vload_half_type("__constant")

def builtin_vstore_half_singlebuiltin(space, vectorwidth, rounding):
  outstr.append("void __attribute__ ((always_inline)) __attribute__((overloadable)) vstore_half" + vectorwidth + rounding + "(float" + vectorwidth + " data, size_t offset, " + space + " half *p);")
  outstr.append("void __attribute__ ((always_inline)) __attribute__((overloadable)) vstorea_half" + vectorwidth + rounding + "(float" + vectorwidth + " data, size_t offset, " + space + " half *p);")

def builtin_vstore_half_round(space, vectorwidth):
  builtin_vstore_half_singlebuiltin(space, vectorwidth, "")
  builtin_vstore_half_singlebuiltin(space, vectorwidth, "_rte")
  builtin_vstore_half_singlebuiltin(space, vectorwidth, "_rtz")
  builtin_vstore_half_singlebuiltin(space, vectorwidth, "_rtp")
  builtin_vstore_half_singlebuiltin(space, vectorwidth, "_rtn")

def builtin_vstore_half_type(space):
  builtin_vstore_half_round(space, "")
  builtin_vstore_half_round(space, "2")
  builtin_vstore_half_round(space, "3")
  builtin_vstore_half_round(space, "4")
  builtin_vstore_half_round(space, "8")
  builtin_vstore_half_round(space, "16")

builtin_vstore_half_type("__global")
builtin_vstore_half_type("__local")
builtin_vstore_half_type("__private")

#half operations removed until supported in frontend
#outstr.append("float vload_half(size_t f, const __global half *g);");
#outstr.append("float vload_half(size_t f, const __local half *g);");
#outstr.append("float vload_half(size_t f, const __constant half *g);");
#outstr.append("float vload_half(size_t f, const __private half *g);");
#builtin_vload_half_rgentypen_sizet_constglobalhalfp("vload_half",clstub);
#builtin_vload_half_rgentypen_sizet_constlocalhalfp("vload_half",clstub);
#builtin_vload_half_rgentypen_sizet_constconstanthalfp("vload_half",clstub);
#builtin_vload_half_rgentypen_sizet_constprivatehalfp("vload_half",clstub);
#outstr.append("void vstore_half(float f,size_t g,__global half *i);");
#outstr.append("void vstore_half_rte(float f,size_t g,__global half *i);");
#outstr.append("void vstore_half_rtz(float f,size_t g,__global half *i);");
#outstr.append("void vstore_half_rtp(float f,size_t g,__global half *i);");
#outstr.append("void vstore_half_rtn(float f,size_t g,__global half *i);");
#outstr.append("void vstore_half(float f,size_t g,__local half *i);");
#outstr.append("void vstore_half_rte(float f,size_t g,__local half *i);");
#outstr.append("void vstore_half_rtz(float f,size_t g,__local half *i);");
#outstr.append("void vstore_half_rtp(float f,size_t g,__local half *i);");
#outstr.append("void vstore_half_rtn(float f,size_t g,__local half *i);");
#outstr.append("void vstore_half(float f,size_t g,__private half *i);");
#outstr.append("void vstore_half_rte(float f,size_t g,__private half *i);");
#outstr.append("void vstore_half_rtz(float f,size_t g,__private half *i);");
#outstr.append("void vstore_half_rtp(float f,size_t g,__private half *i);");
#outstr.append("void vstore_half_rtn(float f,size_t g,__private half *i);");
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstore_half","",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstore_half","_rte",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstore_half","_rtz",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstore_half","_rtp",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstore_half","_rtn",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstore_half","",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstore_half","_rte",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstore_half","_rtz",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstore_half","_rtp",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstore_half","_rtn",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstore_half","",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstore_half","_rte",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstore_half","_rtz",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstore_half","_rtp",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstore_half","_rtn",clstub);
#vloada vstorea 
#builtin_vload_half_rgentypen_sizet_constglobalhalfp("vloada_half",clstub);
#builtin_vload_half_rgentypen_sizet_constlocalhalfp("vloada_half",clstub);
#builtin_vload_half_rgentypen_sizet_constconstanthalfp("vloada_half",clstub);
#builtin_vload_half_rgentypen_sizet_constprivatehalfp("vloada_half",clstub);
#outstr.append("void vstorea_half(float f,size_t g,__global half *i);");
#outstr.append("void vstorea_half_rte(float f,size_t g,__global half *i);");
#outstr.append("void vstorea_half_rtz(float f,size_t g,__global half *i);");
#outstr.append("void vstorea_half_rtp(float f,size_t g,__global half *i);");
#outstr.append("void vstorea_half_rtn(float f,size_t g,__global half *i);");
#outstr.append("void vstorea_half(float f,size_t g,__local half *i);");
#outstr.append("void vstorea_half_rte(float f,size_t g,__local half *i);");
#outstr.append("void vstorea_half_rtz(float f,size_t g,__local half *i);");
#outstr.append("void vstorea_half_rtp(float f,size_t g,__local half *i);");
#outstr.append("void vstorea_half_rtn(float f,size_t g,__local half *i);");
#outstr.append("void vstorea_half(float f,size_t g,__private half *i);");
#outstr.append("void vstorea_half_rte(float f,size_t g,__private half *i);");
#outstr.append("void vstorea_half_rtz(float f,size_t g,__private half *i);");
#outstr.append("void vstorea_half_rtp(float f,size_t g,__private half *i);");
#outstr.append("void vstorea_half_rtn(float f,size_t g,__private half *i);");
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstorea_half","",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstorea_half","_rte",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstorea_half","_rtz",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstorea_half","_rtp",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeglobalp("vstorea_half","_rtn",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstorea_half","",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstorea_half","_rte",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstorea_half","_rtz",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstorea_half","_rtp",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypelocalp("vstorea_half","_rtn",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstorea_half","",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstorea_half","_rte",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstorea_half","_rtz",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstorea_half","_rtp",clstub);
#builtin_vstore_half_rvoid_gentypen_sizet_gentypeprivatep("vstorea_half","_rtn",clstub);

#Synchronization Functions
#OpenCL 1.2 Section 6.12.8
outstr.append("void barrier(uint i);")

#Explicit Memory Fence Functions
#OpenCL 1.2 Section 6.12.9


#Async Copy from Global to Local Memory, Local to Global Memory and Prefetch
#OpenCL 1.2 Section 6.12.10 
builtin_async_reventt_gentypelocalp_gentypeconstglobalp_sizet_eventt("async_work_group_copy",clstub)
builtin_async_reventt_gentypeglobalp_gentypeconstlocalp_sizet_eventt("async_work_group_copy",clstub)
builtin_async_reventt_gentypelocalp_gentypeconstglobalp_sizet_sizet_eventt("async_work_group_strided_copy",clstub)
builtin_async_reventt_gentypeglobalp_gentypeconstlocalp_sizet_sizet_eventt("async_work_group_strided_copy",clstub)
outstr.append("void wait_group_events(int i,event_t *evt);")
builtin_prefetch_rvoid_constglobalp_sizet("prefetch",clstub)




#builtin_rv_v_v("void","int","event_t","wait_group_events",clstub)
#builtin_async_rvoid_gentypeglobalp_sizet("prefetch",clstub)

# OpenCL 2.0 Pipes
outstr.append("#define read_pipe __opencl_read_pipe")
outstr.append("#define write_pipe __opencl_write_pipe")
outstr.append("#define reserve_read_pipe __opencl_reserve_read_pipe")
outstr.append("#define reserve_write_pipe __opencl_reserve_write_pipe")
outstr.append("#define commit_read_pipe __opencl_commit_read_pipe")
outstr.append("#define commit_write_pipe __opencl_commit_write_pipe")
outstr.append("#define get_pipe_max_packets __opencl_get_pipe_max_packets")
outstr.append("#define get_pipe_num_packets __opencl_get_pipe_num_packets")
outstr.append("#define work_group_reserve_read_pipe __opencl_work_group_reserve_read_pipe")
outstr.append("#define work_group_reserve_write_pipe __opencl_work_group_reserve_write_pipe")
outstr.append("#define work_group_commit_read_pipe __opencl_work_group_commit_read_pipe")
outstr.append("#define work_group_commit_write_pipe __opencl_work_group_commit_write_pipe")
outstr.append("_Bool __attribute__ ((always_inline)) is_valid_reserve_id(reserve_id_t id);")

outstr.append("#endif // _CLC_H_");

#O(n^2) remove duplicates

nodups=[""]

removedlines = 0;
for i in range(1,len(outstr)):
  foundpriormatch=0
  for j in range(1,i):
    if(outstr[i]==outstr[j]):
      foundpriormatch=1;
  if(foundpriormatch==0):
    nodups.append(outstr[i])
  else:
    removedlines=removedlines+1 

for item in nodups:
  print item

sys.exit(0);

#TODO BELOW
#
#
#
#
#
#
#
#
#
#

outstr.append("                                                   \n\
//OpenCL 1.2 Section 6.12.6 Relational Functions                                        \n\
                                                                                        \n\
//OpenCL 1.2 Section 6.12.7 Vector Data Load and Store Functions                        \n\
//Table 6.15                                                                            \n\
//gentype functions can take                                                            \n\
//char uchar short ushort int uint long ulong float double                              \n\
//gentypen represents n-element vectors of gentype                                      \n\
//halfn represents n-element vectors of half elements                                   \n\
//where n=2,3,4,8,16                                                                    \n\
                                                                                        \n\
//vload                                                                                 \n\
//gentype vload (size_t offset, const __global gentype *p,clstub)                              \n\
")


def builtin_vload_singlebuiltin(gentype, space, vectorwidth,  name,clstub) :
 if clstub == 1:
   term= "{return 0;}"
 else:
   term= ";"
 outstr.append( gentype + vectorwidth + " __attribute__ ((always_inline)) __attribute__((overloadable)) " + name + vectorwidth + "(size_t offset,const " + space +  " " + gentype + " *p)" + term)

def builtin_vload_type(gentype,space,name,clstub) :
  builtin_vload_singlebuiltin(gentype,space,"2",name,clstub)
  builtin_vload_singlebuiltin(gentype,space,"3",name,clstub)
  builtin_vload_singlebuiltin(gentype,space,"4",name,clstub)
  builtin_vload_singlebuiltin(gentype,space,"8",name,clstub)
  builtin_vload_singlebuiltin(gentype,space,"16",name,clstub)

builtin_vload_type("char","__global","vload",clstub)
builtin_vload_type("char","__local","vload",clstub)
builtin_vload_type("char","__constant","vload",clstub)
builtin_vload_type("char","__private","vload",clstub)
builtin_vload_type("uchar","__global","vload",clstub)
builtin_vload_type("uchar","__local","vload",clstub)
builtin_vload_type("uchar","__constant","vload",clstub)
builtin_vload_type("uchar","__private","vload",clstub)
builtin_vload_type("short","__global","vload",clstub)
builtin_vload_type("short","__local","vload",clstub)
builtin_vload_type("short","__constant","vload",clstub)
builtin_vload_type("short","__private","vload",clstub)
builtin_vload_type("ushort","__global","vload",clstub)
builtin_vload_type("ushort","__local","vload",clstub)
builtin_vload_type("ushort","__constant","vload",clstub)
builtin_vload_type("ushort","__private","vload",clstub)
builtin_vload_type("int","__global","vload",clstub)
builtin_vload_type("int","__local","vload",clstub)
builtin_vload_type("int","__constant","vload",clstub)
builtin_vload_type("int","__private","vload",clstub)
builtin_vload_type("uint","__global","vload",clstub)
builtin_vload_type("uint","__local","vload",clstub)
builtin_vload_type("uint","__constant","vload",clstub)
builtin_vload_type("uint","__private","vload",clstub)
#builtin_vload_type("long","__global","vload",clstub)
#builtin_vload_type("long","__local","vload",clstub)
#builtin_vload_type("long","__constant","vload",clstub)
#builtin_vload_type("long","__private","vload",clstub)
builtin_vload_type("float","__global","vload",clstub)
builtin_vload_type("float","__local","vload",clstub)
builtin_vload_type("float","__constant","vload",clstub)
builtin_vload_type("float","__private","vload",clstub)
#builtin_vload_type("double","__global","vload",clstub)
#builtin_vload_type("double",__local","vload",clstub)
#builtin_vload_type("double","__private","vload",clstub)
#builtin_vload_type("double","__constant","vload",clstub)

outstr.append( "//vstore                                                                         \n\
//void vstoren (gentypen data, size_t offset, __global gentype *p,clstub)                      ")

def builtin_vstore_singlebuiltin(gentype, space, vectorwidth, builtinmangleend, name,clstub) :
 if clstub == 1:
   term= "{p[0]=0;}"
 else:
   term= ";"
 outstr.append( "void __attribute__ ((always_inline)) __attribute__((overloadable)) " + name + vectorwidth + "(" + gentype + vectorwidth + " data, size_t offset," + space + " " + gentype + "*p)" + term)
 
def builtin_vstore_type(gentype,space,builtinmangleend,name,clstub) :
  builtin_vstore_singlebuiltin(gentype,space,"2",builtinmangleend,name,clstub)
  builtin_vstore_singlebuiltin(gentype,space,"3",builtinmangleend,name,clstub)
  builtin_vstore_singlebuiltin(gentype,space,"4",builtinmangleend,name,clstub)
  builtin_vstore_singlebuiltin(gentype,space,"8",builtinmangleend,name,clstub)
  builtin_vstore_singlebuiltin(gentype,space,"16",builtinmangleend,name,clstub)

builtin_vstore_type("char", "__global", "_i8_sizet_p1_i8", "vstore",clstub)
builtin_vstore_type("char", "__local", "_i8_sizet_p3_i8", "vstore",clstub)
builtin_vstore_type("char", "__private", "_i8_sizet_p0_i8", "vstore",clstub)
builtin_vstore_type("uchar"," __global", "_u8_sizet_p1_u8", "vstore",clstub)
builtin_vstore_type("uchar", "__local", "_u8_sizet_p3_u8", "vstore",clstub)
builtin_vstore_type("uchar", "__private", "_u8_sizet_p0_u8", "vstore",clstub)
builtin_vstore_type("short", "__global", "_i16_sizet_p1_i16", "vstore",clstub)
builtin_vstore_type("short", "__local", "_i16_sizet_p3_i16", "vstore",clstub)
builtin_vstore_type("short", "__private", "_i16_sizet_p0_i16", "vstore",clstub)
builtin_vstore_type("ushort","__global", "_u16_sizet_p1_u16", "vstore",clstub)
builtin_vstore_type("ushort", "__local", "_u16_sizet_p3_u16", "vstore",clstub)
builtin_vstore_type("ushort", "__private", "_u16_sizet_p0_u16", "vstore",clstub)
builtin_vstore_type("int", "__global", "_i32_sizet_p1_i32",  "vstore",clstub)
builtin_vstore_type("int", "__local", "_i32_sizet_p3_i32",  "vstore",clstub)
builtin_vstore_type("int", "__private", "_i32_sizet_p0_i32",  "vstore",clstub)
builtin_vstore_type("uint", "__global", "_u32_sizet_p1_u32",  "vstore",clstub)
builtin_vstore_type("uint", "__local", "_u32_sizet_p3_u32",  "vstore",clstub)
builtin_vstore_type("uint", "__private", "_u32_sizet_p0_u32",  "vstore",clstub)
#builtin_vstore_type("long", "__global", "_i64_sizet_p1_i64", "vstore",clstub)
#builtin_vstore_type("long", "__local", "_i64_sizet_p3_i64", "vstore",clstub)
#builtin_vstore_type("long", "__private", "_i64_sizet_p0_i64", "vstore",clstub)
#builtin_vstore_type("long", "__global", "_u64_sizet_p1_u64", "vstore",clstub)
#builtin_vstore_type("long", "__local", "_u64_sizet_p3_u64", "vstore",clstub)
#builtin_vstore_type("long", "__private", "_u64_sizet_p0_i64", "vstore",clstub)
builtin_vstore_type("float","__global", "_f_sizet_p1_f",  "vstore",clstub)
builtin_vstore_type("float","__local", "_f_sizet_p3_f",  "vstore",clstub)
builtin_vstore_type("float","__private", "_f_sizet_p0_f",  "vstore",clstub)
#builtin_vstore_type("double", "__global", "_d_sizet_p1_d", "vstore",clstub)
#builtin_vstore_type("double", "__local", "_d_sizet_p3_d", "vstore",clstub)
#builtin_vstore_type("double", "__private", "_d_sizet_p0_d", "vstore",clstub)

#//optional
#float vload_half(size_t offset const __global half *p,clstub);
#float vload_half(size_t offset const __local half *p,clstub);
#float vload_half(size_t offset const __constant half *p,clstub);
#float vload_half(size_t offset const __private half *p,clstub);

#outstr.append( "\
#//OpenCL 1.2 Section 6.12.8 Synchnorization Functions                                           \n\
#//SPIR section 2.10.1.1 Synchronization Functions. cl_mem_fence_flags maps to a constant i32    \n\
##define barrier __builtin_spir_barrier_i32                                                      \n\
outstr.append( "void barrier(uint);")

#                                                                                                \n\
#//OpenCL 1.2 Section 6.12.9 Explicit Memory Fence Functions                                     \n\
##define mem_fence __builtin_spir_mem_fence_i32                                                  \n\
##define read_mem_fence __builtin_spir_read_mem_fence_i32                                        \n\
##define write_mem_fence __builtin_spir_write_mem_fence_i32                                      \n\
#                                                                                                \n\
#//OpenCL 1.2 Section 6.12.10 Async Copies from Global to Local Memory,                          \n\
#//Local to Global Memory and Prefetch                                                           \n\
#                                                                                                \n\
#//OpenCL 1.2 Section 6.12.11 Atomic Function                                                    \n\
#                                                                                                \n\
#//OpenCL 1.2 Section 6.12.12 Miscelaneous Vector Function                                       \n\
#                                                                                                \n\
#//OpenCL 1.2 Section 6.12.13 outstr.append(f                                                             \n\
#                                                                                                \n\
#//OpenCL 1.2 Section 6.12.14 Image Read and Write Functions                                     \n\
#                                                                                                \n\
outstr.append("                                                                           \n\
#endif // _CLC_H_                                                                               \n\
")

#create function to use everybuiltin



print outstr


