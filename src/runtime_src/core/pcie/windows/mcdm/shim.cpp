// Copyright (C) 2022 Xilinx, Inc
// SPDX-License-Identifier: Apache-2.0
#define XCL_DRIVER_DLL_EXPORT

#include "shim.h"
#include "core/include/shim_int.h"
#include "core/include/experimental/xrt-next.h"
#include "core/common/dlfcn.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/system.h"

#include <iostream>
#include <memory>
#include <string>

#include <initguid.h>  // must be included prior to at least dxcore.h
#include <guiddef.h>
#include <d3dkmthk.h>
#include <dxcore.h>
#include <winnt.h>
#include <winrt/base.h>

#pragma warning(disable : 4100 4505)
#pragma comment (lib, "dxcore.lib")

namespace {

void
not_supported(const std::string& str)
{
  try {
    throw xrt_core::error(std::errc::not_supported, str);
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
}

// Poor mans conversion of herror to std exception as execpted by
// core XRT for all errors.
static void
throw_if_error(HRESULT value, const std::string& pre = "")
{
  try {
    winrt::check_hresult(value);
  }
  catch (const winrt::hresult_error& ex) {
    std::string msg{pre};
    if (!msg.empty())
      msg.append(": ");
    std::wstring wstr{ex.message()};
    throw xrt_core::error(ex.code(), msg + std::string{wstr.begin(), wstr.end()});
  }
}

// Convert a LUID to a string using GUID conversion
// sizeof(GUID) == 16 {uint32_t, uint16_t, uint16_t, uchar[8]}
// sizeof(LUID) == 8  {uint32_t, int32_t}
// Don't convert garbage bytes, so copy LUID to first 8 bytes of GUID
static std::string
to_string(const LUID& luid)
{
  wchar_t wstr[64] = {0};
  GUID guid = {0};
  std::memcpy(&guid, &luid, sizeof(LUID));
  int wlen = StringFromGUID2(guid, wstr, 64);
  return {wstr, wstr+wlen};
}

// Manage gdi dll loading and symbol lookup
class gdilib
{
  using dll_guard = std::unique_ptr<void, decltype(&xrt_core::dlclose)>;
  dll_guard dll;

public:
  gdilib(const char* dllnm)
    : dll(xrt_core::dlopen(dllnm, 0), xrt_core::dlclose)
  {}

  template <typename FunctionType>
  FunctionType
  get(const char* symbol)
  {
    if (auto dllsym = xrt_core::dlsym(dll.get(), symbol))
      return static_cast<FunctionType>(dllsym);

    throw xrt_core::error(std::errc::address_not_available, symbol);
  }
};

// Open graphical device interface
// Keep open till static destruction
static gdilib gdi("gdi32.dll");

// dxcore wrapper for adapter and adapter_list
namespace dxwrap {

// Wrap an IDXCoreAdapter for easier property access
class adapter
{
  winrt::com_ptr<IDXCoreAdapter> m_adapter;

  size_t
  get_prop_size(DXCoreAdapterProperty pt) const
  {
    size_t sz;
    throw_if_error(m_adapter->GetPropertySize(pt, &sz));
    return sz;
  }

  template <typename ReturnType>
  ReturnType
  get_prop(DXCoreAdapterProperty pt) const
  {
    ReturnType value;
    throw_if_error(m_adapter->GetProperty(pt, &value));
    return value;
  }

  template <>
  std::string
  get_prop<std::string>(DXCoreAdapterProperty pt) const
  {
    auto sz = get_prop_size(pt);
    std::string value(sz, '0');
    auto data = value.data();
    throw_if_error(m_adapter->GetProperty(pt, sz, data));
    return value;
  }

public:
  adapter(winrt::com_ptr<IDXCoreAdapter> adapter)
    : m_adapter(std::move(adapter))
  {}

  template <DXCoreAdapterProperty pt, typename ReturnType>
  ReturnType
  get_property() const
  {
    return get_prop<ReturnType>(pt);
  }
};

// Manage list of adapters as probed from system
class adapter_list
{
  std::vector<adapter> m_adapters;
public:
  adapter_list(const std::string& match = "")
  {
    probe(match);
  }

  void
  probe(const std::string& match = "")
  {
    m_adapters.clear();
    // You begin DXCore adapter enumeration by creating an adapter factory.
    winrt::com_ptr<IDXCoreAdapterFactory> adapter_factory;
    winrt::check_hresult(::DXCoreCreateAdapterFactory(adapter_factory.put()));

    // From the factory, retrieve a list of all the Direct3D 12 Core Compute adapters.
    winrt::com_ptr<IDXCoreAdapterList> adapter_list;
    GUID attributes[]{ DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE };
    winrt::check_hresult
      (adapter_factory->CreateAdapterList(_countof(attributes), attributes, adapter_list.put()));

    // Filter adapter list to the ones we care about
    auto count = adapter_list->GetAdapterCount();
    for (decltype(count) idx = 0; idx < count; ++idx) {
      winrt::com_ptr<IDXCoreAdapter> com_adapter;
      winrt::check_hresult(adapter_list->GetAdapter(idx, com_adapter.put()));
      adapter adapter{com_adapter};
      if (match.empty() || adapter.get_property<DXCoreAdapterProperty::DriverDescription,std::string>() == match)
	m_adapters.push_back(std::move(adapter));
    }
  }

  size_t
  size() const
  {
    return m_adapters.size();
  }

  bool
  empty() const
  {
    return m_adapters.empty();
  }

  adapter
  operator[] (const size_t idx) const
  {
    return m_adapters.at(idx);
  }
};

} // dxwrap

static dxwrap::adapter_list s_adapters;

// class shim - shim level device class for MCDM
//
// Manages kernel context on device associated with adapter.
// Shim objects are created via xclOpen().
class shim
{
  using handle = D3DKMT_HANDLE;

  // RAII for adapter handle
  struct adapter
  {
    handle m_handle;

    adapter(handle h) : m_handle(h) {}

    ~adapter()
    {
      static auto close_adapter = gdi.get<PFND3DKMT_CLOSEADAPTER>("D3DKMTCloseAdapter");
      D3DKMT_CLOSEADAPTER d3close;
      d3close.hAdapter = m_handle;
      close_adapter(&d3close);
    }

    template<KMTQUERYADAPTERINFOTYPE type, typename DataType>
    void
    query(DataType* data)
    {
      D3DKMT_QUERYADAPTERINFO d3query = {0};
      d3query.hAdapter = m_handle;
      d3query.Type = type;
      d3query.pPrivateDriverData = data;
      d3query.PrivateDriverDataSize = sizeof(DataType);
      static auto query_adapter_info = gdi.get<PFND3DKMT_QUERYADAPTERINFO>("D3DKMTQueryAdapterInfo");
      throw_if_error(query_adapter_info(&d3query), "adapter query failed");
    }

    operator handle() const { return m_handle; }
  };

  // RAII for device handle: merge with adapter guard
  struct device
  {
    handle m_handle;

    device(handle h) : m_handle(h) {}

    ~device()
    {
      static auto destroy_device = gdi.get<PFND3DKMT_DESTROYDEVICE>("D3DKMTDestroyDevice");
      D3DKMT_DESTROYDEVICE d3ddestroy{0};
      d3ddestroy.hDevice = m_handle;
      destroy_device(&d3ddestroy);
    }

    void
    send_escape()
    {

    }

    operator handle() const { return m_handle; }
  };

  handle
  open_adapter(dxwrap::adapter adapter)
  {
    std::cout << "Opening adapter: "
              << adapter.get_property<DXCoreAdapterProperty::DriverDescription, std::string>()
              << "\n";

    static auto open_adapter = gdi.get<PFND3DKMT_OPENADAPTERFROMLUID>("D3DKMTOpenAdapterFromLuid");
    D3DKMT_OPENADAPTERFROMLUID d3open = {0};
    d3open.AdapterLuid = adapter.get_property<DXCoreAdapterProperty::InstanceLuid, LUID>();
    std::cout << "Adapter LUID:" << to_string(d3open.AdapterLuid) << "\n";
    throw_if_error(open_adapter(&d3open), "Open adapter failed");
    return d3open.hAdapter;
  }

  handle
  create_device(handle adapter)
  {
    static auto create_device = gdi.get<PFND3DKMT_CREATEDEVICE>("D3DKMTCreateDevice");
    D3DKMT_CREATEDEVICE d3dcreate{0};
    d3dcreate.hAdapter = adapter;
    throw_if_error(create_device(&d3dcreate));
    return d3dcreate.hDevice;
  }

  adapter m_adapter;
  device m_device;
  std::shared_ptr<xrt_core::device> m_core_device;

public:
  void
  self_test()
  {
    D3DKMT_DRIVER_DESCRIPTION d3desc {0};
    m_adapter.query<KMTQAITYPE_DRIVER_DESCRIPTION>(&d3desc);
    std::wcout << "Driver Description: " << d3desc.DriverDescription << '\n';

    D3DKMT_ADAPTERTYPE d3type {0};
    m_adapter.query<KMTQAITYPE_ADAPTERTYPE>(&d3type);
    std::cout << "Adapter Type: 0x" << std::hex << d3type.Value << '\n';

  }


public:
  shim(unsigned int idx)
    : m_adapter(open_adapter(s_adapters[idx]))
    , m_device(create_device(m_adapter))
    , m_core_device(xrt_core::get_userpf_device(this, idx))
  {
    self_test();
  }

  ~shim()
  {}
};

static shim*
get_shim_object(xclDeviceHandle handle)
{
  // TODO: Do some sanity check
  return reinterpret_cast<shim*>(handle);
}

} // anonymous namespace

unsigned int
xclProbe()
{
  s_adapters.probe();
  return static_cast<unsigned int>(s_adapters.size());
}

xclDeviceHandle
xclOpen(unsigned int adapter_index, const char*, xclVerbosityLevel)
{
  if (s_adapters.empty())
    s_adapters.probe();

  return new shim(adapter_index);
}

void
xclClose(xclDeviceHandle handle)
{
  auto shim = get_shim_object(handle);
  delete shim;
}

// XRT Buffer Management APIs
xclBufferHandle
xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned int flags)
{
  not_supported(__func__);
  return XRT_NULL_BO;
}

xclBufferHandle
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned int flags)
{
  not_supported(__func__);
  return XRT_NULL_BO;
}

void*
xclMapBO(xclDeviceHandle handle, xclBufferHandle boHandle, bool write)
{
  not_supported(__func__);
  return nullptr;
}

int
xclUnmapBO(xclDeviceHandle handle, xclBufferHandle boHandle, void* addr)
{
  not_supported(__func__);
  return -1;
}

void
xclFreeBO(xclDeviceHandle handle, xclBufferHandle boHandle)
{
  not_supported(__func__);
}

int
xclSyncBO(xclDeviceHandle handle, xclBufferHandle boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  not_supported(__func__);
  return -1;
}

int
xclCopyBO(xclDeviceHandle handle, xclBufferHandle dstBoHandle,
          xclBufferHandle srcBoHandle, size_t size, size_t dst_offset,
          size_t src_offset)
{
  not_supported(__func__);
  return -1;
}

int
xclReClock2(xclDeviceHandle handle, unsigned short region,
            const uint16_t* targetFreqMHz)
{
  not_supported(__func__);
  return -1;
}

// Compute Unit Execution Management APIs
int
xclOpenContext(xclDeviceHandle handle, const xuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  not_supported(__func__);
  return -1;
}

int
xclOpenContextByName(xclDeviceHandle handle, uint32_t slot, const xuid_t xclbinId, const char* cuname, bool shared)
{
  not_supported(__func__);
  return -1;
}

int xclCloseContext(xclDeviceHandle handle, const xuid_t xclbinId, unsigned int ipIndex)
{
  not_supported(__func__);
  return -1;
}

int
xclExecBuf(xclDeviceHandle handle, xclBufferHandle cmdBO)
{
  not_supported(__func__);
  return -1;
}

int
xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  not_supported(__func__);
  return -1;
}

xclBufferExportHandle
xclExportBO(xclDeviceHandle handle, xclBufferHandle boHandle)
{
  not_supported(__func__);
  return XRT_NULL_BO_EXPORT;
}

xclBufferHandle
xclImportBO(xclDeviceHandle handle, xclBufferExportHandle fd, unsigned flags)
{
  not_supported(__func__);
  return XRT_NULL_BO;
}

int
xclCloseExportHandle(xclBufferExportHandle)
{
  not_supported(__func__);
  return -1;
}

int
xclGetBOProperties(xclDeviceHandle handle, xclBufferHandle boHandle,
		   struct xclBOProperties *properties)
{
  not_supported(__func__);
  return -1;
}

int
xclLoadXclBin(xclDeviceHandle handle, const struct axlf *buffer)
{
  not_supported(__func__);
  return -1;
}

ssize_t
xclUnmgdPwrite(xclDeviceHandle handle, unsigned int flags, const void *buf, size_t count, uint64_t offset)
{
  not_supported(__func__);
  return 0;
}

ssize_t
xclUnmgdPread(xclDeviceHandle handle, unsigned int flags, void *buf, size_t count, uint64_t offset)
{
  not_supported(__func__);
  return 0;
}

// Deprecated APIs
size_t
xclWrite(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset, const void *hostbuf, size_t size)
{
  not_supported(__func__);
  return 0;
}

size_t
xclRead(xclDeviceHandle handle, enum xclAddressSpace space,
        uint64_t offset, void *hostbuf, size_t size)
{
  not_supported(__func__);
  return 0;
}

// Restricted read/write on IP register space
int
xclRegWrite(xclDeviceHandle handle, uint32_t ipidx, uint32_t offset, uint32_t data)
{
  not_supported(__func__);
  return -1;
}

int
xclRegRead(xclDeviceHandle handle, uint32_t ipidx, uint32_t offset, uint32_t* datap)
{
  not_supported(__func__);
  return -1;
}

int
xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
  not_supported(__func__);
  return -1;
}

int
xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t force)
{
  not_supported(__func__);
  return -1;
}

int
xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  not_supported(__func__);
  return -1;
}

int
xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
  not_supported(__func__);
  return -1;
}
