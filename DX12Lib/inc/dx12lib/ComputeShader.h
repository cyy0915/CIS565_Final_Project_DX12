#pragma once

//����GenerateMipsPSO��д��һ����ʾ�������һ��texture

#include "DescriptorAllocation.h"

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>
#include "CommandList.h"
#include "Texture.h"

namespace dx12lib
{

    class Device;
    class PipelineStateObject;
    class RootSignature;

    //��дΪhlsl��cpp���еĽṹ��ͨ������ṹ��hlsl������������ע����c++����Ҫalignas(16)����16B���룬��Ȼ�ᵼ�������ڴ�ṹ��һ��
    struct alignas(16) ConstantBuffer
    {
        DirectX::XMFLOAT4 color;
    };

    namespace ComputeShaderParm
    {
        //����hlsl��ÿһ�����������, ͨ�����ַ�ʽȷ������������
        enum
        {
            Parm1,
            Result,
            NumRootParameters
        };
    }

    class ComputeShader
    {
    public:
        ComputeShader(std::shared_ptr<Device> device, int w, int h);

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
        void dispatch(std::shared_ptr<CommandList> commandList, DirectX::XMFLOAT4 color);

        std::shared_ptr<Texture> GetResultTexture() const {
            return m_ResultTexture;
        }

    private:
        std::shared_ptr<RootSignature>       m_RootSignature;
        std::shared_ptr<PipelineStateObject> m_PipelineState;
        std::shared_ptr<Texture> m_ResultTexture;
        int m_Width, m_Height;
    };
}  // namespace dx12lib