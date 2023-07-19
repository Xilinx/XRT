#include "xrt/xrt_kernel.h"

#include <cstring>

#include "core/edge/include/sk_types.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext
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
