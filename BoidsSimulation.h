#pragma once

#include "DX12Framework.h"
#include "DescriptorHeap.h"
#include "GpuResource.h"
#include "Graphics.h"
#include "SamplerMngr.h"
#include "LinearAllocator.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "CommandContext.h"
#include "Camera.h"

using namespace DirectX;
using namespace Microsoft::WRL;

#include "BoidsSimulation_SharedHeader.inl"

class BoidsSimulation : public Core::IDX12Framework
{
public:
    BoidsSimulation(uint32_t width, uint32_t height);
    ~BoidsSimulation();

    virtual void OnConfiguration();
    virtual void OnInit();
    virtual HRESULT OnCreateResource();
    virtual HRESULT OnSizeChanged();
    virtual void OnUpdate();
    virtual void OnRender(CommandContext& EngineContext);
    virtual void OnDestroy();
    virtual bool OnEvent(MSG* msg);

private:
    HRESULT _LoadAssets();
    HRESULT _LoadSizeDependentResource();
    void _ResetCameraView();

    FishData* _CreateInitialFishData();

    uint32_t _windowWidth;
    uint32_t _windowHeight;

    float _camOrbitRadius = 80.f;
    float _camMaxOribtRadius = 10000.f;
    float _camMinOribtRadius = 1.f;

    SimulationCB _simConstantBuffer;
    RenderCB _renderConstantBuffer;

    // Buffers
    Texture _colorMapTexture;
    StructuredBuffer _boidsPosVelBuffer[2];
    DynAlloc* _constantBufferPtr;

    StructuredBuffer _vertexBuffer;

    GraphicsPSO _graphicsPSO;
    ComputePSO _computePSO;
    RootSignature _rootSignature;

    // App resources.
    OrbitCamera _camera;

    uint8_t _onStageBufferIndex = 0;
    bool _needUpdate = true;
    bool _forcePerFrameSimulation = true;
    bool _simulationPaused = false;
    bool _useSeperatedContext = false;
    float _simulationDelta = 0.015f;
    float _simulationTimer = 0.0f;
    float _simulationMaxDelta = 0.08f;

    LinearAllocator _allocator;
};
