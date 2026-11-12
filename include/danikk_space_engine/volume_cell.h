#pragma once

#include <danikk_space_engine/material.h>
#include <danikk_space_engine/point.h>

namespace danikk_space_engine
{
	enum class VolumeCellTopology : uint8
	{
		none = 0,
		tetrahedron,
		triangular_prism,
		triangular_bipyramid,
		octahedron,
		hexahedron,
		count
	};

	struct VolumeCellTriangle
	{
		uint8 points[3];
	};

	struct VolumeCellTopologyDefinition
	{
		uint8 point_count;
		uint8 triangle_count;
		uint8 face_count;
		const VolumeCellTriangle* triangles;
		const uint8* triangle_face_indices;
	};

	struct VolumeCell
	{
		PointId points[8]{};
		// Vertex order: prism has bottom 0..2 and corresponding top 3..5;
		// bipyramid has equator 0..2 and apices 3,4; octahedron has equator
		// 0..3 and apices 4,5; hexahedron has cyclic bottom 0..3 and
		// corresponding top 4..7. Tetrahedron uses base 0..2 and apex 3.
		VolumeCellTopology topology = VolumeCellTopology::none;
		MaterialId material = default_materials::void_material;
		float mass = 0.0f;
		float temperature = 0.0f;
		float health = 0.0f;
	};

	const VolumeCellTopologyDefinition& getVolumeCellTopologyDefinition(VolumeCellTopology topology);
}
