#ifndef __OP_MACROS__
#define __OP_MACROS__

#if 0
#ifdef __cplusplus
extern "C" {
#endif

#include <xaiengine.h>

#include "op_base.h"


#define GENERATE_FUNC_SIGNATURE(INSTRNAME) int op_##INSTRNAME##_func(XAie_DevInst*, op_base *);


OP_LIST( GENERATE_FUNC_SIGNATURE )

#ifdef __cplusplus
}
#endif
#endif


#endif
