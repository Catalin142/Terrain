#version 460
// get all nodes loaded this frame
// blit them in the indirection texture

// store each mip
layout (binding = 0, r8ui) uniform writeonly uimage2D loadStateTexture[6];

struct StatusNode
{
    uint location;
    uint mip;
};

layout(set = 1, binding = 0) readonly buffer NodesUniformBuffer
{
    StatusNode lNode[1024];
} Nodes;

layout (push_constant) uniform VirtualMapProperties
{
    int loadedNodesCount;
} props;

layout(local_size_x = 32, local_size_y = 1) in;
void main()
{
    if (gl_GlobalInvocationID.x >= props.loadedNodesCount)
        return;

    StatusNode node = Nodes.lNode[gl_GlobalInvocationID.x];

    ivec2 texel;
    texel.x = int(node.location & 0x0000FFFF);
    texel.y = int(node.location & 0xFFFF0000) >> 16;

    uint mip = node.mip & 0x0000ffff;
    uint status = node.mip & 0x00010000;
    status = status >> 16;

    switch (mip) 
    {
        case 0: imageStore(loadStateTexture[0], texel, uvec4(status, 0u, 0u, 0u)); break;
        case 1: imageStore(loadStateTexture[1], texel, uvec4(status, 0u, 0u, 0u)); break;
        case 2: imageStore(loadStateTexture[2], texel, uvec4(status, 0u, 0u, 0u)); break;
        case 3: imageStore(loadStateTexture[3], texel, uvec4(status, 0u, 0u, 0u)); break;
        case 4: imageStore(loadStateTexture[4], texel, uvec4(status, 0u, 0u, 0u)); break;
        case 5: imageStore(loadStateTexture[5], texel, uvec4(status, 0u, 0u, 0u)); break;
    }
}