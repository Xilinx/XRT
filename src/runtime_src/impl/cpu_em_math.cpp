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


#if defined( __i386__ ) || defined( __x86_64__ )
    #include <xmmintrin.h>
#elif defined( __PPC__ ) 
    #include <fpu_control.h>
#endif

extern "C" {

typedef int FPU_mode_type;
extern void ForceFTZ( FPU_mode_type *mode );
extern void RestoreFPState( FPU_mode_type *mode );

  // Restore the reference hardware to floating point state indicated by *mode
void RestoreFPState( FPU_mode_type *mode )
{
#if defined( __i386__ ) || defined( __x86_64__ ) || defined( _MSC_VER ) || defined (__MINGW32__)
  _mm_setcsr( *mode );
#elif defined( __PPC__)
  fpu_control_t flags = 0;
  _FPU_GETCW(flags);
  flags &= ~_FPU_MASK_NI;
  _FPU_SETCW(flags);
//XLNX from OpenCL 1.2 conformance suite
#elif defined(__arm__)
  __asm__ volatile ("fmxr fpscr, %0" :: "r"(*mode));
#else
#warning RestoreFPState needs an implementation
#endif
}

void ForceFTZ( FPU_mode_type *mode )
{
#if defined( __i386__ ) || defined( __x86_64__ ) || defined( _MSC_VER ) || defined (__MINGW32__)
  *mode = _mm_getcsr();
  _mm_setcsr( *mode | 0x8040);
#elif defined( __PPC__ ) 
  fpu_control_t flags = 0;
  _FPU_GETCW(flags);
  flags |= _FPU_MASK_NI;
  _FPU_SETCW(flags);
//XLNX from OpenCL 1.2 conformance suite
#elif defined(__arm__)
  unsigned fpscr;
  __asm__ volatile ("fmrx %0, fpscr" : "=r"(fpscr));
  *mode = fpscr;
  __asm__ volatile ("fmxr fpscr, %0" :: "r"(fpscr | (1U << 24)));
#else
  #warning ForceFTZ needs an implentation
#endif
}

}


