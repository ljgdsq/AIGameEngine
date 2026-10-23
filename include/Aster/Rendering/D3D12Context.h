#pragma once

#include "Aster/Core/Base.h"
#include "Aster/Platform/Window.h"

#include <nvrhi/nvrhi.h>

#include <functional>
#include <string>

#ifdef _WIN32
#include <d3d12.h>
#include <dxgiformat.h>
#endif

namespace Aster
{
    class D3D12Context
    {
    public:
        D3D12Context();
        ~D3D12Context();

        D3D12Context(const D3D12Context&) = delete;
        D3D12Context& operator=(const D3D12Context&) = delete;

        [[nodiscard]] bool Initialize(Window& window, std::string* error = nullptr);
        void Shutdown();
        void WaitIdle();

        [[nodiscard]] bool BeginFrame();
        // The optional callback is invoked after scene rendering and before the back buffer is transitioned
        // to Present, so that the UI renderer can record native D3D12 commands on the active command list.
        void EndFrame(const std::function<void(ID3D12GraphicsCommandList*)>& uiCallback = {});

        [[nodiscard]] nvrhi::IDevice* GetDevice() const;
        [[nodiscard]] nvrhi::ICommandList* GetCommandList() const;
        [[nodiscard]] nvrhi::IFramebuffer* GetCurrentFramebuffer() const;
        [[nodiscard]] nvrhi::ITexture* GetCurrentBackbuffer() const;
        [[nodiscard]] nvrhi::ITexture* GetDepthTexture() const;

        [[nodiscard]] ID3D12Device* GetNativeDevice() const;
        [[nodiscard]] ID3D12CommandQueue* GetNativeQueue() const;
        [[nodiscard]] ID3D12GraphicsCommandList* GetNativeCommandList() const;
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;
        [[nodiscard]] DXGI_FORMAT GetBackbufferFormat() const;

        [[nodiscard]] uint32_t GetWidth() const;
        [[nodiscard]] uint32_t GetHeight() const;
        [[nodiscard]] uint32_t GetBackbufferCount() const;
        [[nodiscard]] uint32_t GetCurrentBackbufferIndex() const;
        [[nodiscard]] const std::string& GetAdapterName() const;
        void LogDebugMessages();
        [[nodiscard]] bool CaptureBackbuffer(const std::string& path, std::string* error = nullptr);

    private:
        struct Impl;
        Scope<Impl> m_Impl;
    };
}
