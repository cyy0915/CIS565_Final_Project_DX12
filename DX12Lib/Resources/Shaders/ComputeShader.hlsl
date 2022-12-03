
#define BLOCK_SIZE 8

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

struct SDFGrid
{
    float dist;
    int geomId;
};

struct SDF
{
    float4 minCorner;
    float4 maxCorner;
    int4 resolution;
    float4 gridExtent;
};

struct Geom {
    int materialid;
    float3 translation;
        //
    float3 rotation;
    int faceStartIdx; // use with array of Triangle
        //
    float3 scale;
    int faceNum;
        //
    float4x4 transform;
    float4x4 inverseTransform;
    float4x4 invTranspose;
    int type;
};

struct BVHNode {
    float3 minCorner;
    int idx;
        //
    float3 maxCorner;
    int isLeaf;
        //
    float3 point1;
    int hasFace;
        //
    float3 point2;
    int geomId;
        //
    float4 point3;
    float4 normal1;
    float4 normal2;
    float4 normal3;
    float4 triangleMinCorner;
    float4 triangleMaxCorner;
    float4 center;
};

ConstantBuffer<SDF> sdf : register(b0);

StructuredBuffer<BVHNode> nodes : register(s0);

StructuredBuffer<Geom> geoms : register(s1);

//RWTexture2D<float4> OutTexture : register(u0);
RWStructuredBuffer<SDFGrid> sdfGrids : register(u0);

// Linear clamp sampler.
//SamplerState LinearClampSampler : register(s0);

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    //DispatchThreadID.xy是二维索引，对应cuda中的索引，可以用这索引直接访问texture对应像素点float4
    
    int x = IN.DispatchThreadID.x;
    int y = IN.DispatchThreadID.y;
    int z = IN.DispatchThreadID.z;

    int idx = z * sdf.resolution.x * sdf.resolution.y + y * sdf.resolution.x + x;


    float3 voxelPos = float3(sdf.minCorner.x + sdf.gridExtent.x * (float(x) + 0.5f),
                                   sdf.minCorner.y + sdf.gridExtent.y * (float(y) + 0.5f),
                                   sdf.minCorner.z + sdf.gridExtent.z * (float(z) + 0.5f));
    
    // find closest triangle
    int currIdx = 0;

    int idxToVisit[64];
    int toVisitOffset = 0;

    idxToVisit[toVisitOffset++] = 0;

    const Triangle* minTriangle = nullptr;
    float minUdf = FLT_MAX;

    while (toVisitOffset > 0)
    {
        currIdx = idxToVisit[--toVisitOffset];
        //if (currIdx >= bvhNodes_size) continue;

        if (!bvhNodes[currIdx].isLeaf)
        {
            /*float rayLength = boxIntersectionTest(bvhNodes[currIdx].minCorner, bvhNodes[currIdx].maxCorner, r);
            if (rayLength == -1.f || rayLength > minUdf) continue;*/

            float distToBox = udfBox(voxelPos, bvhNodes[currIdx].minCorner, bvhNodes[currIdx].maxCorner);
            if (distToBox > minUdf)
                continue;

            int leftIdx = currIdx * 2 + 1;
            int rightIdx = leftIdx + 1;

            idxToVisit[toVisitOffset++] = rightIdx;
            idxToVisit[toVisitOffset++] = leftIdx;
        }
        else
        {
            if (!bvhNodes[currIdx].hasFace)
                continue;
            const Triangle* face =  & bvhNodes[currIdx].face;
            float t = udfTriangle(voxelPos, face.point1, face.point2, face.point3);

            if (minUdf > t)
            {
                minTriangle =  & bvhNodes[currIdx].face;
                minUdf = t;
            }
        }
    }


    // will crash without this
    if (minTriangle == nullptr)
        return;
    int geomId = minTriangle.geomId;

    float3 triCenter = (minTriangle.point1 + minTriangle.point2 + minTriangle.point3) / 3.f;
    float3 worldNor = glm::normalize(triCenter - voxelPos);
    float3 localNor = multiplyMV(geoms[geomId].inverseTransform, float4(worldNor, 0.f));

    float3 localtriNor = multiplyMV(geoms[geomId].inverseTransform, float4(minTriangle.normal1, 0.f));
    // if inside
    if (glm::dot(localNor, localtriNor) > 0.f)
    {
        sdfGrids[idx].dist = -minUdf;
    }
    else
    {
        sdfGrids[idx].dist = minUdf;
    }

    sdfGrids[idx].geomId = minTriangle.geomId;
}

