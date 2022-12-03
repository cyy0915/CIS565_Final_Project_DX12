#include "DX12LibPCH.h"

#include "dx12lib/bvhTree.h"
#if USE_BVH_FOR_INTERSECTION
#include <algorithm>

using namespace dx12lib;

BVHNode::BVHNode()
	: idx(-1), minCorner(glm::vec3(FLT_MAX)), maxCorner(glm::vec3(FLT_MIN)), isLeaf(false), hasFace(false), axis(0)
{
}


BVHNode::~BVHNode()
{
}

 
int BVHNode::getLeftChildIdx() const
{
	return idx * 2 + 1;
}

 
int BVHNode::getRightChildIdx() const
{
	return idx * 2 + 2;
}


BVHTree::BVHTree()
{
}


BVHTree::~BVHTree()
{
}

// this function modifies the order of faces, so Geom.faceStartIdx may become invalid

void BVHTree::build(std::vector<Triangle>& faces)
{
	if (faces.size() == 0) return;
	// The maximum depth of nodes if we use equal counts methods to build the tree.
	int depth = glm::ceil(glm::log2((float)faces.size())) + 1;
	bvhNodes.resize(glm::pow(2, depth) - 1);

	recursiveBuild(0, faces);
}


void BVHTree::recursiveBuild(int nodeIdx, std::vector<Triangle>& faces)
{
	if (nodeIdx >= bvhNodes.size() * 2 - 1) return;
	bvhNodes[nodeIdx].idx = nodeIdx;

	if (faces.size() == 0)
	{
		bvhNodes[nodeIdx].isLeaf = true;
	}

	// find the bounding box of the node
	bvhNodes[nodeIdx].minCorner = faces[0].minCorner;
	bvhNodes[nodeIdx].maxCorner = faces[0].maxCorner;
	for (int i = 1; i < faces.size(); i++)
	{
		bvhNodes[nodeIdx].minCorner = glm::min(bvhNodes[nodeIdx].minCorner, faces[i].minCorner);
		bvhNodes[nodeIdx].maxCorner = glm::max(bvhNodes[nodeIdx].maxCorner, faces[i].maxCorner);
	}

	if (/*nodeIdx >= (bvhNodes.size() + 1) / 2 - 1 && */faces.size() == 1)
	{
		bvhNodes[nodeIdx].face = faces[0];
		bvhNodes[nodeIdx].isLeaf = true;
		bvhNodes[nodeIdx].hasFace = true;
		return;
	}

	// estimate split axis by calculating maximum extent
	int& axis = bvhNodes[nodeIdx].axis;
	axis = 0;
	glm::vec3 diagonal = bvhNodes[nodeIdx].maxCorner - bvhNodes[nodeIdx].minCorner;
	if (diagonal.x > diagonal.y && diagonal.x > diagonal.z) axis = 0;
	else if (diagonal.y > diagonal.z) axis = 1;
	else axis = 2;

	// Partition primitives into equally sized subsets
	int midIdx = faces.size() / 2;
	std::nth_element(&faces[0], &(faces[midIdx]), &(faces[faces.size() - 1]) + 1,
		[axis](const Triangle& a, const Triangle& b)
		{
			return a.centroid[axis] < b.centroid[axis];
		});

	std::vector<Triangle> leftFaces(faces.begin(), faces.begin() + midIdx);
	recursiveBuild(bvhNodes[nodeIdx].getLeftChildIdx(), leftFaces);

	std::vector<Triangle> rightFaces(faces.begin() + midIdx, faces.end());
	recursiveBuild(bvhNodes[nodeIdx].getRightChildIdx(), rightFaces);
}

void BVHTree::getGPUData(std::vector<BVHNodeGPU>& data) {
	for (auto& node : bvhNodes) {
		data.push_back(BVHNodeGPU({ node.minCorner, node.idx, node.maxCorner, node.isLeaf,
			node.face.point1, node.hasFace, node.face.point2, node.face.geomId,
			glm::vec4(node.face.point3, 1.f), glm::vec4(node.face.normal1, 1.f), glm::vec4(node.face.normal2, 1.f),
			glm::vec4(node.face.normal3, 1.f), glm::vec4(node.face.minCorner, 1.f), glm::vec4(node.face.maxCorner, 1.f),
			glm::vec4(node.face.centroid, 1.f) }));
	}
}


#endif