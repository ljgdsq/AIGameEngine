#include "Aster/Rendering/D3D12Context.h"

#include "Aster/Core/Log.h"

#include <nvrhi/d3d12.h>

#include <stb_image_write.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace Aster
{
    namespace
    {
        constexpr uint32_t BackbufferCount = 2;

        class NvrhiMessageCallback final : public nvrhi::IMessageCallback
        {
        public:
            void message(nvrhi::MessageSeverity severity, const char* text) override
            {
                switch (severity)
                {
                case nvrhi::MessageSeverity::Warning:
                    ASTER_LOG_WARN("NVRHI", "%s", text);
                    break;
                case nvrhi::MessageSeverity::Error:
                case nvrhi::MessageSeverity::Fatal:
                    ASTER_LOG_ERROR("NVRHI", "%s", text);
                    break;
                default:
                    ASTER_LOG_INFO("NVRHI", "%s", text);
                    break;
                }
            }
        };

        std::string HResultMessage(const char* operation, HRESULT result)
        {
            char buffer[256]{};
            std::snprintf(buffer, sizeof(buffer), "%s failed (HRESULT 0x%08lX)", operation,
                static_cast<unsigned long>(result));
            return buffer;
        }
    }

    struct D3D12Context::Impl
    {
        Window* Window = nullptr;
        ComPtr<IDXGIFactory4> Factory;
        ComPtr<IDXGIAdapter1> Adapter;
        ComPtr<ID3D12Device> NativeDevice;
        ComPtr<ID3D12InfoQueue> InfoQueue;
        ComPtr<ID3D12CommandQueue> NativeQueue;
        ComPtr<IDXGISwapChain3> SwapChain;
        ComPtr<ID3D12DescriptorHeap> RtvHeap;
        std::array<ComPtr<ID3D12Resource>, BackbufferCount> BackbufferResources;
        std::array<nvrhi::TextureHandle, BackbufferCount> BackbufferTextures;
        std::array<nvrhi::FramebufferHandle, BackbufferCount> Framebuffers;
        nvrhi::TextureHandle DepthTexture;
        nvrhi::DeviceHandle Device;
        nvrhi::CommandListHandle CommandList;
        NvrhiMessageCallback MessageCallback;
        std::string AdapterName;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t CurrentBackbuffer = 0;
        bool Initialized = false;
        bool DeviceRemoved = false;

        void ReleaseBackbuffers()
        {
            for (uint32_t index = 0; index < BackbufferCount; ++index)
            {
                Framebuffers[index] = nullptr;
                BackbufferTextures[index] = nullptr;
                BackbufferResources[index].Reset();
            }
            DepthTexture = nullptr;
        }

        [[nodiscard]] bool CreateBackbuffers(std::string* error)
        {
            ReleaseBackbuffers();

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = BackbufferCount;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            HRESULT result = NativeDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&RtvHeap));
            if (FAILED(result))
            {
                if (error != nullptr)
                {
                    *error = HResultMessage("CreateDescriptorHeap(RTV)", result);
                }
                return false;
            }

            const uint32_t descriptorSize =
                NativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RtvHeap->GetCPUDescriptorHandleForHeapStart();

            nvrhi::TextureDesc depthDesc;
            depthDesc.setWidth(Width)
                .setHeight(Height)
                .setFormat(nvrhi::Format::D32)
                .setIsRenderTarget(true)
                .setInitialState(nvrhi::ResourceStates::DepthWrite)
                .setKeepInitialState(true)
                .setDebugName("Aster Depth");
            depthDesc.isShaderResource = true;
            depthDesc.clearValue = nvrhi::Color(1.0f, 0.0f, 0.0f, 0.0f);
            depthDesc.useClearValue = true;
            DepthTexture = Device->createTexture(depthDesc);
            if (!DepthTexture)
            {
                if (error != nullptr)
                {
                    *error = "Failed to create the depth texture";
                }
                return false;
            }

            for (uint32_t index = 0; index < BackbufferCount; ++index)
            {
                result = SwapChain->GetBuffer(index, IID_PPV_ARGS(&BackbufferResources[index]));
                if (FAILED(result))
                {
                    if (error != nullptr)
                    {
                        *error = HResultMessage("GetBuffer", result);
                    }
                    return false;
                }

                NativeDevice->CreateRenderTargetView(BackbufferResources[index].Get(), nullptr, rtvHandle);
                rtvHandle.ptr += descriptorSize;

                nvrhi::TextureDesc colorDesc;
                colorDesc.setWidth(Width)
                    .setHeight(Height)
                    .setFormat(nvrhi::Format::RGBA8_UNORM)
                    .setIsRenderTarget(true)
                    .setInitialState(nvrhi::ResourceStates::Present)
                    .setKeepInitialState(true)
                    .setDebugName("Aster Backbuffer");
                colorDesc.isShaderResource = false;
                nvrhi::Object nativeObject(static_cast<void*>(BackbufferResources[index].Get()));
                BackbufferTextures[index] = Device->createHandleForNativeTexture(
                    nvrhi::ObjectTypes::D3D12_Resource, nativeObject, colorDesc);
                if (!BackbufferTextures[index])
                {
                    if (error != nullptr)
                    {
                        *error = "Failed to wrap the swap chain back buffer";
                    }
                    return false;
                }

                nvrhi::FramebufferDesc framebufferDesc;
                framebufferDesc.addColorAttachment(BackbufferTextures[index]);
                framebufferDesc.setDepthAttachment(DepthTexture);
                Framebuffers[index] = Device->createFramebuffer(framebufferDesc);
                if (!Framebuffers[index])
                {
                    if (error != nullptr)
                    {
                        *error = "Failed to create the swap chain framebuffer";
                    }
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool Resize(uint32_t width, uint32_t height, std::string* error)
        {
            if (width == 0 || height == 0 || (width == Width && height == Height))
            {
                return true;
            }

            Device->waitForIdle();
            // A closed nvrhi command list keeps its tracked texture handles alive. ResizeBuffers
            // rejects the call while the swap chain back buffers are still referenced, so drop the
            // command list before releasing the back buffers and recreate it afterwards.
            CommandList = nullptr;
            ReleaseBackbuffers();
            Width = width;
            Height = height;
            const HRESULT result = SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(result))
            {
                if (error != nullptr)
                {
                    *error = HResultMessage("ResizeBuffers", result);
                }
                return false;
            }
            if (!CreateBackbuffers(error))
            {
                return false;
            }
            CommandList = Device->createCommandList();
            if (!CommandList)
            {
                if (error != nullptr)
                {
                    *error = "NVRHI failed to recreate the command list after a resize";
                }
                return false;
            }
            return true;
        }
    };

    D3D12Context::D3D12Context() : m_Impl(CreateScope<Impl>())
    {
    }

    D3D12Context::~D3D12Context()
    {
        Shutdown();
    }

    bool D3D12Context::Initialize(Window& window, std::string* error)
    {
        if (m_Impl->Initialized)
        {
            return true;
        }
        if (window.GetWin32Handle() == nullptr)
        {
            if (error != nullptr)
            {
                *error = "A D3D12 context requires a Win32 window";
            }
            return false;
        }

        m_Impl->Window = &window;

        HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&m_Impl->Factory));
        if (FAILED(result))
        {
            if (error != nullptr)
            {
                *error = HResultMessage("CreateDXGIFactory1", result);
            }
            return false;
        }

        SIZE_T bestMemory = 0;
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT index = 0; m_Impl->Factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index)
        {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
            {
                continue;
            }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device),
                    nullptr)) &&
                desc.DedicatedVideoMemory > bestMemory)
            {
                bestMemory = desc.DedicatedVideoMemory;
                m_Impl->Adapter = adapter;
            }
        }
        if (!m_Impl->Adapter)
        {
            if (error != nullptr)
            {
                *error = "No D3D12-capable adapter was found";
            }
            return false;
        }

        DXGI_ADAPTER_DESC1 adapterDesc{};
        m_Impl->Adapter->GetDesc1(&adapterDesc);
        const std::wstring wideName(adapterDesc.Description);
        const int nameLength = WideCharToMultiByte(CP_UTF8, 0, wideName.c_str(),
            static_cast<int>(wideName.size()), nullptr, 0, nullptr, nullptr);
        m_Impl->AdapterName.resize(nameLength > 0 ? static_cast<size_t>(nameLength) : 0);
        if (nameLength > 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, wideName.c_str(), static_cast<int>(wideName.size()),
                m_Impl->AdapterName.data(), nameLength, nullptr, nullptr);
        }

        result = D3D12CreateDevice(m_Impl->Adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_Impl->NativeDevice));
        if (FAILED(result))
        {
            if (error != nullptr)
            {
                *error = HResultMessage("D3D12CreateDevice", result);
            }
            return false;
        }

        m_Impl->NativeDevice.As(&m_Impl->InfoQueue);

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        result = m_Impl->NativeDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_Impl->NativeQueue));
        if (FAILED(result))
        {
            if (error != nullptr)
            {
                *error = HResultMessage("CreateCommandQueue", result);
            }
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 swapDesc{};
        swapDesc.Width = static_cast<UINT>(window.GetWidth());
        swapDesc.Height = static_cast<UINT>(window.GetHeight());
        swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.Stereo = FALSE;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = BackbufferCount;
        swapDesc.Scaling = DXGI_SCALING_STRETCH;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapDesc.Flags = 0;

        ComPtr<IDXGISwapChain1> swapChain;
        result = m_Impl->Factory->CreateSwapChainForHwnd(m_Impl->NativeQueue.Get(),
            static_cast<HWND>(window.GetWin32Handle()), &swapDesc, nullptr, nullptr, &swapChain);
        if (FAILED(result))
        {
            if (error != nullptr)
            {
                *error = HResultMessage("CreateSwapChainForHwnd", result);
            }
            return false;
        }
        result = swapChain.As(&m_Impl->SwapChain);
        if (FAILED(result))
        {
            if (error != nullptr)
            {
                *error = HResultMessage("QueryInterface(IDXGISwapChain3)", result);
            }
            return false;
        }
        m_Impl->SwapChain->SetMaximumFrameLatency(1);

        nvrhi::d3d12::DeviceDesc deviceDesc{};
        deviceDesc.errorCB = &m_Impl->MessageCallback;
        deviceDesc.pDevice = m_Impl->NativeDevice.Get();
        deviceDesc.pGraphicsCommandQueue = m_Impl->NativeQueue.Get();
        m_Impl->Device = nvrhi::d3d12::createDevice(deviceDesc);
        if (!m_Impl->Device)
        {
            if (error != nullptr)
            {
                *error = "NVRHI failed to create a D3D12 device";
            }
            return false;
        }

        m_Impl->CommandList = m_Impl->Device->createCommandList();
        if (!m_Impl->CommandList)
        {
            if (error != nullptr)
            {
                *error = "NVRHI failed to create a command list";
            }
            return false;
        }

        m_Impl->Width = static_cast<uint32_t>(window.GetWidth());
        m_Impl->Height = static_cast<uint32_t>(window.GetHeight());
        m_Impl->CurrentBackbuffer = m_Impl->SwapChain->GetCurrentBackBufferIndex();
        if (!m_Impl->CreateBackbuffers(error))
        {
            return false;
        }

        m_Impl->Initialized = true;
        ASTER_LOG_INFO("D3D12", "Adapter: %s (%ux%u)", m_Impl->AdapterName.c_str(), m_Impl->Width,
            m_Impl->Height);
        return true;
    }

    void D3D12Context::Shutdown()
    {
        if (!m_Impl || !m_Impl->Initialized)
        {
            return;
        }
        ASTER_LOG_INFO("D3D12", "Shutdown: wait idle");
        m_Impl->Device->waitForIdle();
        ASTER_LOG_INFO("D3D12", "Shutdown: release backbuffers");
        m_Impl->ReleaseBackbuffers();
        ASTER_LOG_INFO("D3D12", "Shutdown: release command list");
        m_Impl->CommandList = nullptr;
        ASTER_LOG_INFO("D3D12", "Shutdown: release nvrhi device");
        m_Impl->Device = nullptr;
        ASTER_LOG_INFO("D3D12", "Shutdown: release swapchain");
        m_Impl->SwapChain.Reset();
        ASTER_LOG_INFO("D3D12", "Shutdown: release rtv heap");
        m_Impl->RtvHeap.Reset();
        ASTER_LOG_INFO("D3D12", "Shutdown: release queue");
        m_Impl->NativeQueue.Reset();
        ASTER_LOG_INFO("D3D12", "Shutdown: release native device");
        m_Impl->NativeDevice.Reset();
        ASTER_LOG_INFO("D3D12", "Shutdown: release adapter");
        m_Impl->Adapter.Reset();
        ASTER_LOG_INFO("D3D12", "Shutdown: release factory");
        m_Impl->Factory.Reset();
        m_Impl->Initialized = false;
        ASTER_LOG_INFO("D3D12", "Context shut down");
    }

    void D3D12Context::WaitIdle()
    {
        if (m_Impl && m_Impl->Device)
        {
            m_Impl->Device->waitForIdle();
        }
    }

    bool D3D12Context::BeginFrame()
    {
        if (!m_Impl->Initialized || m_Impl->DeviceRemoved)
        {
            return false;
        }
        int width = 0;
        int height = 0;
        if (m_Impl->Window != nullptr)
        {
            width = m_Impl->Window->GetWidth();
            height = m_Impl->Window->GetHeight();
        }
        if (width > 0 && height > 0 &&
            (static_cast<uint32_t>(width) != m_Impl->Width || static_cast<uint32_t>(height) != m_Impl->Height))
        {
            std::string error;
            if (!m_Impl->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height), &error))
            {
                ASTER_LOG_ERROR("D3D12", "Resize failed: %s", error.c_str());
                return false;
            }
        }

        m_Impl->CurrentBackbuffer = m_Impl->SwapChain->GetCurrentBackBufferIndex();
        m_Impl->CommandList->open();
        m_Impl->CommandList->beginTrackingTextureState(m_Impl->BackbufferTextures[m_Impl->CurrentBackbuffer],
            nvrhi::TextureSubresourceSet(0, 1, 0, 1), nvrhi::ResourceStates::Present);
        return true;
    }

    void D3D12Context::EndFrame(const std::function<void(ID3D12GraphicsCommandList*)>& uiCallback)
    {
        if (!m_Impl->Initialized)
        {
            return;
        }

        if (uiCallback)
        {
            uiCallback(GetNativeCommandList());
        }

        m_Impl->CommandList->setTextureState(m_Impl->BackbufferTextures[m_Impl->CurrentBackbuffer],
            nvrhi::TextureSubresourceSet(0, 1, 0, 1), nvrhi::ResourceStates::Present);
        m_Impl->CommandList->close();
        nvrhi::ICommandList* commandLists[] = { m_Impl->CommandList.Get() };
        m_Impl->Device->executeCommandLists(commandLists, 1);
        const HRESULT result = m_Impl->SwapChain->Present(1, 0);
        if (FAILED(result))
        {
            const HRESULT removedReason = m_Impl->NativeDevice != nullptr
                ? m_Impl->NativeDevice->GetDeviceRemovedReason()
                : S_OK;
            ASTER_LOG_ERROR("D3D12", "Present failed (HRESULT 0x%08lX, device removed reason 0x%08lX)",
                static_cast<unsigned long>(result), static_cast<unsigned long>(removedReason));
            if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
            {
                m_Impl->DeviceRemoved = true;
                return;
            }
        }
        m_Impl->Device->runGarbageCollection();
    }

    nvrhi::IDevice* D3D12Context::GetDevice() const
    {
        return m_Impl->Device.Get();
    }

    nvrhi::ICommandList* D3D12Context::GetCommandList() const
    {
        return m_Impl->CommandList.Get();
    }

    nvrhi::IFramebuffer* D3D12Context::GetCurrentFramebuffer() const
    {
        return m_Impl->Framebuffers[m_Impl->CurrentBackbuffer].Get();
    }

    nvrhi::ITexture* D3D12Context::GetCurrentBackbuffer() const
    {
        return m_Impl->BackbufferTextures[m_Impl->CurrentBackbuffer].Get();
    }

    nvrhi::ITexture* D3D12Context::GetDepthTexture() const
    {
        return m_Impl->DepthTexture.Get();
    }

    ID3D12Device* D3D12Context::GetNativeDevice() const
    {
        return m_Impl->NativeDevice.Get();
    }

    ID3D12CommandQueue* D3D12Context::GetNativeQueue() const
    {
        return m_Impl->NativeQueue.Get();
    }

    ID3D12GraphicsCommandList* D3D12Context::GetNativeCommandList() const
    {
        if (!m_Impl->CommandList)
        {
            return nullptr;
        }
        return static_cast<ID3D12GraphicsCommandList*>(
            m_Impl->CommandList->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList).pointer);
    }

    void D3D12Context::RestoreDescriptorHeaps()
    {
        if (!m_Impl->Initialized || !m_Impl->CommandList)
        {
            return;
        }
        auto* device = static_cast<nvrhi::d3d12::IDevice*>(m_Impl->Device.Get());
        auto* srvHeap = device->getDescriptorHeap(nvrhi::d3d12::DescriptorHeapType::ShaderResourceView);
        auto* samplerHeap = device->getDescriptorHeap(nvrhi::d3d12::DescriptorHeapType::Sampler);
        if (srvHeap == nullptr || samplerHeap == nullptr)
        {
            return;
        }
        ID3D12GraphicsCommandList* commandList = GetNativeCommandList();
        if (commandList == nullptr)
        {
            return;
        }
        ID3D12DescriptorHeap* heaps[2] = { srvHeap->getShaderVisibleHeap(), samplerHeap->getShaderVisibleHeap() };
        commandList->SetDescriptorHeaps(2, heaps);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12Context::GetCurrentRTV() const    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_Impl->RtvHeap->GetCPUDescriptorHandleForHeapStart();
        const uint32_t descriptorSize =
            m_Impl->NativeDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        handle.ptr += static_cast<SIZE_T>(m_Impl->CurrentBackbuffer) * descriptorSize;
        return handle;
    }

    DXGI_FORMAT D3D12Context::GetBackbufferFormat() const
    {
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    uint32_t D3D12Context::GetWidth() const
    {
        return m_Impl->Width;
    }

    uint32_t D3D12Context::GetHeight() const
    {
        return m_Impl->Height;
    }

    uint32_t D3D12Context::GetBackbufferCount() const
    {
        return BackbufferCount;
    }

    uint32_t D3D12Context::GetCurrentBackbufferIndex() const
    {
        return m_Impl->CurrentBackbuffer;
    }

    const std::string& D3D12Context::GetAdapterName() const
    {
        return m_Impl->AdapterName;
    }

    bool D3D12Context::CaptureBackbuffer(const std::string& path, std::string* error)
    {
        if (!m_Impl || !m_Impl->Initialized)
        {
            if (error != nullptr)
            {
                *error = "D3D12 context is not initialized";
            }
            return false;
        }

        nvrhi::ITexture* texture = m_Impl->BackbufferTextures[m_Impl->CurrentBackbuffer];
        nvrhi::TextureDesc stagingDesc;
        stagingDesc.setWidth(m_Impl->Width)
            .setHeight(m_Impl->Height)
            .setFormat(nvrhi::Format::RGBA8_UNORM)
            .setDimension(nvrhi::TextureDimension::Texture2D)
            .setMipLevels(1)
            .setArraySize(1)
            .setSampleCount(1)
            .setIsRenderTarget(false)
            .setIsUAV(false)
            .setInitialState(nvrhi::ResourceStates::Unknown)
            .setKeepInitialState(false)
            .setDebugName("Aster Screenshot Staging");
        nvrhi::StagingTextureHandle staging = m_Impl->Device->createStagingTexture(stagingDesc,
            nvrhi::CpuAccessMode::Read);
        if (!staging)
        {
            if (error != nullptr)
            {
                *error = "Could not create a screenshot staging texture";
            }
            return false;
        }

        const nvrhi::TextureSubresourceSet subresources(0, 1, 0, 1);
        nvrhi::TextureSlice slice;
        slice.setSize(m_Impl->Width, m_Impl->Height);
        m_Impl->Device->waitForIdle();
        m_Impl->CommandList->open();
        m_Impl->CommandList->setTextureState(texture, subresources, nvrhi::ResourceStates::CopySource);
        m_Impl->CommandList->copyTexture(staging, slice, texture, slice);
        m_Impl->CommandList->setTextureState(texture, subresources, nvrhi::ResourceStates::Present);
        m_Impl->CommandList->close();
        nvrhi::ICommandList* commandLists[] = { m_Impl->CommandList.Get() };
        m_Impl->Device->executeCommandLists(commandLists, 1);
        m_Impl->Device->waitForIdle();

        size_t rowPitch = 0;
        void* pixels = m_Impl->Device->mapStagingTexture(staging, slice, nvrhi::CpuAccessMode::Read, &rowPitch);
        if (pixels == nullptr)
        {
            if (error != nullptr)
            {
                *error = "Could not map the screenshot staging texture";
            }
            return false;
        }

        const int written = stbi_write_png(path.c_str(), static_cast<int>(m_Impl->Width),
            static_cast<int>(m_Impl->Height), 4, pixels, static_cast<int>(rowPitch));
        m_Impl->Device->unmapStagingTexture(staging);
        if (written == 0)
        {
            if (error != nullptr)
            {
                *error = "Could not write screenshot to " + path;
            }
            return false;
        }
        return true;
    }

    void D3D12Context::LogDebugMessages()
    {        if (!m_Impl->InfoQueue)
        {
            return;
        }

        const UINT64 count = m_Impl->InfoQueue->GetNumStoredMessages();
        for (UINT64 index = 0; index < count; ++index)
        {
            SIZE_T length = 0;
            if (FAILED(m_Impl->InfoQueue->GetMessage(index, nullptr, &length)) || length == 0)
            {
                continue;
            }
            std::vector<char> storage(length);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            if (FAILED(m_Impl->InfoQueue->GetMessage(index, message, &length)))
            {
                continue;
            }
            const char* text = message->pDescription != nullptr ? message->pDescription : "(no description)";
            if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
                message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
            {
                ASTER_LOG_ERROR("D3D12Debug", "%s", text);
            }
            else if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING)
            {
                ASTER_LOG_WARN("D3D12Debug", "%s", text);
            }
        }
        m_Impl->InfoQueue->ClearStoredMessages();
    }
}
