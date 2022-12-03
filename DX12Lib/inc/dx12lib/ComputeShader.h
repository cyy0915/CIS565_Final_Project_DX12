#pragma once

//����GenerateMipsPSO��д��һ����ʾ�������һ��texture

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

    //��дΪhlsl��cpp���еĽṹ��ͨ������ṹ��hlsl������������ע����c++����Ҫalignas(16)����16B���룬��Ȼ�ᵼ�������ڴ�ṹ��һ��
    struct ConstantBuffer
    {
        XMFLOAT3 color;
        XMFLOAT3 color2;
    };

    namespace ComputeShaderParm
    {
        //����hlsl��ÿһ�����������, ͨ�����ַ�ʽȷ������������
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

        //����һ��compute shader�ļ���
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