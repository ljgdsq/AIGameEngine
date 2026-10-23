#include "Aster/Rendering/ImGuiRenderer.h"

#include "Aster/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

#include <filesystem>

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
        io.ConfigWindowsResizeFromEdges = true;

        const std::filesystem::path fontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        if (std::filesystem::exists(fontPath))
        {
            if (io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f) == nullptr)
            {
                io.Fonts->AddFontDefault();
            }
        }
        else
        {
            io.Fonts->AddFontDefault();
        }

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.CellPadding = ImVec2(8.0f, 6.0f);
        style.WindowRounding = 8.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.ScrollbarRounding = 8.0f;
        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;
        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.88f, 0.91f, 0.96f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.51f, 0.60f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.052f, 0.059f, 0.076f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.065f, 0.074f, 0.096f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.070f, 0.080f, 0.104f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.15f, 0.18f, 0.24f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.095f, 0.110f, 0.145f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.145f, 0.175f, 0.225f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.180f, 0.220f, 0.285f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.052f, 0.059f, 0.076f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.080f, 0.095f, 0.125f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.042f, 0.048f, 0.062f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.045f, 0.052f, 0.068f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.175f, 0.205f, 0.260f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.235f, 0.275f, 0.345f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.300f, 0.350f, 0.430f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.29f, 0.64f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.29f, 0.64f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.73f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.105f, 0.130f, 0.170f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.170f, 0.235f, 0.320f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.230f, 0.360f, 0.520f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.145f, 0.235f, 0.360f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.185f, 0.300f, 0.450f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.225f, 0.370f, 0.550f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.150f, 0.180f, 0.235f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.290f, 0.450f, 0.650f, 1.00f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.350f, 0.540f, 0.780f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.26f, 0.34f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.29f, 0.64f, 1.00f, 0.60f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.73f, 1.00f, 0.90f);
        colors[ImGuiCol_Tab] = ImVec4(0.080f, 0.095f, 0.120f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.180f, 0.280f, 0.400f, 1.00f);
        colors[ImGuiCol_TabSelected] = ImVec4(0.145f, 0.235f, 0.360f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.29f, 0.64f, 1.00f, 1.00f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.065f, 0.075f, 0.095f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.110f, 0.145f, 0.200f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.60f, 0.70f, 0.85f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.29f, 0.64f, 1.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.085f, 0.100f, 0.130f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.150f, 0.180f, 0.235f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.100f, 0.120f, 0.155f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.40f, 0.65f, 0.55f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.65f, 0.20f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.29f, 0.64f, 1.00f, 1.00f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02f, 0.025f, 0.035f, 0.65f);


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
        m_Context->RestoreDescriptorHeaps();
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
