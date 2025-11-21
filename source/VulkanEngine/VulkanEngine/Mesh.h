#pragma once
#include "engineTypes.h"
#include "VulkanBackend.h"
#include <vector>
#include <array>

// Just like a pixel is a 'picture-element', and a voxel is a 'volume-element', a mexel is a 'mesh-element'
// They are the individual mesh pieces with separate materials that make up a mesh
struct Mexel
{
	zstring<CHAR_T>* Filename;
	uint32_t startingVertex;
	uint32_t startingIndex;
	uint32_t IndexBufferLength;
	uint32_t mexelIndex;

	float3 boundingBoxMin;
	float3 boundingBoxMax;
	float3 boundingBoxCentre;
};

struct Vertex {
	float3 pos;
	float3 nrm;
	float3 tangent;
	float4 uv;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	// In order to change the attributes for each vertex, update the array length and fill in the added entry.
	static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

		// The format parameter describes the type of data for the attribute.
		// A bit confusingly, the formats are specified using the same enumeration as color formats.
		// The following shader types and formats are commonly used together:

		// float: VK_FORMAT_R32_SFLOAT
		// vec2 : VK_FORMAT_R32G32_SFLOAT
		// vec3 : VK_FORMAT_R32G32B32_SFLOAT
		// vec4 : VK_FORMAT_R32G32B32A32_SFLOAT

		// As you can see, you should use the format where the amount of color channels matches the number of components in the shader data type.
		// It is allowed to use more channels than the number of components in the shader, but they will be silently discarded.
		// If the number of channels is lower than the number of components, then the BGA components will use default values of (0, 0, 1).
		// The color type(SFLOAT, UINT, SINT) and bit width should also match the type of the shader input.
		// See the following examples :

		// ivec2: VK_FORMAT_R32G32_SINT, a 2 - component vector of 32 - bit signed integers
		// uvec4 : VK_FORMAT_R32G32B32A32_UINT, a 4 - component vector of 32 - bit unsigned integers
		// double : VK_FORMAT_R64_SFLOAT, a double - precision(64 - bit) float

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, nrm);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, uv);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, tangent);

		return attributeDescriptions;
	}
};

void BindVertexAndIndexBuffer(VkCommandBuffer commandBuffer);

class Mesh
{
public:
	zstring<char>* name;
	std::vector<Mexel*> mexels;
	SDF* sdf;
	float3 boundingBoxMin;
	float3 boundingBoxMax;
	float3 boundingBoxCentre;

	static inline VulkanMemory* allVertexBuffer;
	static inline VulkanMemory* allIndexBuffer;
	static inline std::vector<Vertex> allVertices;
	static inline std::vector<uint32_t> allIndices;

	Mesh(const char* meshName);
	~Mesh();

	static Mesh* LoadMesh(const char* name);
	static std::vector<Mesh*>& GetAllMeshes();
	static void DeleteAllMeshes();

	static void CreateAllVertexBuffer();
	static void CreateAllIndexBuffer();
};