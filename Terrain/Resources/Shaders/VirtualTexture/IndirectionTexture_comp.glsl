#version 450
// get all nodes loaded this frame
// blit them in the indirection texture

// store each mip
layout (binding = 0, r32ui) uniform writeonly uimage2D indirectionTexture;

struct LoadedNode
{
    int virtualLocation;
    int physicalLocation;
    int mip;
};


layout(set = 1, binding = 0) uniform NodesUniformBuffer
{
    LoadedNode lNode[64];
} Nodes;

layout (push_constant) uniform VirtualMapProperties
{
    int chunkSize;
    int loadedNodesCount;
} props;

layout(local_size_x = 8, local_size_y = 1) in;
void main()
{
    if (gl_GlobalInvocationID.x >= props.loadedNodesCount)
        return;

    LoadedNode node = Nodes.lNode[gl_GlobalInvocationID.x];

    int virtualLocX = int(node.virtualLocation & 0x0000FFFF);
    int virtualLocY = int(node.virtualLocation & 0xFFFF0000) >> 16;

    ivec2 texel;
    texel.x = (virtualLocX / props.chunkSize) >> node.mip;
    texel.y = (virtualLocY / props.chunkSize) >> node.mip;

	imageStore(indirectionTexture, texel, uvec4(node.physicalLocation, 0u, 0u, 0u));
}