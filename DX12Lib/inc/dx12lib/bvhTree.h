#pragma once

#include <vector>
#include "glm/glm.hpp"
#include "sceneStructs.h"
#include <DirectXMath.h>
using namespace DirectX;

#define USE_BVH_FOR_INTERSECTION 1

#if USE_BVH_FOR_INTERSECTION

namespace dx12lib {

    struct alignas(16) BVHNodeGPU {
        glm::vec3 minCorner;
        int idx;
        //
        glm::vec3 maxCorner;
        int isLeaf;
        //
        glm::vec3 point1;
        int hasFace;
        //
        glm::vec3 point2;
        int matId;
        //
        glm::vec4 point3;
        glm::vec4 normal1;
        glm::vec2 texCoord1;
        glm::vec2 texCoord2;
        //
        glm::vec2 texCoord3;
        glm::vec2 padding;
        //
        glm::vec4 triangleMinCorner;
        glm::vec4 triangleMaxCorner;
        glm::vec4 centroid;
    };

    class BVHNode
    {
    public:
        int idx;
        glm::vec3 minCorner, maxCorner; // The world-space bounds of this node
        //glm::vec3 centroid;
        bool isLeaf;
        bool hasFace;
        Triangle face;

        int axis; // split axis

        BVHNode();
        ~BVHNode();

        int getLeftChildIdx() const;
        int getRightChildIdx() const;
    };

    class BVHTree
    {
    public:
        //BVHNode* root;
        std::vector<BVHNode> bvhNodes;

        BVHTree();
        ~BVHTree();
        void build(std::vector<Triangle>& faces);

        void getGPUData(std::vector<BVHNodeGPU>& data);

    private:
        void recursiveBuild(int nodeIdx, std::vector<Triangle>& faces);

    };
}

#endif