#version 460

struct PassMetadata
{
    uint TMPArray1Index;
    uint ResultArrayIndex;
    uint LoadedSize;
};

layout (set = 0, binding = 0, r8ui) uniform writeonly uimage2D LODMap;

struct GPUQuadTreeRenderChunk
{
    uint Offset;
    uint Lod;
    uint ChunkPhysicalLocation;
    uint NeighboursLod;
};

layout(std430, set = 0, binding = 1) buffer _FinalResult
{
    GPUQuadTreeRenderChunk nodes[];
} FinalResult;

layout(std430, set = 0, binding = 2) buffer _MetadataSet {
    PassMetadata metadata;
};

layout(local_size_x = 8, local_size_y = 8) in;
void main()
{
	uvec2 texel = gl_GlobalInvocationID.xy;

	for (int nodeIndex = 0; nodeIndex < metadata.ResultArrayIndex; nodeIndex++)
	{
		GPUQuadTreeRenderChunk node = FinalResult.nodes[nodeIndex];

		uvec2 offset;
		offset.x = node.Offset & 0x0000ffffu;
		offset.y = (node.Offset >> 16) & 0x0000ffffu;

		int chunkSize = (1 << node.Lod);
		uvec2 lodTexel = (texel / chunkSize);
		if (lodTexel.x == offset.x && lodTexel.y == offset.y)
		{
			imageStore(LODMap, ivec2(texel), uvec4(node.Lod, 1u, 1u, 1u));
			return;
		}
	}
}