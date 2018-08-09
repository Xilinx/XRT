#include "xocl/core/refcount.h"
#include "xrt/device/device.h"


namespace xocl { namespace qdma {

class stream : public refcount
{   
};

class stream_mem : public refcount
{   
};

}} // qdma, xocl

struct _cl_stream : public xocl::qdma::stream {};

//TODO: This should be integrated with  memory.h. Just put it 
//here for compile success now.
struct _cl_stream_mem : public xocl::qdma::stream_mem {};
