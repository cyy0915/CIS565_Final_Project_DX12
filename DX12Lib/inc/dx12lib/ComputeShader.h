#pragma once

//����GenerateMipsPSO��д��һ����ʾ�������һ��texture

#include "DescriptorAllocation.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include "CommandList.h"
#include "CommandQueue.h"
#include "Texture.h"
#include "bvhTree.h"
#include "glm/glm.hpp"
#include "StructuredBuffer.h"
#include "Material.h"

namespace dx12lib
{

    class Device;
    class PipelineStateObject;
    class RootSignature;

    //��дΪhlsl��cpp���еĽṹ��ͨ������ṹ��hlsl������������ע����c++����Ҫalignas(16)����16B���룬��Ȼ�ᵼ�������ڴ�ṹ��һ��
    struct alignas(16) MaterialSDF {
        int textureId;
        glm::vec3 color;
    };

    namespace ComputeShaderParm
    {
        //����hlsl��ÿһ�����������, ͨ�����ַ�ʽȷ������������
        enum
        {
            sdf,
            bias,
            SDFGrids,
            bvhNodes,
            mats,
            textures,
            NumRootParameters
        };
    }
    namespace ComputeShaderRegisterT {
        enum {
            bvhNodes,
            mats,
            textures
        };
    }

    class ComputeShader
    {
    public:
        ComputeShader(std::shared_ptr<Device> device, SDF sdfParm, int textureNum);

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
        void dispatch(CommandQueue& commandQueue, BVHTree& bvhTree, std::vector<std::shared_ptr<Material>> mats);

        std::shared_ptr<StructuredBuffer> GetResult() const {
            return m_Result;
        }

        void complete() {
            m_bvhResource.reset();
        }

        SDF m_sdfParm;
        std::shared_ptr<StructuredBuffer> m_bvhResource;

    private:
        std::shared_ptr<RootSignature>       m_RootSignature;
        std::shared_ptr<PipelineStateObject> m_PipelineState;
        std::shared_ptr<Texture> m_ResultTexture;
        std::shared_ptr<StructuredBuffer> m_Result;



        int m_textureNum;
    };
}  // namespace dx12lib