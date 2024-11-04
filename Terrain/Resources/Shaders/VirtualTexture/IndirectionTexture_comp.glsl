#version 460
// get all nodes loaded this frame
// blit them in the indirection texture

// store each mip
layout (binding = 0, r32ui) uniform writeonly uimage2D indirectionTexture[6];

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
    texel.x = (virtualLocX >> node.mip) / props.chunkSize;
    texel.y = (virtualLocY >> node.mip) / props.chunkSize;

    // idk how to avoid this, we can't index with not constant in glsl
    switch (node.mip) 
    {
        case 0: imageStore(indirectionTexture[0], texel, uvec4(node.mip, 0u, 0u, 0u)); break;
        case 1: imageStore(indirectionTexture[1], texel, uvec4(node.mip, 0u, 0u, 0u)); break;
        case 2: imageStore(indirectionTexture[2], texel, uvec4(node.mip, 0u, 0u, 0u)); break;
        case 3: imageStore(indirectionTexture[3], texel, uvec4(node.mip, 0u, 0u, 0u)); break;
        case 4: imageStore(indirectionTexture[4], texel, uvec4(node.mip, 0u, 0u, 0u)); break;
        case 5: imageStore(indirectionTexture[5], texel, uvec4(node.mip, 0u, 0u, 0u)); break;
    }
}