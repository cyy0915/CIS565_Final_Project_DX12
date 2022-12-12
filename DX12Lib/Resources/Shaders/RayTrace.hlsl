
#define BLOCK_SIZE 8
#define FLT_MAX 3.402823466e+38
#define PI                3.1415926535897932384626422832795028841971f
#define TWO_PI            6.2831853071795864769252867665590057683943f
#define SQRT_OF_ONE_THIRD 0.5773502691896257645091487805019574556476f
#define EPSILON           0.00001f
#define INV_PI           0.31830988618379067

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
    float3 color;
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

struct Camera
{
    float4 position;
    float2 pixelLength;
    float2 resolution;
    matrix cameraToWorld;
    matrix projection;
};

struct BVHNode
{
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
    int matId;
        //
    float3 point3;
    float pad1;
    //
    float4 normal1;
    float2 texCoord1;
    float2 texCoord2;
    //
    float2 texCoord3;
    float2 pad2;
    //
    float4 triangleMinCorner;
    float4 triangleMaxCorner;
    float4 center;
};

struct Ray
{
    float3 origin; //world space
    bool ss; //if still in screen tracing
    float3 dir; //world space
    bool direct; // if sample in the direction of light
    float3 dirVS; //view space
    float3 color;
    int2 xy;
};

struct Intersection
{
    bool hit;
    float t;
    int materiaId;
    float3 normal;
    float3 color;
};
struct RenderParm
{
    int iter;
    int change;
    int depth;
    int useSDF;
        //
    float3 lightDir;
    int screenTracing;
};

ConstantBuffer<Camera> camera : register(b0);
ConstantBuffer<SDF> sdf : register(b1);
ConstantBuffer<RenderParm> renderParm : register(b2);

StructuredBuffer<SDFGrid> SDFGrids : register(t0);
StructuredBuffer<BVHNode> bvhNodes : register(t1);

//gbuffers
Texture2D<float4> normalTexture : register(t2);
Texture2D<float4> depthMatTexture : register(t3);
Texture2D<float4> colorTexture : register(t4);

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
    ray.ss = false;
    ray.direct = false;
    ray.xy = int2(pixelx, pixely);
}

static uint2 seed;
float rng()
{
    seed += uint2(1, 1);
    uint2 q = 1103515245U * ((seed >> 1U) ^ (seed.yx));
    uint n = 1103515245U * ((q.x) ^ (q.y >> 3U));
    return float(n) * (1.0 / float(0xffffffffU));
}


float3 traceScreenSpace(float3 pos, float3 dir, float step, float drate, float zThickness, float maxIter, out int2 hitpixel, out float3 hitpoint, out bool hit)
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
            hit = false;
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
            hit = false;
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
            hit = true;
            hitpixel = c1;
            hitpoint = newpos;
            return float3(1, 1, 1);

        }
        else
        {
            hit = false;
            return float3(0, 0, 1);

        }
    }
    hit = false;
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

float sdfIntersectionTest(in Ray r, out float3 intersectionPoint, out float3 normal, out bool outside, out float3 color)
{
    int startGrid = sceneSDF(r.origin);

    //if (startGrid == nullptr) return -1.f;

    if (startGrid == -1)
    {
        outside = false;
        return -1;
    }
    else
        outside = true;

    normal = float3(0.f, 0.f, 0.f);
    //normal = estimateNormal(r.origin, sdf, SDFGrids);
    float t = sdf.gridExtent.x * 1.8 * (1 + 2 * rng());
    int maxMarchSteps = 64;
    float3 lastRayMarchPos = r.origin;
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

        if (SDFGrids[currSDFGrid].dist < sdf.gridExtent.x * 0.5)
        {
            intersectionPoint = lastRayMarchPos;
            normal = estimateNormal(lastRayMarchPos);
            color = SDFGrids[currSDFGrid].color;
            return t;
        }

        // Move along the view ray
        t += SDFGrids[currSDFGrid].dist;
        lastRayMarchPos = rayMarchPos;

    }

    return -1.f;
}

void intersect(in out Ray ray, out Intersection isect)
{
    //intersect a ray

    //temporarily just first hit in screen space
    if (ray.ss)
    {
        ray.ss = false;
        if (depthMatTexture[ray.xy].z > 900)
        {
            isect.hit = false;
            isect.t = -1;
        }
        else
        {
            isect.color = colorTexture[ray.xy];
            isect.normal = mul((float3x3) camera.cameraToWorld, (float3)normalTexture[ray.xy]);
            isect.hit = true;
            isect.t = depthMatTexture[ray.xy].z / ray.dirVS.z;
        }
    }
    else
    {
        float3 intersect_point;
        float3 normal;
        float t_min = FLT_MAX;
        bool outside = true;
        float3 color;
		// naive parse through global geoms
		t_min = sdfIntersectionTest(ray, intersect_point, normal, outside, color);
		//printf("%d\n",hit_geom_index);

        if (t_min == FLT_MAX || t_min < 0)
        {
            isect.t = -1.0f;
            isect.hit = false;
        }
        else
        {
			//The ray hits something, temporarily no material just diffuse texture color
            isect.t = t_min;
            //isect.materiaId = geoms[hit_geom_index].materialid;
            isect.normal = normal;
            isect.hit = true;
            isect.color = color;
        }
    }
}

void biasHitPos(in out float3 pos, in float3 normal)
{
    float3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = float3(1, 0, 0);
    }
    else if (abs(normal.y) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = float3(0, 1, 0);
    }
    else
    {
        directionNotNormal = float3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    float3 perpendicularDirection1 =
        normalize(cross(normal, directionNotNormal));
    float3 perpendicularDirection2 =
        normalize(cross(normal, perpendicularDirection1));
    
    float r = sqrt(rng()) * sdf.gridExtent.x * 0.5;
    float theta = rng() * TWO_PI;
    pos = pos + r * cos(theta) * perpendicularDirection1 + r * sin(theta) * perpendicularDirection2;

}

float3 calculateRandomDirectionInHemisphere(float3 normal, out float pdf)
{
    float up = sqrt(rng()); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = rng() * TWO_PI;

    // Find a direction that is not the normal based off of whether or not the
    // normal's components are all equal to sqrt(1/3) or whether or not at
    // least one component is less than sqrt(1/3). Learned this trick from
    // Peter Kutz.

    float3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = float3(1, 0, 0);
    }
    else if (abs(normal.y) < SQRT_OF_ONE_THIRD)
    {
        directionNotNormal = float3(0, 1, 0);
    }
    else
    {
        directionNotNormal = float3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    float3 perpendicularDirection1 =
        normalize(cross(normal, directionNotNormal));
    float3 perpendicularDirection2 =
        normalize(cross(normal, perpendicularDirection1));

    pdf = up * INV_PI;
    return up * normal
        + cos(around) * over * perpendicularDirection1
        + sin(around) * over * perpendicularDirection2;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    float3 skyColor = float3(135, 206, 235) / 255.f;
    //float3 skyColor = float3(0, 0, 0);
    float3 lightDir = normalize(renderParm.lightDir);
    float3 lightColor = float3(10, 10, 10);
    
    int x = IN.DispatchThreadID.x;
    int y = IN.DispatchThreadID.y;
    int2 xy = IN.DispatchThreadID.xy;
    if (x >= camera.resolution.x || y >= camera.resolution.y)
    {
        return;
    }
    
    //generate random seed;
    seed = uint2(renderParm.iter, renderParm.iter + 1) * uint2(xy);
    
    Ray ray;
    generateRayFromCamera(x, y, ray);
    ray.ss = renderParm.screenTracing;
    
    int maxDepth = renderParm.depth + 1;
    for (int depth = 0; depth < maxDepth; depth++)
    {
        Intersection isect;
        intersect(ray, isect);
        if (!isect.hit)
        {
            if (dot(ray.dir, -lightDir) > 0.99)
            {
                ray.color *= lightColor;
            }
            else
            {
                ray.color *= skyColor;
            }
            break;
        }
        else if (ray.direct)
        {
            ray.color = float3(0, 0, 0);
            break;
        }
        
        float3 pos;
        pos = ray.origin + ray.dir * isect.t;
        biasHitPos(pos, isect.normal);
        
        float pdf = 1;
        
        //MIS, assume parallel light
        if (dot(isect.normal, lightDir) > 0)
        {
            float tmppdf;
            ray.dir = calculateRandomDirectionInHemisphere(isect.normal, tmppdf);
            ray.origin = pos + ray.dir * EPSILON;
            pdf = tmppdf;
        }
        else
        {
            if (rng() < 0.5)
            {
                ray.dir = -lightDir;
                ray.origin = pos + ray.dir * EPSILON;
                ray.direct = true;
                pdf = 0.5;
            }
            else
            {
                float tmppdf;
                ray.dir = calculateRandomDirectionInHemisphere(isect.normal, tmppdf);
                ray.origin = pos + ray.dir * EPSILON;
                pdf = 0.5 * tmppdf;
            }
        }
        
        if (pdf < EPSILON)
        {
            ray.color = float3(0, 0, 0);
            break;
        }
        else
        {
            ray.color = ray.color * isect.color * max(dot(isect.normal, ray.dir), 0) / pdf;
        }
        
        if (depth == maxDepth - 1)
        {
            ray.color = float3(0, 0, 0);
        }
    }
    //outTexture[xy] = float4(ray.color, 1);
    if (renderParm.change)
    {
        outTexture[xy] = float4(0, 0, 0, 1);

    }
    outTexture[xy] = (outTexture[xy] * (renderParm.iter - 1) + float4(ray.color, 1)) / float(renderParm.iter);
    
    /*float4 pixelColor = normalTexture[1][xy];
    float z = pixelColor.x;
    float3 normal = normalTexture[0][xy].xyz;
    float d = z / ray.dirVS.z;
    float3 p = ray.dirVS * d;
    //float3 newdir = normalize(ray.dirVS + 2 * normal);
    float3 newdir = normalize(rng(xy / 1000.f + randomNum.n, z + randomNum.n));
    int2 hitpixel;
    float3 hitpoint;
    bool hit = false;
    float3 r = traceScreenSpace(p, newdir, 0.1, 1, 0.5, 400, hitpixel, hitpoint, hit);
    
    float4 currColor;
    if (hit)
    {
        //outTexture[xy] = float4(1, 1, 1, 1);
        currColor = normalTexture[2][xy] + normalTexture[2][hitpixel];
    }
    else
    {
        //outTexture[xy] = float4(0, 0, 0, 1);
        currColor = normalTexture[2][xy];
    }
    outTexture[xy] = (outTexture[xy] * (iter.size - 1) + currColor) / float(iter.size);*/

    //outTexture[xy] = colorTexture[xy];

}

