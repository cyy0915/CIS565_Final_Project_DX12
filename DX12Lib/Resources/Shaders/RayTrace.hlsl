
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
    float3 minCorner;
    int pad1;
    float3 maxCorner;
    int pad2;
    int3 resolution;
    int pad4;
    float3 gridExtent;
    int pad3;
};

struct Geom
{
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
    int4 type;
};

struct Camera
{
    float4 position;
    float2 pixelLength;
    float2 resolution;
    matrix cameraToWorld;
    matrix projection;
};

struct Material {
    float3 color;
    float exponent;
        //
    float3 specularColor;
    float hasReflective;
        //
    float hasRefractive;
    float indexOfRefraction;
    float emittance;
    float padding;
};

struct Ray
{
    float3 origin; //world space
    float3 dir; //world space
    float3 dirVS; //view space
    float3 color;
    bool ss; //if still in screen tracing
};

struct Intersection
{
    bool hit;
    float t;
    int materiaId;
    float3 normal;
    float3 color;
};
struct gSize
{
    int size;
};

ConstantBuffer<Camera> camera : register(b0);
ConstantBuffer<SDF> sdf : register(b1);
ConstantBuffer<gSize> geomNum : register(b2);

StructuredBuffer<SDFGrid> SDFGrids : register(t0);
StructuredBuffer<Material> mats : register(t1);
StructuredBuffer<Geom> geoms : register(t2);

//gbuffers
Texture2D<float4> normalTexture : register(t3);
Texture2D<float4> depthMatTexture : register(t4);

RWTexture2D<float4> outTexture : register(u0);

// Linear clamp sampler.
//SamplerState LinearClampSampler : register(s0);


void generateRayFromCamera(in float pixelx, in float pixely, out Ray ray)
{
    ray.color = float3(1, 1, 1);
    ray.origin = camera.position;
    float3 pPixel = float3(camera.pixelLength.x * (pixelx - camera.resolution.x * 0.5f), -camera.pixelLength.y * (pixely - camera.resolution.y * 0.5f), 1);
    pPixel = normalize(pPixel);
    ray.dir = mul((float3x3) camera.cameraToWorld, pPixel);
    ray.dirVS = pPixel;
    ray.ss = true;
}

float3 traceScreenSpace(float3 pos, float3 dir, float step, float drate, float zThickness, float maxIter, out int2 hitpixel, out float3 hitpoint)
{
    int n = 0;
    float4 originH = mul(camera.projection, float4(pos, 1));
    originH = originH / originH.w;
    float2 originP = originH.xy / originH.z;
    originP = (originP + 1) / 2.f * camera.resolution;
    int2 originC = originP;
    pos = pos + dir * step * 10;
    while (n < maxIter)
    {
        n = n + 1;
        float3 newpos = pos + dir * step;
        if (newpos.z <= 0)
        {
            return float3(1, 0, 0);
        }

        float4 h1 = mul(camera.projection, float4(newpos, 1));
        h1 = h1 / h1.w;
        float2 p1 = h1.xy / h1.z;
        p1 = (p1 + 1) / 2.f * camera.resolution;
        int2 c1 = p1;

        if (c1.x == originC.x && c1.y == originC.y)
        {
            pos = newpos;
            continue;
        }
        
        if (c1.x < 0 || c1.x >= camera.resolution.x || c1.y < 0 || c1.y >= camera.resolution.y)
        {
            return float3(0, 1, 0);

        }
        float rayz = newpos.z;
        float scenez = depthMatTexture[int2(c1.x, camera.resolution.y - c1.y)].x * drate;
        if (rayz < scenez - zThickness)
        {
            pos = newpos;
            continue;
        }
        else if (rayz >= scenez - zThickness && rayz < scenez + zThickness)
        {
            hitpixel = c1;
            hitpoint = newpos;
            return float3(1, 1, 1);

        }
        else
        {
            return float3(0, 0, 1);

        }
    }
    return float3(0, 0, 1);
}

int sceneSDF(float3 pos)
{
    int3 gridCoord = floor((pos - sdf.minCorner) / sdf.gridExtent);
    int idx = gridCoord.z * sdf.resolution.x * sdf.resolution.y + gridCoord.y * sdf.resolution.x + gridCoord.x;
    if (gridCoord.x < 0 || gridCoord.y < 0 || gridCoord.z < 0 || gridCoord.x >= sdf.resolution.x || gridCoord.y >= sdf.resolution.y || gridCoord.z >= sdf.resolution.z) 
        return -1;

    return idx;
}

float3 estimateNormal(float3 p)
{
    int currGrid = sceneSDF(p);
    if (currGrid == -1)
        return float3(0, 0, 0);
    int dx1Grid = sceneSDF(float3(p.x + sdf.gridExtent.x, p.y, p.z));
    float dx1 = dx1Grid == -1 ? SDFGrids[currGrid].dist : SDFGrids[dx1Grid].dist;
    int dx2Grid = sceneSDF(float3(p.x - sdf.gridExtent.x, p.y, p.z));
    float dx2 = dx2Grid == -1 ? SDFGrids[currGrid].dist : SDFGrids[dx2Grid].dist;
    int dy1Grid = sceneSDF(float3(p.x, p.y + sdf.gridExtent.y, p.z));
    float dy1 = dy1Grid == -1 ? SDFGrids[currGrid].dist : SDFGrids[dy1Grid].dist;
    int dy2Grid = sceneSDF(float3(p.x, p.y - sdf.gridExtent.y, p.z));
    float dy2 = dy2Grid == -1 ? SDFGrids[currGrid].dist : SDFGrids[dy2Grid].dist;
    int dz1Grid = sceneSDF(float3(p.x, p.y, p.z + sdf.gridExtent.z));
    float dz1 = dz1Grid == -1 ? SDFGrids[currGrid].dist : SDFGrids[dz1Grid].dist;
    int dz2Grid = sceneSDF(float3(p.x, p.y, p.z - sdf.gridExtent.z));
    float dz2 = dz2Grid == -1 ? SDFGrids[currGrid].dist : SDFGrids[dz2Grid].dist;

    return normalize(float3(
        dx1 - dx2,
        dy1 - dy2,
        dz1 - dz2
    ));
}

float sdfIntersectionTest(in Ray r, out float3 intersectionPoint, out float3 normal, out bool outside, out int hitGeomId)
{
    int startGrid = sceneSDF(r.origin);

    //if (startGrid == nullptr) return -1.f;

    if (startGrid == -1 || SDFGrids[startGrid].dist < 0.f)
        outside = false;
    else
        outside = true;


    normal = float3(0.f, 0.f, 0.f);
    //normal = estimateNormal(r.origin, sdf, SDFGrids);

    //float t = startGrid.dist;
    float t = 0.1f;

    int maxMarchSteps = 64;
    float3 lastRayMarchPos = r.origin;
    int lastGeomId = -1;
    for (int i = 0; i < maxMarchSteps; i++)
    {
        float3 rayMarchPos = r.origin + r.dir * t;
        int currSDFGrid = sceneSDF(rayMarchPos);
        
        // ??
        if (currSDFGrid == -1)
        {
            return -1.f;
            //t += 0.7f;
            //continue;
        }

        if (SDFGrids[currSDFGrid].dist < 0.01f)
        {
            intersectionPoint = lastRayMarchPos;
            normal = estimateNormal(lastRayMarchPos);
            hitGeomId = lastGeomId;
            
            return t;
        }

        // Move along the view ray
        t += SDFGrids[currSDFGrid].dist;
        lastGeomId = SDFGrids[currSDFGrid].geomId;
        lastRayMarchPos = rayMarchPos;

    }

    return -1.f;
}

void intersect(in Ray ray, out Intersection isect)
{
    //intersect a ray

    if (ray.ss)
    {
        

    }
    else
    {
        float t;
        float3 intersect_point;
        float3 normal;
        float t_min = FLT_MAX;
        int hit_geom_index = -1;
        bool outside = true;

        float3 tmp_intersect;
        float3 tmp_normal;

		// naive parse through global geoms
        int geoms_size = geomNum.size;
		t_min = sdfIntersectionTest(ray, intersect_point, normal, outside, hit_geom_index);
		//printf("%d\n",hit_geom_index);

        if (hit_geom_index == -1 || t_min == FLT_MAX || t_min < 0)
        {
            isect.t = -1.0f;
            isect.hit = false;
        }
        else
        {
			//The ray hits something
            isect.t = t_min;
            isect.materiaId = geoms[hit_geom_index].materialid;
            isect.normal = normal;
            isect.hit = true;
        }
    }
    
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    int x = IN.DispatchThreadID.x;
    int y = IN.DispatchThreadID.y;
    int2 xy = IN.DispatchThreadID.xy;
    if (x >= camera.resolution.x || y >= camera.resolution.y)
    {
        return;
    }
    Ray ray;
    generateRayFromCamera(x, y, ray);
    
    Intersection isect;
    ray.ss = false;
    intersect(ray, isect);
    float4 color = float4(0, 0, 0, 1);
    if (isect.hit)
    {
        color = float4(isect.t / 40.f, 0, 0, 1);
    }
    outTexture[xy] = color;
    
    float4 pixelColor = depthMatTexture[xy];
    float z = pixelColor.x;
    float3 normal = normalTexture[xy].xyz;
    float d = z / ray.dirVS.z;
    float3 p = ray.dirVS * d;
    float3 newdir = normalize(ray.dirVS + 2 * normal);
    float2 hitpixel;
    float3 hitpoint;
    //float3 r = traceScreenSpace(p, newdir, 0.1, 1, 0.5, 400, hitpixel, hitpoint);
    
    //outTexture[xy] = float4(r, 1);

}

