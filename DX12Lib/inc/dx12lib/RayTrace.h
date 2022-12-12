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

    struct RenderParm {
        int iter;
        int change;
        int depth;
        int useSDF;
        //
        glm::vec3 lightDir;
        int screenTracing;
    };

    namespace RayTraceParm
    {
        enum
        {
            camera,
            SDFParm,
            renderParm,
            SDFGrids,
            gbuffers,
            bvh,
            result,
            //RadianceCache param
            RadianceCacheParam,
            NumRootParameters
        };
    }

    namespace RayTraceRegisterT
    {
        enum
        {
            sdf,
            bvh,
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

        void dispatch(std::shared_ptr<CommandList> commandList, CameraDataGPU camera, bool change,
            int depth, bool useSDF, glm::vec3 lightDir, bool screenTracing,
            SDF sdfParm, std::shared_ptr<StructuredBuffer> SDFGrids,
            std::shared_ptr<Texture> normal, std::shared_ptr<Texture> depthMatid, std::shared_ptr<Texture> color, std::shared_ptr<StructuredBuffer> bvh);

        std::shared_ptr<Texture> GetResult() const {
            return m_ResultTexture;
        }

        void resize(int w, int h);
    private:
        std::shared_ptr<RootSignature>       m_RootSignature;
        std::shared_ptr<PipelineStateObject> m_PipelineState;
        std::shared_ptr<Texture> m_ResultTexture;
        // structureBuffer
        std::shared_ptr<StructuredBuffer> m_radianceCacheBuffer;

        int m_Width, m_Height;
        int m_iter = 0;
        bool m_change = true;
    };
}  // namespace dx12lib