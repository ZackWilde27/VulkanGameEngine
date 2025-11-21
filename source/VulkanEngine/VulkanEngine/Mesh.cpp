#include "Mesh.h"
#include "engine.h"
#include "VulkanMemory.h"

std::vector<Mexel*> allMexels;
std::vector<Mesh*> allMeshes;

std::vector<Mesh*>& Mesh::GetAllMeshes()
{
	return allMeshes;
}

void Mesh::DeleteAllMeshes()
{
	for (size_t i = 0; i < allMexels.size(); i++)
		free((void*)allMexels[i]->Filename);

	allMeshes.clear();
}

VkDeviceSize vertexBufferOffset = 0;

void BindVertexAndIndexBuffer(VkCommandBuffer commandBuffer)
{
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &Mesh::allVertexBuffer->buffer, &vertexBufferOffset);
	vkCmdBindIndexBuffer(commandBuffer, Mesh::allIndexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

// Returns the starting index into the vertex buffer
static uint32_t AddVertexBuffer(Vertex* vertices, uint32_t numVerts)
{
	uint32_t oldSize = (uint32_t)Mesh::allVertices.size();
	Mesh::allVertices.resize(oldSize + numVerts);

	VkDeviceSize bufferSize = numVerts * sizeof(Vertex);

	memcpy(&Mesh::allVertices[oldSize], vertices, bufferSize);

	return oldSize;
}

static uint32_t AddIndexBuffer16(uint16_t* indices, uint32_t numIndices)
{
	uint32_t startingIndex = (uint32_t)Mesh::allIndices.size();
	Mesh::allIndices.resize(startingIndex + numIndices);

	for (size_t i = 0; i < numIndices; i++)
		Mesh::allIndices[i + startingIndex] = (uint32_t)indices[i];

	return startingIndex;
}

static uint32_t AddIndexBuffer32(uint32_t* indices, uint32_t numIndices)
{
	uint32_t startingIndex = (uint32_t)Mesh::allIndices.size();
	Mesh::allIndices.resize(startingIndex + numIndices);

	memcpy(&Mesh::allIndices[startingIndex], indices, numIndices * sizeof(uint32_t));

	return startingIndex;
}

// It's more like CreateMesh, it does the vertex and index buffer
static Mexel CreateVertexBuffer(void* vertices, uint32_t numVerts, void* indices, uint32_t numIndices, BYTE indexSize)
{
	Mexel mesh{};

	mesh.startingVertex = AddVertexBuffer((Vertex*)vertices, numVerts);

	if (indexSize == 2)
		mesh.startingIndex = AddIndexBuffer16((uint16_t*)indices, numIndices);
	else
		mesh.startingIndex = AddIndexBuffer32((uint32_t*)indices, numIndices);

	mesh.IndexBufferLength = numIndices;

	return mesh;
}

void Mesh::CreateAllVertexBuffer()
{
	if (allVertexBuffer) delete allVertexBuffer;

	allVertexBuffer = new VulkanMemory(allVertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "VertexBuffer", true, allVertices.data());
}

void Mesh::CreateAllIndexBuffer()
{
	if (allIndexBuffer) delete allIndexBuffer;

	allIndexBuffer = new VulkanMemory(allIndices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "IndexBuffer", true, allIndices.data());
}

static Mexel* LoadMexelFromBuffer(char* buffer, char** endPtr, VulkanBackend* backend)
{
	auto mesh = NEW(Mexel);
	check(mesh, "Failed to allocate mexel");

	mesh->Filename = NULL;

	Vertex* vertices;
	void* indices;

	char* ptr = buffer;

	// 1 = 32-bit indices, 0 = 16-bit indices
	BYTE meshType = *ptr++;

	unsigned int numVerts;

	if (meshType)
	{
		numVerts = IncReadAs(ptr, unsigned int);
	}
	else
	{
		numVerts = IncReadAs(ptr, uint16_t);
	}

	vertices = (Vertex*)ptr;
	ptr += numVerts * sizeof(Vertex);

	float3 boundingBoxMin = vertices[0].pos;
	float3 boundingBoxMax = vertices[0].pos;

	for (uint32_t i = 1; i < numVerts; i++)
	{
		boundingBoxMin = glm::min(boundingBoxMin, vertices[i].pos);
		boundingBoxMax = glm::max(boundingBoxMax, vertices[i].pos);
	}

	float3 boundingBoxCentre = ((boundingBoxMax - boundingBoxMin) / float3(2)) + boundingBoxMin;

	uint32_t numIndices;
	BYTE indexSize;

	if (meshType)
	{
		numIndices = IncReadAs(ptr, unsigned int);
		indexSize = 4;
	}
	else
	{
		numIndices = IncReadAs(ptr, uint16_t);
		indexSize = 2;
	}

	indices = ptr;
	ptr += numIndices * indexSize;

	check(numVerts, "No vertices read!");
	check(numIndices, "No indices read!");

	allMexels.push_back(mesh);
	*mesh = CreateVertexBuffer(vertices, numVerts, indices, numIndices, indexSize);

	mesh->boundingBoxMin = boundingBoxMin;
	mesh->boundingBoxMax = boundingBoxMax;
	mesh->boundingBoxCentre = boundingBoxCentre;
	ptr += sizeof(float) * 3;

	if (endPtr)
		*endPtr = ptr;

	backend->stats.triangle_count += numIndices / 3;
	return mesh;
}

static Mexel* LoadMexelFromFile(CHAR_T* filename)
{
	if (!FileExists(filename))
	{
		std::cout << "Mexel file: " << filename << " does not exist!" << "\n";
		free(filename);
		return NULL;
	}

	for (int i = 0; i < allMexels.size(); i++)
	{
		if (allMexels[i]->Filename && WStringCompare(allMexels[i]->Filename->Ptr(), filename))
			return allMexels[i];
	}

	auto data = readFile(filename);

	Mexel* mexel = LoadMexelFromBuffer(data.data(), NULL, GetEngine()->backend);
	mexel->Filename = new zstring((const CHAR_T*)filename);

	return mexel;
}

Mesh* Mesh::LoadMesh(const char* name)
{
	VulkanBackend* backend = GetEngine()->backend;

	for (size_t i = 0; i < allMeshes.size(); i++)
	{
		if (*allMeshes[i]->name == name)
			return allMeshes[i];
	}

	allMeshes.push_back(new Mesh(name));

	if (backend->setup)
	{
		vkDeviceWaitIdle(backend->logicalDevice);
		backend->OnLevelLoad();
	}

	return allMeshes.back();
}

Mesh::Mesh(const char* meshName)
{
	int num = 0;
	zstring<CHAR_T>* filename;

	name = new zstring(meshName);

	mexels = {};

	while (true)
	{
#ifdef WIDE_STRINGS
		filename = new zstring(L"models/%hs_%i.msh", meshName, num++);
#else
		filename = new zstring("models/%s_%i.msh", meshName, num++);
#endif

		if (!FileExists(filename->Ptr()))
		{
			delete filename;

			if (!mexels.size())
			{
				printf("Mesh: %s does not exist!\n", meshName);
				return;
			}

			break;
		}

		mexels.push_back(LoadMexelFromFile(filename->Ptr()));

		delete filename;
	}

	boundingBoxMin = mexels[0]->boundingBoxMin;
	boundingBoxMax = mexels[0]->boundingBoxMax;

	for (size_t i = 1; i < mexels.size(); i++)
	{
		boundingBoxMax = glm::max(boundingBoxMax, mexels[i]->boundingBoxMax);
		boundingBoxMin = glm::min(boundingBoxMin, mexels[i]->boundingBoxMin);
	}

	boundingBoxCentre = (boundingBoxMax - boundingBoxMin) * 0.5f + boundingBoxMin;

	sdf = NULL;// new SDF(this, backend);
}


Mesh::~Mesh()
{
	delete sdf;
}