
#define BLOCK_SIZE 8
#define FLT_MAX 3.402823466e+38

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

StructuredBuffer<BVHNode> bvhNodes : register(t0);

StructuredBuffer<Geom> geoms : register(t1);

//RWTexture2D<float4> OutTexture : register(u0);
RWStructuredBuffer<SDFGrid> sdfGrids : register(u0);

// Linear clamp sampler.
//SamplerState LinearClampSampler : register(s0);




float udfTriangle(float3 p, float3 a, float3 b, float3 c)
{
    float3 ba = b - a;
    float3 pa = p - a;
    float3 cb = c - b;
    float3 pb = p - b;
    float3 ac = a - c;
    float3 pc = p - c;
    float3 nor = cross(ba, ac);

    float3 ba_pa = ba * clamp(dot(ba, pa) / dot(ba, ba), 0.f, 1.f) - pa;
    float3 cb_pb = cb * clamp(dot(cb, pb) / dot(cb, cb), 0.f, 1.f) - pb;
    float3 ac_pc = ac * clamp(dot(ac, pc) / dot(ac, ac), 0.f, 1.f) - pc;

    return sqrt(
        (sign(dot(cross(ba, nor), pa)) +
         sign(dot(cross(cb, nor), pb)) +
         sign(dot(cross(ac, nor), pc)) < 2.f)
        ?
        min(min(
            dot(ba_pa, ba_pa),
            dot(cb_pb, cb_pb)),
            dot(ac_pc, ac_pc))
        :
        dot(nor, pa) * dot(nor, pa) / dot(nor, nor));
}

float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f);
}

float udfBox(float3 p, float3 minCorner, float3 maxCorner)
{
    float3 center = (maxCorner + minCorner) / 2.f;
    float3 halfScale = maxCorner - center;
    return sdBox(p - center, halfScale);
}




[numthreads(BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE)]
void main(ComputeShaderInput IN)
{
    //DispatchThreadID.xy是二维索引，对应cuda中的索引，可以用这索引直接访问texture对应像素点float4
    
    int x = IN.DispatchThreadID.x;
    int y = IN.DispatchThreadID.y;
    int z = IN.DispatchThreadID.z;

    if (x >= sdf.resolution.x || y >= sdf.resolution.y || z >= sdf.resolution.z) return;
    int idx = z * sdf.resolution.x * sdf.resolution.y + y * sdf.resolution.x + x;


    float3 voxelPos = float3(sdf.minCorner.x + sdf.gridExtent.x * (float(x) + 0.5f),
                             sdf.minCorner.y + sdf.gridExtent.y * (float(y) + 0.5f),
                             sdf.minCorner.z + sdf.gridExtent.z * (float(z) + 0.5f));
    
    // find closest triangle
    int currIdx = 0;

    int idxToVisit[64];
    int toVisitOffset = 0;

    idxToVisit[toVisitOffset++] = 0;

    BVHNode minTriangle;
    minTriangle.geomId = -1;
    float minUdf = FLT_MAX;

    while (toVisitOffset > 0)
    {
        currIdx = idxToVisit[--toVisitOffset];
        //if (currIdx >= bvhNodes_size) continue;

        if (!bvhNodes[currIdx].isLeaf)
        {
            float distToBox = udfBox(voxelPos, bvhNodes[currIdx].minCorner, bvhNodes[currIdx].maxCorner);
            if (distToBox > minUdf) continue;

            int leftIdx = currIdx * 2 + 1;
            int rightIdx = leftIdx + 1;

            idxToVisit[toVisitOffset++] = rightIdx;
            idxToVisit[toVisitOffset++] = leftIdx;
        }
        else
        {
            if (!bvhNodes[currIdx].hasFace) continue;
            BVHNode face = bvhNodes[currIdx];
            float t = udfTriangle(voxelPos, face.point1, face.point2, face.point3.xyz);

            if (minUdf > t)
            {
                minTriangle = bvhNodes[currIdx];
                minUdf = t;
            }
        }
    }


    // will crash without this
    if (minTriangle.geomId == -1) return;
    int geomId = minTriangle.geomId;

    float3 triCenter = (minTriangle.point1 + minTriangle.point2 + minTriangle.point3.xyz) / 3.f;
    float3 worldNor = normalize(triCenter - voxelPos);
    float3 localNor = mul(geoms[geomId].inverseTransform, float4(worldNor, 0.f));
    float3 localtriNor = mul(geoms[geomId].inverseTransform, minTriangle.normal1);
    
    // if inside
    if (dot(localNor, localtriNor) > 0.f)
    {
        sdfGrids[idx].dist = -minUdf;
    }
    else
    {
        sdfGrids[idx].dist = minUdf;
    }

    sdfGrids[idx].geomId = minTriangle.geomId;
}

