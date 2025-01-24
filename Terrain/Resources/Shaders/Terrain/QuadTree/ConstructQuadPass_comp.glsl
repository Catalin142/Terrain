#version 460

layout (binding = 0, r32ui) uniform readonly uimage2D statusTexture[6];

struct NodeMetadata
{
    uint location;
    uint mip;
};

// this holds the nodes that we curently process
layout(std430, set = 1, binding = 0) readonly buffer _TMPArray0Set
{
    NodeMetadata nodes[];
} TMPArray0;

// this holds the nodes that will be processed next iteration of the compute
layout(std430, set = 1, binding = 1) writeonly buffer _TMPArray1Set
{
    NodeMetadata nodes[];
} TMPArray1;

// this holds the nodes we will render
layout(std430, set = 1, binding = 2) writeonly buffer _FinalResultSet
{
    NodeMetadata nodes[];
} FinalResult;

struct PassMetadata
{
    uint TMPArray1Index;
    uint ResultArrayIndex;
    uint LoadedSize;
};

layout(std430, set = 2, binding = 0) buffer _MetadataSet {
    PassMetadata metadata;
};

shared uint loadedSize;

layout(local_size_x = 256, local_size_y = 1) in;
void main()
{
    loadedSize = metadata.LoadedSize;
    if (gl_GlobalInvocationID.x >= loadedSize)
        return;

    NodeMetadata node = TMPArray0.nodes[gl_GlobalInvocationID.x];
    
    ivec2 texel;
    texel.x = int(node.location & 0x0000FFFF);
    texel.y = int(node.location & 0xFFFF0000) >> 16;

    uint child0 = 0;
    uint child1 = 0;
    uint child2 = 0;
    uint child3 = 0;

    ivec2 childTexel = ivec2(texel.x * 2, texel.y * 2);

    uint mip = node.mip & 0x0000ffff;

    // each thread in the dispatch will take the same path, the gpu will optimizie this i hope!
    switch (mip) 
    {
        case 0: 
            break;

        case 1: 
            child0 = imageLoad(statusTexture[0], ivec2(childTexel.x,     childTexel.y)).x; 
            child1 = imageLoad(statusTexture[0], ivec2(childTexel.x + 1, childTexel.y)).x; 
            child2 = imageLoad(statusTexture[0], ivec2(childTexel.x,     childTexel.y + 1)).x; 
            child3 = imageLoad(statusTexture[0], ivec2(childTexel.x + 1, childTexel.y + 1)).x; 
            break;

        case 2: 
            child0 = imageLoad(statusTexture[1], ivec2(childTexel.x,     childTexel.y)).x; 
            child1 = imageLoad(statusTexture[1], ivec2(childTexel.x + 1, childTexel.y)).x; 
            child2 = imageLoad(statusTexture[1], ivec2(childTexel.x,     childTexel.y + 1)).x; 
            child3 = imageLoad(statusTexture[1], ivec2(childTexel.x + 1, childTexel.y + 1)).x; 
            break;

        case 3: 
            child0 = imageLoad(statusTexture[2], ivec2(childTexel.x,     childTexel.y)).x;  
            child1 = imageLoad(statusTexture[2], ivec2(childTexel.x + 1, childTexel.y)).x; 
            child2 = imageLoad(statusTexture[2], ivec2(childTexel.x,     childTexel.y + 1)).x; 
            child3 = imageLoad(statusTexture[2], ivec2(childTexel.x + 1, childTexel.y + 1)).x; 
            break;

        case 4: 
            child0 = imageLoad(statusTexture[3], ivec2(childTexel.x,     childTexel.y)).x;  
            child1 = imageLoad(statusTexture[3], ivec2(childTexel.x + 1, childTexel.y)).x; 
            child2 = imageLoad(statusTexture[3], ivec2(childTexel.x,     childTexel.y + 1)).x; 
            child3 = imageLoad(statusTexture[3], ivec2(childTexel.x + 1, childTexel.y + 1)).x; 
            break;

        case 5: 
            child0 = imageLoad(statusTexture[4], ivec2(childTexel.x,     childTexel.y)).x;  
            child1 = imageLoad(statusTexture[4], ivec2(childTexel.x + 1, childTexel.y)).x; 
            child2 = imageLoad(statusTexture[4], ivec2(childTexel.x,     childTexel.y + 1)).x; 
            child3 = imageLoad(statusTexture[4], ivec2(childTexel.x + 1, childTexel.y + 1)).x; 
            break;
    }

    uint loadCount = child0 + child1 + child2 + child3;
    // if all the children are loaded, push the children for the next pass
    if (loadCount == 4)
    {
        uint pos0, pos1, pos2, pos3;
        pos0 = (childTexel.x & 0x0000ffff)       | ((childTexel.y & 0x0000ffff) << 16);
        pos1 = ((childTexel.x + 1) & 0x0000ffff) | ((childTexel.y & 0x0000ffff) << 16);
        pos2 = ((childTexel.x + 1) & 0x0000ffff) | (((childTexel.y + 1) & 0x0000ffff) << 16);
        pos3 = (childTexel.x & 0x0000ffff)       | (((childTexel.y + 1) & 0x0000ffff) << 16);

        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos0, node.mip - 1);
        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos1, node.mip - 1);
        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos2, node.mip - 1);
        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos3, node.mip - 1);
        
        atomicMax(metadata.LoadedSize, metadata.TMPArray1Index);
    }
    // else push the node, we can t subdivide it
    else
    {
        uint index = atomicAdd(metadata.ResultArrayIndex, 1);
        FinalResult.nodes[index] = node;
        atomicAdd(metadata.LoadedSize, -1);
    }
}
