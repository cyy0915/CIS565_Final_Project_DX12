#define BLOCK_SIZE 8

#define SAMPLE_COUNT 500

//Hemisphere Harmonic Coefficient
//reference: 
//(0,0) (m=0,l=0)
#define HSH_COEFFICIENT_0 0.398942280f
//(-1,1) (m=-1,l=1)
#define HSH_COEFFICIENT_1 0.488602512f
//(0,1)(m=0,l=1)
#define HSH_COEFFICIENT_2 0.690988299f
//(1,1)(m=1,l=1)
#define HSH_COEFFICIENT_3 0.488602512f
//(-2,2)(m=-2,l=2)
#define HSH_COEFFICIENT_4 0.182091405f
//(-1,2)(m=-1,l=2)
#define HSH_COEFFICIENT_5 0.364182810f
//(0,2)(m=0,l=2)
#define HSH_COEFFICIENT_6 0.892062058f
//(1,2)(m=1,l=2)
#define HSH_COEFFICIENT_7 0.364182810f
//(2,2)(m=2,l=2)
#define HSH_COEFFICIENT_8 0.182091405f

#define PI   3.1415926535897932384626422832795028841971f
#define SQRT_OF_ONE_THIRD 0.5773502691896257645091487805019574556476f

static const float HemisphereHarmonicCoefficient[9] =
{HSH_COEFFICIENT_0, HSH_COEFFICIENT_1, HSH_COEFFICIENT_2,
HSH_COEFFICIENT_3, HSH_COEFFICIENT_4, HSH_COEFFICIENT_5, HSH_COEFFICIENT_6, HSH_COEFFICIENT_7, HSH_COEFFICIENT_8
};

//This function is used to compute Lagendre value
//Associate Legendre polynominal
float getLegendrePolynomialsValue(int index,float input)
{
    float result = input;
    //This need to be put into Legendre Polynomials
    switch (index)
    {
        case  0:
        //(m=0,l=0)
        //p(0,0)=1
            result =  1;
            break;
        case 1:
        //(m=-1,l=1)
            result = -0.5f*sqrt(pow(input, 2) - 1);
            break;
        case 2:
        //(m=0,l=1)
            result = input;
            break;
        case  3:
        //(m=1,l=1)
            result = sqrt(pow(input, 2) - 1);
            break;
        case  4:
        //(m=-2,l=2)
            result = 0.125f * (1 - pow(input, 2));
            break;
        case  5:
         //(m=-1,l=2)
            result = -0.5f * input * sqrt(pow(input,2)-1);
            break;
        case  6:
        //(m=0,l=2)
            result = 0.5f * (3 * pow(input, 2) - 1);
            break;
        case  7:
        //(m=1,l=2)
            result = 3f * input * sqrt(pow(input, 2) - 1);
            break;
        case  8:
         //(m=2,l=2)
            result = 3f - 3f * pow(input, 2);
            break;
    }
    return result;
}

//return H(m,l) 
float getHemisphereHarmonicBasis(int index,float theta,float phi)
{
    float result = 0;
    float factor = 2 * cos(theta) - 1;
    switch (index)
    {
        case  0:
        //��m=0,l=0��
            result = HemisphereHarmonicCoefficient[0] * getLegendrePolynomialsValue(index, cos(theta));
            break;
        case 1:
        //(m=-1,l=1)
            result = sqrt(2) * HemisphereHarmonicCoefficient[index] * sin(phi) * getLegendrePolynomialsValue(index, factor);
            break;
        case  2:
        //(m=0,l=1)
            result = HemisphereHarmonicCoefficient[index] * getLegendrePolynomialsValue(index, cos(theta));
            break;
        case  3:
        //(m=1,l=1)
            result = sqrt(2) * HemisphereHarmonicCoefficient[index] * cos(phi) * getLegendrePolynomialsValue(index, factor);
            break;
        case  4:
        //(m=-2,l=2)
            result = sqrt(2) * HemisphereHarmonicCoefficient[index] * sin(2 * phi) * getLegendrePolynomialsValue(index, factor);
            break;
        case  5:
        //(m=-1,l=2)
            result = sqrt(2) * HemisphereHarmonicCoefficient[index] * sin(phi) * getLegendrePolynomialsValue(index, factor);
            break;
        case 6:
        //(m=0,l=2)
            result = HemisphereHarmonicCoefficient[index] * getLegendrePolynomialsValue(index, cos(theta));
            break;
        case  7:
        //(m=1,l=2)
            result = sqrt(2) * HemisphereHarmonicCoefficient[index] * cos(phi) * getLegendrePolynomialsValue(index, factor);
            break;
        case index = 8:
        //(m=2,l=2)
            result = sqrt(2) * HemisphereHarmonicCoefficient[index] * cos(2 * phi) * getLegendrePolynomialsValue(index, factor);
            break;
    }
    return result;
 }



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
    float3 radianceCache;
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

float generateRandomSample(int min,int max)
{
    
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
//Hanlin
void ComputeRadianceCache(
    Ray ray,
    Intersection intersect,
    float3 normal)
{
    float uniform_distribution_num = generateRandomSample(0, 1);
    float3 lamda[9] = float3(0, 0, 0);
    ray.radianceCache = float3(0, 0, 0);
    float factor = 2 * PI / SAMPLE_COUNT;
    
    for (int i = 0; i < 9;i++)
    {
        lamda[i] = float3(0, 0, 0);
    }
    
    // generate n sample light
        for (int i = 0; i < SAMPLE_COUNT; i++)
        {
        //generate random sample ray direction
  
            float up = sqrt(uniform_distribution_num); // cos(theta)
            float over = sqrt(1 - up * up); // sin(theta)
            float around = uniform_distribution_num * 2 * PI;

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
            float3 crossProduct_1 = cross(normal, directionNotNormal);
            float3 perpendicularDirection1 = normalize(crossProduct_1);
            float3 crossProduct_2 = cross(normal, perpendicularDirection1);
            float3 perpendicularDirection2 = normalize(crossProduct_2);
        
        //This is in world space

            float3 rayDir = up * normal
            + cos(around) * over * perpendicularDirection1
            + sin(around) * over * perpendicularDirection2;
        
            Ray newRay;
            newRay.dir = rayDir;
            newRay.origin = intersect.hit + 0.0001f * rayDir;
            Intersection newRayIntersection;
            intersect(newRay, newRayIntersection);
        //Get the generated rayDir's intersection BSDF cache
        //First need to convert sample direction it into sphere coordinate
            float r = sqrt(pow(rayDir.x, 2) + pow(rayDir.y, 2) + pow(rayDir.z, 2));
            float theta = acos(rayDir.z / r);
            float phi = acos(rayDir.x / (r * sin(theta)));

        //need to convert raydir into hemisphere theta and phi
        //Compute lamda(m,l)(have 9 in total since second order)
            for (int n = 0; n < 9; n++)
            {
                lamda[n] = newRayIntersection.color * getHemisphereHarmonicBasis(n, theta, phi);
            }
        }
    
    for (int i = 0; i < 9;i++)
    {
        lamda[i] *= factor;
    }

    // Now have lamda, can compute ray radiance 
    //
    float ray_r = sqrt(pow(ray.dir.x, 2) + pow(ray.dir.y, 2) + pow(ray.dir.z, 2));
    float ray_theta = acos(ray.dir.z / ray_r);
    float ray_phi = acos(ray.dir.x / (ray_r * sin(ray_theta)));
    
    for (int i = 0; i < 9;i++)
    {
        ray.radianceCache += lamda[i] * getHemisphereHarmonicBasis(i, ray_theta,ray_phi);
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