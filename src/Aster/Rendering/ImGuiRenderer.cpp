#include "Aster/Rendering/ImGuiRenderer.h"

#include "Aster/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

namespace Aster
{
    ImGuiRenderer::ImGuiRenderer() = default;

    ImGuiRenderer::~ImGuiRenderer()
    {
        Shutdown();
    }

    bool ImGuiRenderer::Initialize(Window& window, D3D12Context& context, std::string* error)
    {
        if (m_Initialized)
        {
            return true;
        }

        m_Window = &window;
        m_Context = &context;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.32f, 0.48f, 1.0f);

        if (!ImGui_ImplGlfw_InitForOther(window.GetHandle(), true))
        {
            if (error != nullptr)
            {
                *error = "Failed to initialize the ImGui GLFW backend";
            }
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 64;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(context.GetNativeDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_SrvHeap))))
        {
            if (error != nullptr)
            {
                *error = "Failed to create the ImGui SRV descriptor heap";
            }
            return false;
        }

        m_SrvAllocator.Heap = m_SrvHeap.Get();
        m_SrvAllocator.DescriptorSize =
            context.GetNativeDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_SrvAllocator.Capacity = heapDesc.NumDescriptors;

        ImGui_ImplDX12_InitInfo info{};
        info.Device = context.GetNativeDevice();
        info.CommandQueue = context.GetNativeQueue();
        info.NumFramesInFlight = static_cast<int>(context.GetBackbufferCount());
        info.RTVFormat = context.GetBackbufferFormat();
        info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        info.SrvDescriptorHeap = m_SrvHeap.Get();
        info.SrvDescriptorAllocFn = &ImGuiRenderer::AllocateSrv;
        info.SrvDescriptorFreeFn = &ImGuiRenderer::FreeSrv;
        info.UserData = &m_SrvAllocator;
        if (!ImGui_ImplDX12_Init(&info))
        {
            if (error != nullptr)
            {
                *error = "Failed to initialize the ImGui DX12 backend";
            }
            return false;
        }

        m_RtvDescriptorSize =
            context.GetNativeDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_Initialized = true;
        ASTER_LOG_INFO("Editor", "ImGui %s initialized", IMGUI_VERSION);
        return true;
    }

    void ImGuiRenderer::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }
        if (m_Context != nullptr)
        {
            m_Context->WaitIdle();
        }
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_SrvHeap.Reset();
        m_SrvAllocator = {};
        m_Context = nullptr;
        m_Window = nullptr;
        m_Initialized = false;
    }

    void ImGuiRenderer::BeginFrame()
    {
        if (!m_Initialized)
        {
            return;
        }
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiRenderer::EndFrame(ID3D12GraphicsCommandList* commandList)
    {
        if (!m_Initialized || commandList == nullptr)
        {
            return;
        }

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData == nullptr)
        {
            return;
        }

        ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvHeap.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_Context->GetCurrentRTV();
        commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        ImGui_ImplDX12_RenderDrawData(drawData, commandList);
    }

    void ImGuiRenderer::AllocateSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
    {
        auto* allocator = static_cast<SrvAllocator*>(info->UserData);
        uint32_t index = 0;
        if (!allocator->Free.empty())
        {
            index = allocator->Free.back();
            allocator->Free.pop_back();
        }
        else if (allocator->Next < allocator->Capacity)
        {
            index = allocator->Next++;
        }
        else
        {
            ASTER_LOG_ERROR("Editor", "ImGui SRV descriptor heap is full");
            index = 0;
        }

        cpuHandle->ptr = allocator->Heap->GetCPUDescriptorHandleForHeapStart().ptr +
            static_cast<SIZE_T>(index) * allocator->DescriptorSize;
        gpuHandle->ptr = allocator->Heap->GetGPUDescriptorHandleForHeapStart().ptr +
            static_cast<UINT64>(index) * allocator->DescriptorSize;
    }

    void ImGuiRenderer::FreeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        auto* allocator = static_cast<SrvAllocator*>(info->UserData);
        const SIZE_T start = allocator->Heap->GetCPUDescriptorHandleForHeapStart().ptr;
        if (cpuHandle.ptr < start || allocator->DescriptorSize == 0)
        {
            return;
        }
        const uint32_t index = static_cast<uint32_t>((cpuHandle.ptr - start) / allocator->DescriptorSize);
        if (index < allocator->Capacity)
        {
            allocator->Free.push_back(index);
        }
    }
}
