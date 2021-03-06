#include "stdafx.h"
#include "BoidsSimulation.h"
#include <ppl.h>

struct FishVertex {
    float Pos3Tex2[5];
};

FishVertex FishMesh[] = {
    {-0.3f, 0.0f, -0.3f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {-0.3f, 0.0f, -0.3f, 0.25f, 0.25f},
    {-1.0f, 1.0f, 0.0f, 0.5f, 0.0f},
    {-0.3f, 0.0f, 0.3f, 0.75f, 0.25},
    {1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
    {-0.3f, 0.0f, 0.3f, 0.75f, 0.25f},
    {-1.0f, -1.0f, 0.0f, 1.0f, 0.5f},
    {-0.3f, 0.0f, -0.3f, 0.75f, 0.75f},
    {1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
};


FishData*
BoidsSimulation::_CreateInitialFishData()
{
    FishData* pFishData = new FishData[_cbSim.uNumInstance];
    if (!pFishData) {
        return nullptr;
    }

    // Cluster radius in meters
    float clusterScale = 0.2f;
    float velFactor = 1.f;

    for (uint32_t i = 0; i < _cbSim.uNumInstance; ++i) {
        pFishData[i].pos.x = (rand() / (float)RAND_MAX * 2 - 1) *
            _cbSim.f3xyzExpand.x * clusterScale +
            _cbSim.f3CenterPos.x;
        pFishData[i].pos.y = (rand() / (float)RAND_MAX * 2 - 1) *
            _cbSim.f3xyzExpand.y * clusterScale +
            _cbSim.f3CenterPos.y;
        pFishData[i].pos.z = (rand() / (float)RAND_MAX * 2 - 1) *
            _cbSim.f3xyzExpand.z * clusterScale +
            _cbSim.f3CenterPos.z;
        pFishData[i].vel.x = (rand() / (float)RAND_MAX * 2 - 1) * velFactor;
        pFishData[i].vel.y = (rand() / (float)RAND_MAX * 2 - 1) * velFactor;
        pFishData[i].vel.z = (rand() / (float)RAND_MAX * 2 - 1) * velFactor;
    };
    return pFishData;
}

BoidsSimulation::BoidsSimulation(uint32_t width, uint32_t height)
    :_allocator(kCpuWritable)
{
    _windowWidth = width;
    _windowHeight = height;

    // Strength of avoidance force
    _cbSim.fAvoidanceFactor = 8.0f;
    // Strength of separation force
    _cbSim.fSeperationFactor = 0.4f;
    // Strength of cohesion force
    _cbSim.fCohesionFactor = 15.f;
    // Strength of alignment force
    _cbSim.fAlignmentFactor = 12.f;
    // Strength of seeking force
    _cbSim.fSeekingFactor = 0.2f;
    // Seeking target position
    _cbSim.f3SeekSourcePos = float3(0.f, 0.f, 0.f);
    // Strength of flee force
    _cbSim.fFleeFactor = 0.f;
    // Flee push force origin
    _cbSim.f3FleeSourcePos = float3(0.f, 0.f, 0.f);
    // Maximum power a fish could offer
    _cbSim.fMaxForce = 200.0f;
    _cbSim.f3CenterPos = float3(0.f, 0.f, 0.f);
    // m/s maximum speed a fish could run
    _cbSim.fMaxSpeed = 20.0f;
    _cbSim.f3xyzExpand = float3(60.f, 30.f, 60.f);
    // m/s minimum speed a fish have to maintain
    _cbSim.fMinSpeed = 2.5f;
    // Neighbor dist threshold
    _cbSim.fVisionDist = 3.5f;
    // Neighbor angle(cos) threshold
    _cbSim.fVisionAngleCos = -0.6f;
    // seconds of simulation interval
    _cbSim.fDeltaT = 0.01f;
    // Number of simulated fish. This has to be multiple of BLOCK_SIZE
    _cbSim.uNumInstance = 40 * BLOCK_SIZE;
    _cbSim.fFishSize = 0.3f;
}

BoidsSimulation::~BoidsSimulation()
{
}

void
BoidsSimulation::_ResetCameraView()
{
    auto center = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    auto radius = _camOrbitRadius;
    auto minRadius = _camMinOribtRadius;
    auto maxRadius = _camMaxOribtRadius;
    auto longAngle = 0.f;
    auto latAngle = DirectX::XM_PIDIV2;
    _camera.View(center, radius, minRadius, maxRadius, longAngle, latAngle);
}


void
BoidsSimulation::OnConfiguration()
{
    Core::g_config.swapChainDesc.BufferCount = 5;
    Core::g_config.swapChainDesc.Width = _windowWidth;
    Core::g_config.swapChainDesc.Height = _windowHeight;
}

void
BoidsSimulation::OnInit()
{
}

HRESULT
BoidsSimulation::OnCreateResource()
{
    HRESULT hr;
    VRET(_LoadSizeDependentResource());
    VRET(_LoadAssets());
    return S_OK;
}

// Load the sample assets.
HRESULT
BoidsSimulation::_LoadAssets()
{
    HRESULT	hr;
    // Create the root signature for both rendering and simulation.
    _rootSignature.Reset(5, 1);
    _rootSignature[0].InitAsConstantBuffer(0);
    _rootSignature[1].InitAsConstantBuffer(1);
    _rootSignature[2].InitAsBufferSRV(0);
    _rootSignature[3].InitAsBufferUAV(0);
    _rootSignature[4].InitAsDescriptorRange(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
    _rootSignature.InitStaticSampler(0, Graphics::g_SamplerLinearWrapDesc);
    _rootSignature.Finalize(L"BoidsSimulation",
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);

    // Create the pipeline state, which includes compiling and loading shaders.
    _graphicsPSO.SetRootSignature(_rootSignature);
    _computePSO.SetRootSignature(_rootSignature);

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> geometryShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> computeShader;

    uint32_t compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    D3D_SHADER_MACRO macro[] = {
        {"__hlsl", "1"},
        {"COMPUTE_SHADER", "0"},
        {"GRAPHICS_SHADER", "1"},
        {nullptr, nullptr}
    };
    VRET(Graphics::CompileShaderFromFile(
        Core::GetAssetFullPath(_T("BoidsSimulation_shader.hlsl")).c_str(),
        macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vsmain", "vs_5_1",
        compileFlags, 0, &vertexShader));
    VRET(Graphics::CompileShaderFromFile(
        Core::GetAssetFullPath(_T("BoidsSimulation_shader.hlsl")).c_str(),
        macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, "gsmain", "gs_5_1",
        compileFlags, 0, &geometryShader));
    VRET(Graphics::CompileShaderFromFile(
        Core::GetAssetFullPath(_T("BoidsSimulation_shader.hlsl")).c_str(),
        macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psmain", "ps_5_1",
        compileFlags, 0, &pixelShader));

    macro[1].Definition = "1";
    macro[2].Definition = "0";
    VRET(Graphics::CompileShaderFromFile(
        Core::GetAssetFullPath(_T("BoidsSimulation_shader.hlsl")).c_str(),
        macro, D3D_COMPILE_STANDARD_FILE_INCLUDE, "csmain", "cs_5_1",
        compileFlags, 0, &computeShader));

    _graphicsPSO.SetVertexShader(
        vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
    _graphicsPSO.SetGeometryShader(
        geometryShader->GetBufferPointer(), geometryShader->GetBufferSize());
    _graphicsPSO.SetPixelShader(
        pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
    _computePSO.SetComputeShader(
        computeShader->GetBufferPointer(), computeShader->GetBufferSize());

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    _graphicsPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
    _graphicsPSO.SetRasterizerState(Graphics::g_RasterizerDefaultCW);
    _graphicsPSO.SetBlendState(Graphics::g_BlendDisable);
    _graphicsPSO.SetDepthStencilState(Graphics::g_DepthStateReadWrite);
    _graphicsPSO.SetSampleMask(UINT_MAX);
    _graphicsPSO.SetPrimitiveTopologyType(
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    DXGI_FORMAT ColorFormat = Graphics::g_SceneColorBuffer.GetFormat();
    DXGI_FORMAT DepthFormat = Graphics::g_SceneDepthBuffer.GetFormat();
    _graphicsPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);

    _graphicsPSO.Finalize();
    _computePSO.Finalize();

    // Create the buffer resource used in rendering and simulation
    FishData* pFishData = _CreateInitialFishData();
    ASSERT(_cbSim.uNumInstance % BLOCK_SIZE == 0);
    _boidsPosVelBuffer[0].Create(L"BoidsPosVolBuffer[0]",
        _cbSim.uNumInstance, sizeof(FishData), (void*)pFishData);
    _boidsPosVelBuffer[1].Create(L"BoidsPosVolBuffer[1]",
        _cbSim.uNumInstance, sizeof(FishData), (void*)pFishData);
    delete pFishData;

    // Define and create vertex buffer for fish
    _vertexBuffer.Create(L"FishVertexBuffer", _countof(FishMesh),
        sizeof(FishVertex), (void*)FishMesh);

    // Create constant buffer
    _constantBuffer.Create(
        L"ConstantBuffer", 1, sizeof(SimulationCB), (void*)&_cbSim);
    _constantBufferPtr = new DynAlloc(
        std::move(_allocator.Allocate(sizeof(SimulationCB))));
    memcpy(_constantBufferPtr->DataPtr,
        &_cbSim, sizeof(SimulationCB));

    // Load color map texture from file
    ASSERT(_colorMapTexture.CreateFromFIle(L"colorMap.dds", true));

    _ResetCameraView();
    return S_OK;
}

// Load size dependent resource
HRESULT
BoidsSimulation::_LoadSizeDependentResource()
{
    uint32_t width = Core::g_config.swapChainDesc.Width;
    uint32_t height = Core::g_config.swapChainDesc.Height;

    float fAspectRatio = width / (FLOAT)height;
    _camera.Projection(XM_PIDIV2 / 2, fAspectRatio);
    return S_OK;
}

// Update frame-based values.
void
BoidsSimulation::OnUpdate()
{
    using namespace ImGui;
    _camera.ProcessInertia();
    static bool showPanel = true;
    bool valueChanged = false;
    if (!Begin("BoidsSimulation", &showPanel)) {
        End();
        return;
    }
    Checkbox("Separate Context", &_useSeperatedContext);
    Checkbox("PerFrame Simulation", &_forcePerFrameSimulation);
    SameLine();
    static char pauseSim[] = "Pause Simulation";
    static char continueSim[] = "Continue Simulation";
    static bool clicked;
    char* buttonTex = _simulationPaused ? continueSim : pauseSim;
    if (clicked = Button(buttonTex)) {
        _simulationPaused = !_simulationPaused;
    }
#define M(x) _cbStaled |= x
    M(SliderFloat("SimulationDelta", &_simDelta, 0.01f, 0.05f));
    Separator();
    M(SliderFloat("FishSize", &_cbSim.fFishSize, 0.1f, 10.f, "%.2f"));
    M(SliderFloat("VisionDist", &_cbSim.fVisionDist, 1.f, 20.f, "%.2f"));
    M(SliderFloat("VisionCos", &_cbSim.fVisionAngleCos, -1.f, 0.5f, "%.2f"));
    M(SliderFloat("AvoidForce", &_cbSim.fAvoidanceFactor, 0.f, 10.f, "%.2f"));
    M(SliderFloat("SeparForce", &_cbSim.fSeperationFactor, 0.f, 10.f, "%.2f"));
    M(SliderFloat("CohForce", &_cbSim.fCohesionFactor, 0.f, 20.f, "%.2f"));
    M(SliderFloat("AligForce", &_cbSim.fAlignmentFactor, 0.f, 20.f, "%.2f"));
    M(SliderFloat("SeekingForce", &_cbSim.fSeekingFactor, 0.f, 5.f, "%.2f"));
    M(SliderFloat("FleeForce", &_cbSim.fFleeFactor, 0.f, 100.f, "%.2f"));
    M(SliderFloat("MaxForce", &_cbSim.fMaxForce, 1.f, 500.f, "%.2f"));
    M(DragFloatRange2("SpeedRange", &_cbSim.fMinSpeed, &_cbSim.fMaxSpeed,
        0.002f, 0.f, 50.f, "Min:%.2f", "Max:%.2f"));
#undef M
    End();
}

// Render the scene.
void
BoidsSimulation::OnRender(CommandContext& EngineContext)
{
    EngineContext.BeginResourceTransition(Graphics::g_SceneColorBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    EngineContext.BeginResourceTransition(Graphics::g_SceneDepthBuffer,
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
    if (_cbStaled) {
        memcpy(_constantBufferPtr->DataPtr,
            &_cbSim, sizeof(SimulationCB));

        EngineContext.CopyBufferRegion(
            _constantBuffer, 0, _constantBufferPtr->Buffer,
            _constantBufferPtr->Offset, sizeof(_cbSim));
        _cbStaled = false;
    }

    float deltaTime = Core::g_deltaTime > _simulationMaxDelta
        ? _simulationMaxDelta
        : (float)Core::g_deltaTime;
    _simulationTimer += deltaTime;
    uint16_t SimulationCnt = (uint16_t)(_simulationTimer / _simDelta);
    _simulationTimer -= SimulationCnt * _simDelta;

    if (_forcePerFrameSimulation) {
        SimulationCnt = 1;
        _cbSim.fDeltaT = deltaTime <= 0
            ? _simDelta : deltaTime;
        _cbStaled = true;
    } else if (_cbSim.fDeltaT != _simDelta) {
        _cbSim.fDeltaT = _simDelta;
        _cbStaled = true;
    }

    wchar_t timerName[32];
    if (_simulationPaused) {
        SimulationCnt = 0;
    }
    for (int i = 0; i < SimulationCnt; ++i) {
        swprintf(timerName, L"Simulation %d", i);
        ComputeContext& cptContext = _useSeperatedContext
            ? ComputeContext::Begin(L"Simulating")
            : EngineContext.GetComputeContext();
        {
            GPU_PROFILE(cptContext, timerName);
            cptContext.SetRootSignature(_rootSignature);
            cptContext.SetPipelineState(_computePSO);
            cptContext.TransitionResource(
                _boidsPosVelBuffer[_onStageBufferIndex],
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cptContext.TransitionResource(
                _boidsPosVelBuffer[1 - _onStageBufferIndex],
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            cptContext.SetConstantBuffer(
                1, _constantBuffer.RootConstantBufferView());
            cptContext.SetBufferSRV(
                2, _boidsPosVelBuffer[_onStageBufferIndex]);
            cptContext.SetBufferUAV(
                3, _boidsPosVelBuffer[1 - _onStageBufferIndex]);
            ASSERT(_cbSim.uNumInstance % BLOCK_SIZE == 0);
            cptContext.Dispatch1D(_cbSim.uNumInstance, BLOCK_SIZE);
            _onStageBufferIndex = 1 - _onStageBufferIndex;
            cptContext.BeginResourceTransition(
                _boidsPosVelBuffer[1 - _onStageBufferIndex],
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            cptContext.BeginResourceTransition(
                _boidsPosVelBuffer[_onStageBufferIndex],
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        if (_useSeperatedContext) {
            cptContext.Finish();
        }
    }

    // Record all commands we need to render the scene into the command list.
    XMMATRIX view = _camera.View();
    XMMATRIX proj = _camera.Projection();

    // [NOTE]: vectors matrix in shader is column major in default, but on CPU 
    // side they are in row major as default. So for saving cpu cycles to do
    // transpose, we change the order of matrix multiplication here. Otherwise, 
    // We should do _renderConstantBuffer.mWorldViewProj = 
    //  XMMatrixTranspose(XMMatricMultiply(proj,view))
    _renderConstantBuffer.mWorldViewProj = XMMatrixMultiply(view, proj);

    GraphicsContext& gfxContext = _useSeperatedContext
        ? GraphicsContext::Begin(L"Rendering")
        : EngineContext.GetGraphicsContext();
    {
        GPU_PROFILE(gfxContext, L"Render");
        gfxContext.TransitionResource(
            _boidsPosVelBuffer[_onStageBufferIndex],
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        gfxContext.TransitionResource(Graphics::g_SceneColorBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        gfxContext.TransitionResource(Graphics::g_SceneDepthBuffer,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        gfxContext.FlushResourceBarriers();
        gfxContext.ClearColor(Graphics::g_SceneColorBuffer);
        gfxContext.ClearDepth(Graphics::g_SceneDepthBuffer);
        gfxContext.SetRootSignature(_rootSignature);
        gfxContext.SetPipelineState(_graphicsPSO);
        gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        gfxContext.SetConstantBuffer(
            1, _constantBuffer.RootConstantBufferView());
        gfxContext.SetDynamicConstantBufferView(
            0, sizeof(RenderCB), (void*)(&_renderConstantBuffer));
        gfxContext.SetBufferSRV(2, _boidsPosVelBuffer[_onStageBufferIndex]);
        gfxContext.SetDynamicDescriptors(4, 0, 1, &_colorMapTexture.GetSRV());
        gfxContext.SetRenderTargets(
            1, &Graphics::g_SceneColorBuffer.GetRTV(),
            Graphics::g_SceneDepthBuffer.GetDSV());
        gfxContext.SetViewport(Graphics::g_DisplayPlaneViewPort);
        gfxContext.SetScisor(Graphics::g_DisplayPlaneScissorRect);
        gfxContext.SetVertexBuffer(0, _vertexBuffer.VertexBufferView());
        ASSERT(_cbSim.uNumInstance % BLOCK_SIZE == 0);
        gfxContext.DrawInstanced(
            _countof(FishMesh), _cbSim.uNumInstance);
        gfxContext.BeginResourceTransition(
            _boidsPosVelBuffer[_onStageBufferIndex],
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        gfxContext.BeginResourceTransition(
            _boidsPosVelBuffer[1 - _onStageBufferIndex],
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    if (_useSeperatedContext) {
        gfxContext.Finish();
    }
}

HRESULT
BoidsSimulation::OnSizeChanged()
{
    HRESULT hr;
    VRET(_LoadSizeDependentResource());
    return S_OK;
}

void
BoidsSimulation::OnDestroy()
{
}

bool
BoidsSimulation::OnEvent(MSG* msg)
{
    switch (msg->message) {
    case WM_MOUSEWHEEL: {
        auto delta = GET_WHEEL_DELTA_WPARAM(msg->wParam);
        _camera.ZoomRadius(-0.1f*delta);
    }
    case WM_POINTERDOWN:
    case WM_POINTERUPDATE:
    case WM_POINTERUP: {
        auto pointerId = GET_POINTERID_WPARAM(msg->wParam);
        POINTER_INFO pointerInfo;
        if (GetPointerInfo(pointerId, &pointerInfo)) {
            if (msg->message == WM_POINTERDOWN) {
                // Compute pointer position in render units
                POINT p = pointerInfo.ptPixelLocation;
                ScreenToClient(Core::g_hwnd, &p);
                RECT clientRect;
                GetClientRect(Core::g_hwnd, &clientRect);
                p.x = p.x * Core::g_config.swapChainDesc.Width /
                    (clientRect.right - clientRect.left);
                p.y = p.y * Core::g_config.swapChainDesc.Height /
                    (clientRect.bottom - clientRect.top);
                // Camera manipulation
                _camera.AddPointer(pointerId);
            }
        }

        // Otherwise send it to the camera controls
        _camera.ProcessPointerFrames(pointerId, &pointerInfo);
        if (msg->message == WM_POINTERUP) {
            _camera.RemovePointer(pointerId);
        }
    }
    }
    return false;
}