#pragma once

#include "DescriptorAllocation.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include "CommandList.h"
#include "Texture.h"
#include "glm/glm.hpp"
#include "StructuredBuffer.h"
#include "sceneStructs.h"

namespace dx12lib
{
    class Device;
    class PipelineStateObject;
    class RootSignature;

    struct alignas(16) CameraDataGPU {
        DirectX::XMVECTOR position;
        DirectX::XMFLOAT2 pixelLength;
        DirectX::XMFLOAT2 resolution;
        DirectX::XMMATRIX cameraToWorld;
        DirectX::XMMATRIX projection;
    };

    namespace RayTraceParm
    {
        enum
        {
            camera,
            SDFParm,
            geomsNum,
            geoms,
            SDFGrids,
            materials,
            gbuffers,
            result,
            NumRootParameters
        };
    }

    namespace RayTraceRegisterT
    {
        enum
        {
            sdf,
            materials,
            geoms,
            gbuffers,
        };
    }
    class RayTrace
    {
    public:
        RayTrace(std::shared_ptr<Device> device, int width, int height);

        std::shared_ptr<RootSignature> GetRootSignature() const
        {
            return m_RootSignature;
        }

        std::shared_ptr<PipelineStateObject> GetPipelineState() const
        {
            return m_PipelineState;
        }

        void dispatch(std::shared_ptr<CommandList> commandList, std::vector<Material1> mats, CameraDataGPU camera, 
            SDF sdfParm, std::vector<Geom> geoms, std::shared_ptr<StructuredBuffer> SDFGrids,
            std::shared_ptr<Texture> normal, std::shared_ptr<Texture> depthMatid);

        std::shared_ptr<Texture> GetResult() const {
            return m_ResultTexture;
        }

    private:
        std::shared_ptr<RootSignature>       m_RootSignature;
        std::shared_ptr<PipelineStateObject> m_PipelineState;
        std::shared_ptr<Texture> m_ResultTexture;
        int m_Width, m_Height;
    };
}  // namespace dx12lib