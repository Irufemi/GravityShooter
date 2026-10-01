#include "BasePassVertexOutput.hlsli"
#include "Transform.hlsli"

struct Well
{
	float32_t4x4 skeletonSpaceMatrix;
	float32_t4x4 skeletonInverseTransposeMatrix;
};