#include "BackendUtils.h"
#include "engine.h"
#include "VulkanBackend/Mesh.h"
#include <vector>



float4x4 RotateMatrix(float4x4& matrix, float3& rotation)
{
	float4x4 newMatrix = glm::rotate(matrix, rotation.z, float3(0.0f, 0.0f, 1.0f));
	newMatrix = glm::rotate(newMatrix, rotation.y, float3(0.0f, 1.0f, 0.0f));
	newMatrix = glm::rotate(newMatrix, rotation.x, float3(1.0f, 0.0f, 0.0f));
	return newMatrix;
}

float4x4 RotationMatrix(float3& rotation)
{
	float4x4 matrix = glm::identity<float4x4>();
	return RotateMatrix(matrix, rotation);
}

float4x4 WorldMatrix(float3& pos)
{
	return glm::translate(glm::identity<float4x4>(), pos);
}

float4x4 WorldMatrix(float3& pos, float3& scale)
{
	return glm::scale(WorldMatrix(pos), scale);
}

float4x4 WorldMatrix(float3& pos, float3& rot, float3& scale)
{
	float4x4 matrix = WorldMatrix(pos);
	return glm::scale(RotateMatrix(matrix, rot), scale);
}

#define NUMDIM	3
#define RIGHT	0
#define LEFT	1
#define MIDDLE	2

bool HitBoundingBox(float3& minB, float3& maxB, float3& origin, float3& dir, float3& coord)
{
	char inside = true;
	char quadrant[NUMDIM];
	int i;
	int whichPlane;
	float maxT[NUMDIM];
	float candidatePlane[NUMDIM];

	/* Find candidate planes; this loop can be avoided if
	rays cast all from the eye(assume perpsective view) */
	for (i = 0; i < NUMDIM; i++)
		if (origin[i] < minB[i]) {
			quadrant[i] = LEFT;
			candidatePlane[i] = minB[i];
			inside = false;
		}
		else if (origin[i] > maxB[i]) {
			quadrant[i] = RIGHT;
			candidatePlane[i] = maxB[i];
			inside = false;
		}
		else {
			quadrant[i] = MIDDLE;
		}

	/* Ray origin inside bounding box */
	if (inside) {
		coord = origin;
		return (true);
	}


	/* Calculate T distances to candidate planes */
	for (i = 0; i < NUMDIM; i++)
		if (quadrant[i] != MIDDLE && dir[i] != 0.)
			maxT[i] = (candidatePlane[i] - origin[i]) / dir[i];
		else
			maxT[i] = -1.;

	/* Get largest of the maxT's for final choice of intersection */
	whichPlane = 0;
	for (i = 1; i < NUMDIM; i++)
		if (maxT[whichPlane] < maxT[i])
			whichPlane = i;

	/* Check final candidate actually inside box */
	if (maxT[whichPlane] < 0.) return (false);
	for (i = 0; i < NUMDIM; i++)
		if (whichPlane != i) {
			coord[i] = origin[i] + maxT[whichPlane] * dir[i];
			if (coord[i] < minB[i] || coord[i] > maxB[i])
				return (false);
		}
		else {
			coord[i] = candidatePlane[i];
		}
	return (true);				/* ray hits box */
}

#define EPSILON 0.0001

// Tomas Moller Algorithm, edited a bit to work with the engine
VkBool32 intersect_triangle(float3 orig, float3 dir, float3 vert0, float3 vert1, float3 vert2, double* t, float3* normal)
{
	float3 edge1, edge2, tvec, pvec, qvec;
	double det, inv_det, u, v;

	edge1 = vert1 - vert0;
	edge2 = vert2 - vert0;

	*normal = glm::normalize(glm::cross(edge1, edge2));

	pvec = glm::cross(dir, edge2);

	det = glm::dot(edge1, pvec);

	if (det < EPSILON)
		return VK_FALSE;

	tvec = orig - vert0;
	u = glm::dot(tvec, pvec);
	if (u < 0 || u > det)
		return VK_FALSE;

	qvec = glm::cross(tvec, edge1);

	v = glm::dot(dir, qvec);
	if (v < 0 || u + v > det)
		return VK_FALSE;

	*t = glm::dot(edge2, qvec);
	inv_det = 1 / det;
	*t *= inv_det;

	if (*t < 0)
		return VK_FALSE;
	//u *= inv_det;
	//v *= inv_det;

	return VK_TRUE;
}

bool HitMesh(float3& rayOrigin, float3& rayDir, Mesh* mesh, double& out_distance, float3& out_normal)
{
	double distance;
	float3 normal;
	out_distance = 9999999.f;
	bool hit = false;

	float3 coord;

	if (HitBoundingBox(mesh->boundingBoxMin, mesh->boundingBoxMax, rayOrigin, rayDir, coord))
	{
		auto& verts = Mesh::allVertices;
		auto& indices = Mesh::allIndices;

		for (const auto m : mesh->mexels)
		{
			for (uint32_t i = 0; i < m->IndexBufferLength; i += 3)
			{
				float3 p1 = verts[indices[m->startingIndex + i] + m->startingVertex].pos;
				float3 p2 = verts[indices[m->startingIndex + i + 1] + m->startingVertex].pos;
				float3 p3 = verts[indices[m->startingIndex + i + 2] + m->startingVertex].pos;
				if (intersect_triangle(rayOrigin, rayDir, p1, p2, p3, &distance, &normal))
				{
					hit = true;

					if (distance < out_distance)
					{
						out_distance = distance;
						out_normal = normal;
					}
				}
			}
		}
	}

	return hit;
}