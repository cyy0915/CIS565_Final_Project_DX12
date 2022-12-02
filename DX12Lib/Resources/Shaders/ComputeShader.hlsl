
#define BLOCK_SIZE 8

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

cbuffer ComputeShaderCB : register(b0)
{
    //对应头文件里的结构
    float4 color;
}

RWTexture2D<float4> OutTexture : register(u0);

// Linear clamp sampler.
SamplerState LinearClampSampler : register(s0);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    //DispatchThreadID.xy是二维索引，对应cuda中的索引，可以用这索引直接访问texture对应像素点float4
    
    OutTexture[IN.DispatchThreadID.xy] = color;
}

