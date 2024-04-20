#version 450

layout(push_constant) uniform SizePushConstant
{
	int patchCount;
	int minimumChunkSize;
} Size;

layout (binding = 0, r8ui) uniform writeonly uimage2D LODMap;

struct Node
{
    vec2 Offset;
    int Size;
    int Lod;
};

layout(binding = 1) uniform NodesUniformBufferSet
{
    Node node[64];
} Nodes;

layout(local_size_x = 64, local_size_y = 1) in;
void main()
{
	if (gl_GlobalInvocationID.x >= Size.patchCount)
		return;

	int texSize = imageSize(LODMap).x;
	Node node = Nodes.node[int(gl_GlobalInvocationID.x)];

	for (int y = int(node.Offset.y) / Size.minimumChunkSize; y < (int(node.Offset.y) + node.Size) / Size.minimumChunkSize; y++)
		for (int x = int(node.Offset.x) / Size.minimumChunkSize; x < (int(node.Offset.x) + node.Size) / Size.minimumChunkSize; x++)
		{			
			imageStore(LODMap, ivec2(x, y), uvec4(node.Lod, 1u, 1u, 1u));
		}
}