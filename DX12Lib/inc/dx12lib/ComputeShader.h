#pragma once

//参照GenerateMipsPSO编写，一个简单示例，输出一个texture

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

    //编写为hlsl与cpp共有的结构，通过这个结构向hlsl传少量常量，注意在c++中需要alignas(16)，即16B对齐，不然会导致两边内存结构不一样
    struct alignas(16) ConstantBuffer
    {
        DirectX::XMFLOAT4 color;
    };

    namespace ComputeShaderParm
    {
        //传入hlsl的每一项参数的命名, 通过这种方式确定参数的索引
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

        //进行一次compute shader的计算
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