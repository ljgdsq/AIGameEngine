#pragma once

#include "Aster/Platform/Window.h"
#include "Aster/Rendering/D3D12Context.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

struct ImGui_ImplDX12_InitInfo;

namespace Aster
{
    class ImGuiRenderer
    {
    public:
        ImGuiRenderer();
        ~ImGuiRenderer();

        ImGuiRenderer(const ImGuiRenderer&) = delete;
        ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

        [[nodiscard]] bool Initialize(Window& window, D3D12Context& context, std::string* error = nullptr);
        void Shutdown();
        void BeginFrame();
        void EndFrame(ID3D12GraphicsCommandList* commandList);

        [[nodiscard]] bool IsInitialized() const { return m_Context != nullptr; }

    private:
        struct SrvAllocator
        {
            ID3D12DescriptorHeap* Heap = nullptr;
            uint32_t DescriptorSize = 0;
            uint32_t Capacity = 0;
            uint32_t Next = 0;
            std::vector<uint32_t> Free;
        };

        static void AllocateSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle);
        static void FreeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

        Window* m_Window = nullptr;
        D3D12Context* m_Context = nullptr;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
        SrvAllocator m_SrvAllocator;
        uint32_t m_RtvDescriptorSize = 0;
        bool m_Initialized = false;
    };
}
