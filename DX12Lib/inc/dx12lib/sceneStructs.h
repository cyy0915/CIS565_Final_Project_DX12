#pragma once

#include <string>
#include <vector>
#include "glm/glm.hpp"
#include <DirectXMath.h>

#define BACKGROUND_COLOR (glm::vec3(0.0f))
#define USE_BVH_FOR_INTERSECTION 1

namespace dx12lib
{

    enum GeomType {
        SPHERE,
        CUBE,
        TRIANGLE,
        MESH,
    };

    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct alignas(16) GeomGPU {
        int materialid;
        glm::vec3 translation;
        //
        glm::vec3 rotation;
        int faceStartIdx; // use with array of Triangle
        //
        glm::vec3 scale;
        int faceNum;
        //
        DirectX::XMFLOAT4X4 transform;
        DirectX::XMFLOAT4X4 inverseTransform;
        DirectX::XMFLOAT4X4 invTranspose;
        enum GeomType type;
    };

    struct Geom {
        enum GeomType type;
        int materialid;
        glm::vec3 translation;
        glm::vec3 rotation;
        glm::vec3 scale;
        glm::mat4 transform;
        glm::mat4 inverseTransform;
        glm::mat4 invTranspose;
        int faceStartIdx; // use with array of Triangle
        int faceNum;

        GeomGPU getGPUData() {
            glm::mat4 transformT = glm::transpose(transform);
            glm::mat4 inverseTransformT = glm::transpose(inverseTransform);
            glm::mat4 invTransposeT = glm::transpose(invTranspose);

            return GeomGPU({ materialid, translation, rotation, faceStartIdx, scale, faceNum,
                DirectX::XMFLOAT4X4(&transformT[0][0]), DirectX::XMFLOAT4X4(&inverseTransformT[0][0]), DirectX::XMFLOAT4X4(&invTransposeT[0][0]),
                type });
        }
    };

    struct Triangle {
        glm::vec3 point1;
        glm::vec3 point2;
        glm::vec3 point3;
        glm::vec3 normal1;
        glm::vec3 normal2;
        glm::vec3 normal3;
#if USE_BVH_FOR_INTERSECTION
        int geomId;
        glm::vec3 minCorner;
        glm::vec3 maxCorner;
        glm::vec3 centroid;
        void computeLocalBoundingBox()
        {
            minCorner = glm::min(point1, glm::min(point2, point3));
            maxCorner = glm::max(point1, glm::max(point2, point3));
            centroid = (minCorner + maxCorner) * 0.5f;
        }
        void computeGlobalBoundingBox(const Geom& geom)
        {
            glm::vec3 globalPoint1 = glm::vec3(geom.transform * glm::vec4(point1, 1.0f));
            glm::vec3 globalPoint2 = glm::vec3(geom.transform * glm::vec4(point2, 1.0f));
            glm::vec3 globalPoint3 = glm::vec3(geom.transform * glm::vec4(point3, 1.0f));
            minCorner = glm::min(globalPoint1, glm::min(globalPoint2, globalPoint3));
            maxCorner = glm::max(globalPoint1, glm::max(globalPoint2, globalPoint3));
            centroid = (minCorner + maxCorner) * 0.5f;
        }

        void localToWorld(const Geom& geom)
        {
            point1 = glm::vec3(geom.transform * glm::vec4(point1, 1.0f));
            point2 = glm::vec3(geom.transform * glm::vec4(point2, 1.0f));
            point3 = glm::vec3(geom.transform * glm::vec4(point3, 1.0f));
            normal1 = glm::vec3(geom.transform * glm::vec4(normal1, 0.f));
            normal2 = glm::vec3(geom.transform * glm::vec4(normal2, 0.f));
            normal3 = glm::vec3(geom.transform * glm::vec4(normal3, 0.f));
        }
#endif
    };

    struct alignas(16) MaterialGPU {
        glm::vec3 color;
        float exponent;
        //
        glm::vec3 specularColor;
        float hasReflective;
        //
        float hasRefractive;
        float indexOfRefraction;
        float emittance;
    };

    struct Material1 {
        glm::vec3 color;
        struct {
            float exponent;
            glm::vec3 color;
        } specular;
        float hasReflective;
        float hasRefractive;
        float indexOfRefraction;
        float emittance;

        MaterialGPU getGPUData() {
            return MaterialGPU({ color, specular.exponent, specular.color, hasReflective, hasRefractive, indexOfRefraction, emittance });
        }
    };

    /*struct Camera {
        glm::ivec2 resolution;
        glm::vec3 position;
        glm::vec3 lookAt;
        glm::vec3 view;
        glm::vec3 up;
        glm::vec3 right;
        glm::vec2 fov;
        glm::vec2 pixelLength;
    };

    struct RenderState {
        Camera camera;
        unsigned int iterations;
        int traceDepth;
        std::vector<glm::vec3> image;
        std::string imageName;
    };*/

    struct PathSegment {
        Ray ray;
        glm::vec3 color;
        int pixelIndex;
        int remainingBounces;
    };

    // Use with a corresponding PathSegment to do:
    // 1) color contribution computation
    // 2) BSDF evaluation: generate a new ray
    struct ShadeableIntersection {
        float t;
        glm::vec3 surfaceNormal;
        int materialId;
    };

    // CHECKITOUT - a simple struct for storing scene geometry information per-pixel.
    // What information might be helpful for guiding a denoising filter?
    struct GBufferPixel {
        float t;
        glm::vec3 pos;
        glm::vec3 nor;
    };
    struct SDFGPU {
        glm::vec4 minCorner;
        glm::vec4 maxCorner;
        glm::ivec4 resolution;
        glm::vec4 gridExtent;
    };
    struct SDF
    {
        glm::vec3 minCorner;
        glm::vec3 maxCorner;
        glm::ivec3 resolution;
        glm::vec3 gridExtent;

        SDFGPU getGPUData() {
            return SDFGPU({ glm::vec4(minCorner, 1.f), glm::vec4(maxCorner, 1.f), glm::ivec4(resolution, 1.f), glm::vec4(gridExtent, 1.f) });
        }
    };

    struct SDFGrid {
        float dist;
        int geomId;
    };



}