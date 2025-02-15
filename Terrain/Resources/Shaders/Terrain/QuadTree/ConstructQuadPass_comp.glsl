#version 460

layout (set = 0, binding = 0, r8ui) uniform readonly uimage2D statusTexture[6];
layout (set = 0, binding = 1, r32ui) uniform readonly uimage2D indirectionTexture[6];

struct NodeMetadata
{
    uint Offset;
    uint Lod;
};

struct GPUQuadTreeRenderChunk
{
    uint Offset;
    uint Lod;
    uint ChunkPhysicalLocation;
    uint NeighboursLod;
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
layout(std430, set = 1, binding = 2) writeonly buffer _FinalResult
{
    GPUQuadTreeRenderChunk nodes[];
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
    texel.x = int(node.Offset & 0x0000ffffu);
    texel.y = int(node.Offset & 0xffff0000u) >> 16;

    uint child0 = 0;
    uint child1 = 0;
    uint child2 = 0;
    uint child3 = 0;

    ivec2 childTexel = ivec2(texel.x * 2, texel.y * 2);

    uint lod = node.Lod & 0x0000ffff;

    switch (lod) 
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

        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos0, node.Lod - 1);
        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos1, node.Lod - 1);
        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos2, node.Lod - 1);
        TMPArray1.nodes[atomicAdd(metadata.TMPArray1Index, 1)] = NodeMetadata(pos3, node.Lod - 1);
        
        atomicMax(metadata.LoadedSize, metadata.TMPArray1Index);
    }
    // else push the node, we can t subdivide it
    else
    {
        uint chunkPhysicalLocation = 0;
        switch (node.Lod) 
        {
            case 0: chunkPhysicalLocation = imageLoad(indirectionTexture[0], texel).r; break;
            case 1: chunkPhysicalLocation = imageLoad(indirectionTexture[1], texel).r; break;
            case 2: chunkPhysicalLocation = imageLoad(indirectionTexture[2], texel).r; break;
            case 3: chunkPhysicalLocation = imageLoad(indirectionTexture[3], texel).r; break;
            case 4: chunkPhysicalLocation = imageLoad(indirectionTexture[4], texel).r; break;
            case 5: chunkPhysicalLocation = imageLoad(indirectionTexture[5], texel).r; break;
        }

        GPUQuadTreeRenderChunk renderNode;
        renderNode.Offset = node.Offset;
        renderNode.Lod = node.Lod;
        renderNode.ChunkPhysicalLocation = chunkPhysicalLocation;
        renderNode.NeighboursLod = 0;

        uint index = atomicAdd(metadata.ResultArrayIndex, 1);
        FinalResult.nodes[index] = renderNode;
        atomicAdd(metadata.LoadedSize, -1);
    }
}
