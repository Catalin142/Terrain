#version 450

layout(push_constant) uniform SizePushConstant
{
	int patchCount;
	int minimumChunkSize;
} Size;

layout (binding = 0, r8ui) uniform writeonly uimage2D LODMap;

struct Node
{
    int Offset;
    int Lod;
};

layout(binding = 1) readonly buffer NodesUniformBufferSet
{
    Node node[];
} Nodes;

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
	if (gl_GlobalInvocationID.x >= 64 || gl_GlobalInvocationID.y >= 64)
		return;

	uvec2 texel = gl_GlobalInvocationID.xy;

	for (int nodeIndex = 0; nodeIndex < Size.patchCount; nodeIndex++)
	{
		Node node = Nodes.node[nodeIndex];

		uvec2 offset;
		offset.x = node.Offset & 0x0000ffff;
		offset.y = node.Offset << 16 & 0x0000ffff;

		int chunkSize = (1 << node.Lod);
		uvec2 lodTexel = (texel / chunkSize) * chunkSize;
		if (lodTexel.x == offset.x && lodTexel.y == offset.y)
		{
			imageStore(LODMap, ivec2(texel), uvec4(1 << node.Lod, 1u, 1u, 1u));
			return;
		}
	}
}