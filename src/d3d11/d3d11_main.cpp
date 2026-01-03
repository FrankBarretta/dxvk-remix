#include <array>

// NV-DXVK start: RTX FileSystem management
#include "../util/util_env.h"
#include "../util/util_filesys.h"
#include "../util/util_once.h"
// NV-DXVK end

#include "../dxgi/dxgi_adapter.h"

#include "../dxvk/dxvk_instance.h"
#include "../dxvk/rtx_render/rtx_options.h"

#include "d3d11_device.h"
#include "d3d11_enums.h"
#include "d3d11_interop.h"

#include <windows.h>

namespace dxvk {
  Logger Logger::s_instance("d3d11.log");

  static void InitD3D11Logging() {
    ONCE(
      Logger::info("InitD3D11Logging called. Setting up RTX FileSys...");
      const auto exePath = env::getExePath();
      const auto exeDir = std::filesystem::path(exePath).parent_path();
      util::RtxFileSys::init(exeDir.string());
      Logger::info("RTX FileSys initialized. Switching to RTX Log...");
      // Logger::initRtxLog();
      util::RtxFileSys::print();
    );
  }
}
  
extern "C" {
  using namespace dxvk;
  
  DLLEXPORT HRESULT __stdcall D3D11CoreCreateDevice(
          IDXGIFactory*       pFactory,
          IDXGIAdapter*       pAdapter,
          UINT                Flags,
    const D3D_FEATURE_LEVEL*  pFeatureLevels,
          UINT                FeatureLevels,
          ID3D11Device**      ppDevice) {
    
    // Direct debug file to verify execution
    {
      std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
      debugFile << "Entry: D3D11CoreCreateDevice" << std::endl;
    }

    Logger::info("Entry: D3D11CoreCreateDevice");
    InitD3D11Logging();
    InitReturnPtr(ppDevice);

    Rc<DxvkAdapter>  dxvkAdapter;
    Rc<DxvkInstance> dxvkInstance;

    Com<IDXGIDXVKAdapter> dxgiVkAdapter;
    
    // Try to find the corresponding Vulkan device for the DXGI adapter
    if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIDXVKAdapter), reinterpret_cast<void**>(&dxgiVkAdapter)))) {
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Found DXVK Adapter" << std::endl;
      }
      dxvkAdapter  = dxgiVkAdapter->GetDXVKAdapter();
      dxvkInstance = dxgiVkAdapter->GetDXVKInstance();
      
      // Initialize RTX Options for d3d11.dll
      static bool s_rtxInitialized = false;
      if (!s_rtxInitialized) {
        s_rtxInitialized = true;
        Logger::info("D3D11: Initializing RTX Options...");
        {
          std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
          debugFile << "Initializing RTX Options..." << std::endl;
        }

        // Add quality.conf layer
        Logger::info("D3D11: Adding quality.conf layer...");
        RtxOptionImpl::addRtxOptionLayer("quality.conf", (uint32_t) RtxOptionLayer::SystemLayerPriority::Quality, true, 1.0f, 0.1f);
        
        // Add user.conf layer
        Logger::info("D3D11: Adding user.conf layer...");
        RtxOptionImpl::addRtxOptionLayer("user.conf", (uint32_t) RtxOptionLayer::SystemLayerPriority::USER, true, 1.0f, 0.1f);
        
        // We need to set startup options!
        Logger::info("D3D11: Setting startup config...");
        RtxOptionManager::setStartupConfig(dxvkInstance->config());
        
        Logger::info("D3D11: Initializing RTX Options (reading configs)...");
        {
          std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
          debugFile << "Calling initializeRtxOptions..." << std::endl;
        }
        RtxOptionManager::initializeRtxOptions();
        
        // Add layers to manager
        Logger::info("D3D11: Adding layers to manager...");
        for (const auto& [unusedLayerKey, optionLayerPtr] : RtxOptionImpl::getRtxOptionLayerMap()) {
          RtxOptionManager::addRtxOptionLayer(*optionLayerPtr);
        }

        // Finally create the RtxOptions instance
        Logger::info("D3D11: Creating RtxOptions instance...");
        {
          std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
          debugFile << "Calling RtxOptions::Create..." << std::endl;
        }
        RtxOptions::Create(dxvkInstance->config());
        
        Logger::info("D3D11: RTX Options initialized.");
        {
          std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
          debugFile << "RTX Options initialized." << std::endl;
        }
      } else {
        {
          std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
          debugFile << "RTX Options already initialized." << std::endl;
        }
      }
    } else {
      Logger::warn("D3D11CoreCreateDevice: Adapter is not a DXVK adapter");
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Adapter is not a DXVK adapter" << std::endl;
      }
      DXGI_ADAPTER_DESC desc;
      pAdapter->GetDesc(&desc);

      dxvkInstance = new DxvkInstance();
      dxvkAdapter  = dxvkInstance->findAdapterByLuid(&desc.AdapterLuid);

      if (dxvkAdapter == nullptr)
        dxvkAdapter = dxvkInstance->findAdapterByDeviceId(desc.VendorId, desc.DeviceId);
      
      if (dxvkAdapter == nullptr)
        dxvkAdapter = dxvkInstance->enumAdapters(0);

      if (dxvkAdapter == nullptr)
        return E_FAIL;
    }
    
    // Feature levels to probe if the
    // application does not specify any.
    std::array<D3D_FEATURE_LEVEL, 6> defaultFeatureLevels = {
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
      D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1,
    };
    
    if (pFeatureLevels == nullptr || FeatureLevels == 0) {
      pFeatureLevels = defaultFeatureLevels.data();
      FeatureLevels  = defaultFeatureLevels.size();
    }
    
    {
      std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
      debugFile << "Probing feature levels..." << std::endl;
    }

    // Find the highest feature level supported by the device.
    // This works because the feature level array is ordered.
    UINT flId;

    for (flId = 0 ; flId < FeatureLevels; flId++) {
      Logger::info(str::format("D3D11CoreCreateDevice: Probing ", pFeatureLevels[flId]));
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Probing " << pFeatureLevels[flId] << std::endl;
      }
      
      bool supported = D3D11Device::CheckFeatureLevelSupport(dxvkInstance, dxvkAdapter, pFeatureLevels[flId]);
      
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Probing result for " << pFeatureLevels[flId] << ": " << (supported ? "Supported" : "Not Supported") << std::endl;
      }

      if (supported)
        break;
    }
    
    {
      std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
      debugFile << "Probing finished. Selected index: " << flId << std::endl;
    }

    if (flId == FeatureLevels) {
      Logger::err("D3D11CoreCreateDevice: Requested feature level not supported");
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Requested feature level not supported" << std::endl;
      }
      return E_INVALIDARG;
    }
    
    // Try to create the device with the given parameters.
    const D3D_FEATURE_LEVEL fl = pFeatureLevels[flId];
    
    try {
      Logger::info(str::format("D3D11CoreCreateDevice: Using feature level ", fl));
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Creating D3D11DXGIDevice with feature level " << fl << std::endl;
      }

      // Static weak pointer to store the last created device
      static DxvkDevice* s_lastDxvkDevice = nullptr;
      Rc<DxvkDevice> sharedDevice = nullptr;

      // Check if we can reuse the device
      if (s_lastDxvkDevice != nullptr) {
         // We can't easily check if it's valid via raw pointer, but we can try to ref it if we had a weak_ptr.
         // However, DxvkDevice is ref counted.
         // Let's assume for this hack that if s_lastDxvkDevice is set, we try to use it.
         // BUT, we need a proper Rc<>.
         // Since we don't have a global Rc<>, we can't safely resurrect it if refcount went to 0.
         // So we should keep a global Rc<> to keep it alive?
         // If we keep it alive, it never gets destroyed. This might be what we want for the game session.
      }
      
      static Rc<DxvkDevice> s_keepAliveDevice = nullptr;
      
      if (s_keepAliveDevice != nullptr) {
          Logger::info("D3D11CoreCreateDevice: Reusing existing DxvkDevice");
          {
            std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
            debugFile << "Reusing existing DxvkDevice" << std::endl;
          }
          sharedDevice = s_keepAliveDevice;
      }

      Com<D3D11DXGIDevice> device = new D3D11DXGIDevice(
        pAdapter, dxvkInstance, dxvkAdapter, fl, Flags, sharedDevice);
      
      if (s_keepAliveDevice == nullptr) {
          s_keepAliveDevice = device->GetDXVKDevice();
          Logger::info("D3D11CoreCreateDevice: Stored DxvkDevice for reuse");
          {
            std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
            debugFile << "Stored DxvkDevice for reuse" << std::endl;
          }
      }

      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "D3D11DXGIDevice created. Querying interface..." << std::endl;
      }

      HRESULT hr = device->QueryInterface(
        __uuidof(ID3D11Device),
        reinterpret_cast<void**>(ppDevice));

      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Exit: D3D11CoreCreateDevice (Result: " << hr << ")" << std::endl;
      }
      
      return hr;
    } catch (const DxvkError& e) {
      Logger::err("D3D11CoreCreateDevice: Failed to create D3D11 device");
      {
        std::ofstream debugFile("d3d11_debug.txt", std::ios::app);
        debugFile << "Failed to create D3D11 device: " << e.message() << std::endl;
      }
      return E_FAIL;
    }
  }
  
  
  static HRESULT D3D11InternalCreateDeviceAndSwapChain(
          IDXGIAdapter*         pAdapter,
          D3D_DRIVER_TYPE       DriverType,
          HMODULE               Software,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          UINT                  SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
          IDXGISwapChain**      ppSwapChain,
          ID3D11Device**        ppDevice,
          D3D_FEATURE_LEVEL*    pFeatureLevel,
          ID3D11DeviceContext** ppImmediateContext) {
    InitD3D11Logging();
    InitReturnPtr(ppDevice);
    InitReturnPtr(ppSwapChain);
    InitReturnPtr(ppImmediateContext);

    if (pFeatureLevel)
      *pFeatureLevel = D3D_FEATURE_LEVEL(0);

    HRESULT hr;

    Com<IDXGIFactory> dxgiFactory = nullptr;
    Com<IDXGIAdapter> dxgiAdapter = pAdapter;
    Com<ID3D11Device> device      = nullptr;
    
    if (ppSwapChain && !pSwapChainDesc)
      return E_INVALIDARG;
    
    if (!pAdapter) {
      // We'll treat everything as hardware, even if the
      // Vulkan device is actually a software device.
      if (DriverType != D3D_DRIVER_TYPE_HARDWARE)
        Logger::warn("D3D11CreateDevice: Unsupported driver type");
      
      // We'll use the first adapter returned by a DXGI factory
      hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory));

      if (FAILED(hr)) {
        Logger::err("D3D11CreateDevice: Failed to create a DXGI factory");
        return hr;
      }

      hr = dxgiFactory->EnumAdapters(0, &dxgiAdapter);

      if (FAILED(hr)) {
        Logger::err("D3D11CreateDevice: No default adapter available");
        return hr;
      }
    } else {
      // We should be able to query the DXGI factory from the adapter
      if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
        Logger::err("D3D11CreateDevice: Failed to query DXGI factory from DXGI adapter");
        return E_INVALIDARG;
      }
      
      // In theory we could ignore these, but the Microsoft docs explicitly
      // state that we need to return E_INVALIDARG in case the arguments are
      // invalid. Both the driver type and software parameter can only be
      // set if the adapter itself is unspecified.
      // See: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476082(v=vs.85).aspx
      if (DriverType != D3D_DRIVER_TYPE_UNKNOWN || Software)
        return E_INVALIDARG;
    }
    
    // Create the actual device
    hr = D3D11CoreCreateDevice(
      dxgiFactory.ptr(), dxgiAdapter.ptr(),
      Flags, pFeatureLevels, FeatureLevels,
      &device);
    
    if (FAILED(hr))
      return hr;
    
    // Create the swap chain, if requested
    if (ppSwapChain) {
      DXGI_SWAP_CHAIN_DESC desc = *pSwapChainDesc;
      hr = dxgiFactory->CreateSwapChain(device.ptr(), &desc, ppSwapChain);

      if (FAILED(hr)) {
        Logger::err("D3D11CreateDevice: Failed to create swap chain");
        return hr;
      }
    }
    
    // Write back whatever info the application requested
    if (pFeatureLevel)
      *pFeatureLevel = device->GetFeatureLevel();
    
    if (ppDevice)
      *ppDevice = device.ref();
    
    if (ppImmediateContext)
      device->GetImmediateContext(ppImmediateContext);

    // If we were unable to write back the device and the
    // swap chain, the application has no way of working
    // with the device so we should report S_FALSE here.
    if (!ppDevice && !ppImmediateContext && !ppSwapChain)
      return S_FALSE;
    
    return S_OK;
  }
  

  DLLEXPORT HRESULT __stdcall D3D11CreateDevice(
          IDXGIAdapter*         pAdapter,
          D3D_DRIVER_TYPE       DriverType,
          HMODULE               Software,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          UINT                  SDKVersion,
          ID3D11Device**        ppDevice,
          D3D_FEATURE_LEVEL*    pFeatureLevel,
          ID3D11DeviceContext** ppImmediateContext) {
    Logger::info("Entry: D3D11CreateDevice");
    return D3D11InternalCreateDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags,
      pFeatureLevels, FeatureLevels, SDKVersion,
      nullptr, nullptr,
      ppDevice, pFeatureLevel, ppImmediateContext);
  }
  
  
  DLLEXPORT HRESULT __stdcall D3D11CreateDeviceAndSwapChain(
          IDXGIAdapter*         pAdapter,
          D3D_DRIVER_TYPE       DriverType,
          HMODULE               Software,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          UINT                  SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
          IDXGISwapChain**      ppSwapChain,
          ID3D11Device**        ppDevice,
          D3D_FEATURE_LEVEL*    pFeatureLevel,
          ID3D11DeviceContext** ppImmediateContext) {
    Logger::info("Entry: D3D11CreateDeviceAndSwapChain");
    return D3D11InternalCreateDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags,
      pFeatureLevels, FeatureLevels, SDKVersion,
      pSwapChainDesc, ppSwapChain,
      ppDevice, pFeatureLevel, ppImmediateContext);
  }
  

  DLLEXPORT HRESULT __stdcall D3D11On12CreateDevice(
          IUnknown*             pDevice,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          IUnknown* const*      ppCommandQueues,
          UINT                  NumQueues,
          UINT                  NodeMask,
          ID3D11Device**        ppDevice,
          ID3D11DeviceContext** ppImmediateContext,
          D3D_FEATURE_LEVEL*    pChosenFeatureLevel) {
    InitD3D11Logging();
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11On12CreateDevice: Not implemented");

    return E_NOTIMPL;
  }

}