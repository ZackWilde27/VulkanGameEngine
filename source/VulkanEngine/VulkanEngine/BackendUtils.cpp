#include "BackendUtils.h"

float4x4 WorldMatrix(float3 pos, float3 rot, float3 scale)
{
	float4x4 matrix = glm::identity<float4x4>();

	matrix = glm::translate(matrix, pos);

	matrix = glm::rotate(matrix, rot.z, float3(0.0f, 0.0f, 1.0f));
	matrix = glm::rotate(matrix, rot.y, float3(0.0f, 1.0f, 0.0f));
	matrix = glm::rotate(matrix, rot.x, float3(1.0f, 0.0f, 0.0f));

	matrix = glm::scale(matrix, scale);

	return matrix;
}

#define NUMDIM	3
#define RIGHT	0
#define LEFT	1
#define MIDDLE	2

bool HitBoundingBox(float3 minB, float3 maxB, float3 origin, float3 dir, float3& coord)
{
	char inside = true;
	char quadrant[NUMDIM];
	register int i;
	int whichPlane;
	double maxT[NUMDIM];
	double candidatePlane[NUMDIM];

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