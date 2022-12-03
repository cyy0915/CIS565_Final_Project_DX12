#pragma once

//参照GenerateMipsPSO编写，一个简单示例，输出一个texture

#include "DescriptorAllocation.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include "CommandList.h"
#include "Texture.h"
#include "bvhTree.h"
#include "glm/glm.hpp"
#include "StructuredBuffer.h"

namespace dx12lib
{

    class Device;
    class PipelineStateObject;
    class RootSignature;

    //编写为hlsl与cpp共有的结构，通过这个结构向hlsl传少量常量，注意在c++中需要alignas(16)，即16B对齐，不然会导致两边内存结构不一样
    struct ConstantBuffer
    {
        XMFLOAT3 color;
        XMFLOAT3 color2;
    };

    namespace ComputeShaderParm
    {
        //传入hlsl的每一项参数的命名, 通过这种方式确定参数的索引
        enum
        {
            sdf,
            SDFGrids,
            bvhNodes,
            geoms,
            NumRootParameters
        };
    }

    class ComputeShader
    {
    public:
        ComputeShader(std::shared_ptr<Device> device, glm::ivec3 resolution);

        void resize(int w, int h);

        std::shared_ptr<RootSignature> GetRootSignature() const
        {
            return m_RootSignature;
        }

        std::shared_ptr<PipelineStateObject> GetPipelineState() const
        {
            return m_PipelineState;
        }

        //进行一次compute shader的计算
        void dispatch(std::shared_ptr<CommandList> commandList, SDF sdf, BVHTree& bvhTree, std::vector<Geom> geoms);

        std::shared_ptr<Texture> GetResultTexture() const {
            return m_ResultTexture;
        }

    private:
        std::shared_ptr<RootSignature>       m_RootSignature;
        std::shared_ptr<PipelineStateObject> m_PipelineState;
        std::shared_ptr<Texture> m_ResultTexture;
        std::shared_ptr<StructuredBuffer> m_Result;
        glm::ivec3 m_Resolution;
    };
}  // namespace dx12lib