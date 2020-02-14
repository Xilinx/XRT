
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"
#include "lop.h"

namespace bfs = boost::filesystem;

// Helper functions for loading the plugin library
namespace {

  static const char* emptyOrValue(const char* cstr)
  {
    return cstr ? cstr : "" ;
  }

  static boost::filesystem::path& dllExt()
  {
#ifdef _WIN32
    static boost::filesystem::path sDllExt(".dll");
#else
    static boost::filesystem::path sDllExt(".so");
#endif
    return sDllExt;
  }

  static bool isDLL(const bfs::path& path)
  {
    return (bfs::exists(path) && bfs::is_regular_file(path) && path.extension() == dllExt()) ;
  }

  static boost::filesystem::path
  dllpath(const boost::filesystem::path& root, const std::string& libnm)
  {
#ifdef _WIN32
    return root / "bin" / (libnm + ".dll");
#else
    return root / "lib" / ("lib" + libnm + ".so");
#endif
  }
} // end anonymous namespace

namespace xdplop {

  // The loading of the function should only happen once.  Since it 
  //  could theoretically be called from two user threads at once, we
  //  use an internal struct constructor that is thread safe to ensure
  //  it only happens once
  void load_xdp_lop()
  {
    struct xdp_lop_once_loader
    {
      xdp_lop_once_loader()
      {
	bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
	if (xrt.empty()) 
	  throw std::runtime_error("XILINX_XRT not set");
	
	bfs::path xrtlib(xrt / "lib");
	if (!bfs::is_directory(xrtlib))
	  throw std::runtime_error("No such directory '"+xrtlib.string()+"'");

	auto libpath = dllpath(xrt, "xdp_lop_plugin");
	if (!isDLL(libpath)) 
	  throw std::runtime_error("Library "+libpath.string()+" not found!");
	
	auto handle = 
	  xrt_core::dlopen(libpath.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
	if (!handle)
	  throw std::runtime_error("Failed to open XDP library '" + libpath.string() + "'\n" + xrt_core::dlerror());
	
	register_lop_functions(handle) ;
      }
    };

    // Thread safe per C++-11
    static xdp_lop_once_loader xdp_lop_loaded ;
  }

  // All of the function pointers that will be dynamically linked from
  //  the XDP Plugin side
  std::function<void (const char*, long long int, unsigned int)> function_start_cb;
  std::function<void (const char*, long long int, unsigned int)> function_end_cb;

  void register_lop_functions(void* handle)
  {
    typedef void (*ftype)(const char*, long long int, unsigned int) ;
    function_start_cb = (ftype)(xrt_core::dlsym(handle, "lop_function_start")) ;
    if (dlerror() != NULL) function_start_cb = nullptr ;    

    function_end_cb = (ftype)(xrt_core::dlsym(handle, "lop_function_end"));
    if (dlerror() != NULL) function_end_cb = nullptr ;
  }


  std::atomic<unsigned int> LOPFunctionCallLogger::m_funcid_global(0) ;

  LOPFunctionCallLogger::LOPFunctionCallLogger(const char* function) :
    LOPFunctionCallLogger(function, 0)
  {    
  }

  LOPFunctionCallLogger::LOPFunctionCallLogger(const char* function, 
					       long long int address) :
    m_name(function), m_address(address)
  {
    // Load the LOP plugin if not already loaded
    static bool s_load_lop = false ;
    if (!s_load_lop)
    {
      s_load_lop = true ;
      if (xrt_core::config::get_lop_profile()) 
	load_xdp_lop() ;
    }

    // Log the stats for this function
    m_funcid = m_funcid_global++ ;
    if (function_start_cb)
      function_start_cb(m_name, m_address, m_funcid) ;
  }

  LOPFunctionCallLogger::~LOPFunctionCallLogger()
  {
    if (function_end_cb)
      function_end_cb(m_name, m_address, m_funcid) ;
  }

} // end namespace xdplop
