#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/mman.h>
#include "xrt/xrt_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "core/edge/include/sk_types.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
public:
};


__attribute__((visibility("default")))
int hello_world(int *input, int *output, int count, struct xrtHandles *xrtHandle)
{
    memcpy(output, input, count*sizeof(int));

    return 0;
}

#ifdef __cplusplus
}
#endif
