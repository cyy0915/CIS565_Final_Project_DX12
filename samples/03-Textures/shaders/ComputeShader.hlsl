#define BLOCK_SIZE 8

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

struct Camera
{
    float4 position;
    float2 pixelLength;
    float2 resolution;
    matrix cameraToWorld;
    matrix projection;
};
ConstantBuffer<Camera> camera : register(b0);

struct SDFvoxel
{
    float3 normal;
    float l;
    float4 color;
};
ConstantBuffer<Camera> SDF : register(b1);

// Source mip map.
Texture2D<float4> depthTexture : register(t0);

// Write up to 4 mip map levels.
RWTexture2D<float4> outTexture : register(u0);

// Linear clamp sampler.
SamplerState LinearClampSampler : register(s0);

struct Ray
{
    float3 origin;  //world space
    float3 dir;     //world space
    float3 dirVS;   //view space
    float3 color;
    bool ss;        //if still in screen tracing
};

struct Intersection{
    bool hit;
    float3 position;
    float3 normal;
    float3 color;
};

void generateRayFromCamera(in float pixelx, in float pixely, out Ray ray)
{
    ray.color = float3(1, 1, 1);
    ray.origin = camera.position;
    float3 pPixel = float3(camera.pixelLength.x * (pixelx - camera.resolution.x * 0.5f) / 2.f, -camera.pixelLength.y * (pixely - camera.resolution.y * 0.5f) / 2.f, 1);
    pPixel = normalize(pPixel);
    ray.dir = mul((float3x3) camera.cameraToWorld, pPixel);
    ray.dirVS = pPixel;
    ray.ss = true;
}

void firstIntersect(in Ray ray, out Intersection isect){
    
}

void intersect(in Ray ray, out Intersection isect){
    //intersect a ray

    if (ray.ss){
        

    }
    else{
        float3 center = float3(5, 0, 5);
        float3 v = center - ray.origin;
        float d = length(cross(v, ray.dir));
        if (d < 2){
            isect.hit = true;
            isect.position = ray.origin + ray.dir * dot(v, ray.dir);
            isect.normal = normalize(isect.position - center);
            isect.color = float3(1, 0, 0); 
        }
        else{
            isect.hit = false;
            isect.color = float3(0.1, 0.1, 0.1);
        }
    }
    
}

float distanceSquared(float2 a, float2 b) { a -= b; return dot(a, a); }

// Returns true if the ray hit something
bool traceScreenSpaceRay1(
 // Camera-space ray origin, which must be within the view volume
 float3 csOrig, 

 // Unit length camera-space ray direction
 float3 csDir,

 // A projection matrix that maps to pixel coordinates (not [-1, +1]
 // normalized device coordinates)
 matrix proj, 

 // The camera-space Z buffer (all negative values)
 Texture2D csZBuffer, float drate,

 // Dimensions of csZBuffer
 float2 csZBufferSize,

 // Camera space thickness to ascribe to each pixel in the depth buffer
 float zThickness, 

 // (Negative number)
 float nearPlaneZ, 

 // Step in horizontal or vertical pixels between samples. This is a float
 // because integer math is slow on GPUs, but should be set to an integer >= 1
 float stride,

 // Number between 0 and 1 for how far to bump the ray in stride units
 // to conceal banding artifacts
 float jitter,

 // Maximum number of iterations. Higher gives better images but may be slow
 const float maxSteps, 

 // Maximum camera-space distance to trace before returning a miss
 float maxDistance, 

 // Pixel coordinates of the first intersection with the scene
 out float2 hitPixel, 

 // Camera space location of the ray hit
 out float3 hitPoint) {

    // Clip to the near plane    
    float rayLength = ((csOrig.z + csDir.z * maxDistance) > nearPlaneZ) ?
        (nearPlaneZ - csOrig.z) / csDir.z : maxDistance;
    float3 csEndPoint = csOrig + csDir * rayLength;

    // Project into homogeneous clip space
    float4 H0 = mul(proj,float4(csOrig, 1.0));
    float4 H1 = mul(proj,float4(csEndPoint, 1.0));

    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

    // The interpolated homogeneous version of the camera-space points  
    float3 Q0 = csOrig * k0, Q1 = csEndPoint * k1;

    // Screen-space endpoints
    float2 P0 = H0.xy * k0, P1 = H1.xy * k1;
    P0 = (P0 + 1) / 2.f * csZBufferSize;
    P1 = (P1 + 1) / 2.f * csZBufferSize;

    // If the line is degenerate, make it cover at least one pixel
    // to avoid handling zero-pixel extent as a special case later
    float tP = (distanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0;
    P1 += float2(tP, tP);
    float2 delta = P1 - P0;

    // Permute so that the primary iteration is in x to collapse
    // all quadrant-specific DDA cases later
    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) { 
        // This is a more-vertical line
        permute = true; delta = delta.yx; P0 = P0.yx; P1 = P1.yx; 
    }

    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;

    // Track the derivatives of Q and k
    float3  dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;
    float2  dP = float2(stepDir, delta.y * invdx);

    // Scale derivatives by the desired pixel stride and then
    // offset the starting values by the jitter fraction
    dP *= stride; dQ *= stride; dk *= stride;
    P0 += dP * jitter; Q0 += dQ * jitter; k0 += dk * jitter;

    // Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
    float3 Q = Q0; 

    // Adjust end condition for iteration direction
    float  end = P1.x * stepDir;

    float k = k0, stepCount = 0.0, prevZMaxEstimate = csOrig.z;
    float rayZMin = prevZMaxEstimate, rayZMax = prevZMaxEstimate;
    float sceneZMax = rayZMax + 100;
    for (float2 P = P0; 
         ((P.x * stepDir) <= end) && (stepCount < maxSteps) &&
         ((rayZMax < sceneZMax - zThickness) || (rayZMin > sceneZMax)) &&
          (sceneZMax != 0); 
         P += dP, Q.z += dQ.z, k += dk, ++stepCount) {
        
        rayZMin = prevZMaxEstimate;
        rayZMax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
        prevZMaxEstimate = rayZMax;
        if (rayZMin > rayZMax) { 
           float t = rayZMin; rayZMin = rayZMax; rayZMax = t;
        }

        hitPixel = permute ? P.yx : P;
        // You may need hitPixel.y = csZBufferSize.y - hitPixel.y; here if your vertical axis TODO
        // is different than ours in screen space
        sceneZMax = csZBuffer[int2(hitPixel)] * drate;
        //sceneZMax = texelFetch(csZBuffer, int2(hitPixel), 0);
    }
    
    // Advance Q based on the number of steps
    Q.xy += dQ.xy * stepCount;
    hitPoint = Q * (1.0 / k);
    return (rayZMax >= sceneZMax - zThickness) && (rayZMin < sceneZMax);
}

float3 traceScreenSpace(float3 pos, float3 dir, float step, float drate, float zThickness, float maxIter, out int2 hitpixel, out float3 hitpoint){
    if (pos.x >= 9.9)
    {
        return float3(1, 1, 1);
    }
    int n = 0;
    float4 originH = mul(camera.projection, float4(pos, 1)); originH = originH / originH.w;
    float2 originP = originH.xy / originH.z; originP = (originP + 1) / 2.f * camera.resolution;
    int2 originC = originP;
    pos = pos + dir * step * 10;
    while (n < maxIter){
        n = n + 1;
        float3 newpos = pos + dir * step;
        if (newpos.z <= 0){
            return float3(1, 0, 0);
        }

        float4 h1 = mul(camera.projection, float4(newpos, 1)); h1 = h1 / h1.w;
        float2 p1 = h1.xy / h1.z; p1 = (p1 + 1) / 2.f * camera.resolution;
        int2 c1 = p1;

        if (c1.x == originC.x && c1.y == originC.y)
        {
            pos = newpos;
            continue;
        }
        
        if (c1.x < 0 || c1.x >= camera.resolution.x || c1.y < 0 || c1.y >= camera.resolution.y){
            return float3(0, 1, 0);

        }
        float rayz = newpos.z;
        float scenez = depthTexture[int2(c1.x, camera.resolution.y - c1.y)].x * drate;
        if (rayz < scenez - zThickness){
            pos = newpos;
            continue;
        }
        else if (rayz >= scenez - zThickness && rayz < scenez + zThickness){
            hitpixel = c1;
            hitpoint = newpos;
            return float3(1, 1, 1);

        }
        else{
            return float3(0, 0, 1);

        }
    }
    return float3(0, 0, 1);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    float drate = 40.f;
    Ray ray;
    generateRayFromCamera(IN.DispatchThreadID.x, IN.DispatchThreadID.y, ray);
    
    //Intersection isect;
    //firstIntersect(ray, isect);

    //first hit
    float4 pixelColor = depthTexture[IN.DispatchThreadID.xy];
    float z = pixelColor.x * drate;
    float3 normal = pixelColor.yzw;
    float d = z / ray.dirVS.z;
    float3 p = ray.dirVS * d;
    float3 newdir = normalize(ray.dirVS + 2 * normal);

    float2 hitpixel;
    float3 hitpoint;
    float3 r = traceScreenSpace(p, newdir, 0.1, 40, 0.5, 200, hitpixel, hitpoint);
    // Camera-space ray origin, which must be within the view volume

    float depth = depthTexture[IN.DispatchThreadID.xy].x;
    //outTexture[IN.DispatchThreadID.xy] = float4(depth,depth,depth, 1);
    
    outTexture[IN.DispatchThreadID.xy] = float4(r, 1);
    //outTexture[IN.DispatchThreadID.xy] = float4(isect.color, 1);

}