#include "engineTypes.h"
#include "VulkanBackend.h"
#include "BackendUtils.h"
#include "Mesh.h"
#include "Texture.h"

constexpr float DOUBLEPI = 3.14159f * 2;
constexpr size_t SDF_SPHERE_RESOLUTION = 45;
constexpr float SDF_SPHERE_RESOLUTION_RADS = DOUBLEPI / SDF_SPHERE_RESOLUTION;
constexpr float SDF_RESOLUTION = 8;

static float3 PointOnSphereFromAngles(float yaw, float pitch)
{
	float x2 = cos(pitch);
	float y2 = sin(pitch);

	float x1 = sin(yaw) * x2;
	float y1 = cos(yaw) * x2;

	return float3(x1, y1, y2);
}

static void CreateSDF(Mesh* mesh, float* buffer, uint3& extent, VulkanBackend* backend)
{
	uint32_t size = extent.x * extent.y * extent.z;
	float3 normal;

	for (uint32_t i = 0; i < size; i++)
	{
		uint32_t x = i % extent.x;
		uint32_t y = (i / extent.x) % extent.y;
		uint32_t z = (i / extent.x / extent.y) % extent.z;

		float3 samplePoint = float3(x, y, z) / SDF_RESOLUTION + mesh->boundingBoxMin;
		double distance;
		double closest = 9999999.f;

		// For each voxel of the SDF, it'll shoot a ray in as many directions as possible to get the closest point on the mesh
		// It'll be slow, but it should work
		for (float yaw = 0; yaw < DOUBLEPI; yaw += SDF_SPHERE_RESOLUTION_RADS)
		{
			for (float pitch = 0; pitch < DOUBLEPI; pitch += SDF_SPHERE_RESOLUTION_RADS)
			{
				float3 dir = PointOnSphereFromAngles(yaw, pitch);
				if (HitMesh(samplePoint, dir, mesh, distance, normal))
				{
					if (distance < closest)
						closest = distance;
				}
			}
		}

		buffer[i] = closest;
	}
}

SDF::SDF(Mesh* mesh, VulkanBackend* backend)
{
	device = backend->logicalDevice;

	float3 boundingBoxMax = mesh->mexels[0]->boundingBoxMax;
	float3 boundingBoxMin = mesh->mexels[0]->boundingBoxMin;

	size_t numMexels = mesh->mexels.size();
	for (size_t i = 1; i < numMexels; i++)
	{
		boundingBoxMax = glm::max(boundingBoxMax, mesh->mexels[i]->boundingBoxMax);
		boundingBoxMin = glm::min(boundingBoxMin, mesh->mexels[i]->boundingBoxMin);
	}

	uint3 extent = glm::max((uint3)(boundingBoxMax - boundingBoxMin), uint3(1)) * (uint32_t)SDF_RESOLUTION;
	printf("%u, %u, %u\n", extent.x, extent.y, extent.z);
	size_t imageSize = (size_t)extent.x * extent.y * extent.z * sizeof(float);

	float* buffer = (float*)malloc(imageSize);
	check(buffer, "Failed to allocate memory for SDF!");

	CreateSDF(mesh, buffer, extent, backend);

	texture = new Texture(VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R32_SFLOAT, extent.x, extent.y, extent.z, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, true, backend);
	texture->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	texture->CopyFromBuffer(buffer, imageSize);

	free(buffer);
}

SDF::~SDF()
{
	delete texture;
}