#version 450

// maybe in the future ill need to make this more general TODO

layout(push_constant) uniform IndexCount
{
	int indexCount;
};

struct VkDrawIndexedIndirectCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct PassMetadata
{
    uint TMPArray1Index;
    uint ResultArrayIndex;
    uint LoadedSize;
};

layout(std430, set = 0, binding = 0) writeonly buffer _DrawCommand
{
    VkDrawIndexedIndirectCommand drawCommand;
};

layout(std430, set = 0, binding = 1) buffer _Metadata {
    PassMetadata metadata;
};

layout(local_size_x = 1, local_size_y = 1) in;
void main()
{
    drawCommand.indexCount = indexCount;
    drawCommand.instanceCount = metadata.ResultArrayIndex;
    drawCommand.firstIndex = 0;
    drawCommand.vertexOffset = 0;
    drawCommand.firstInstance = 0;
}