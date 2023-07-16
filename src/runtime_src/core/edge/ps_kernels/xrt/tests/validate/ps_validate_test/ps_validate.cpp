#include "xrt/xrt_kernel.h"

#include <cstring>

#include "experimental/xrt_pscontext.h"

// User private data structure container (context object) definition
class xrtHandles : public xrt::pscontext
{
public:
};

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default")))
int hello_world(int *input, int *output, int count, struct xrtHandles *xrtHandle)
{
    memcpy(output, input, count*sizeof(int));

    return 0;
}

#ifdef __cplusplus
}
#endif
