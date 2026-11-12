#include <danikk_space_engine/volume_cell.h>

namespace danikk_space_engine
{
	namespace
	{
		constexpr VolumeCellTriangle tetrahedron_triangles[] =
		{
			{{0, 2, 1}},
			{{0, 1, 3}},
			{{1, 2, 3}},
			{{2, 0, 3}}
		};

		constexpr VolumeCellTriangle triangular_prism_triangles[] =
		{
			{{0, 2, 1}}, {{3, 4, 5}},
			{{0, 1, 4}}, {{0, 4, 3}},
			{{1, 2, 5}}, {{1, 5, 4}},
			{{2, 0, 3}}, {{2, 3, 5}}
		};

		constexpr VolumeCellTriangle triangular_bipyramid_triangles[] =
		{
			{{3, 0, 1}}, {{3, 1, 2}}, {{3, 2, 0}},
			{{4, 1, 0}}, {{4, 2, 1}}, {{4, 0, 2}}
		};

		constexpr VolumeCellTriangle octahedron_triangles[] =
		{
			{{4, 0, 1}}, {{4, 1, 2}}, {{4, 2, 3}}, {{4, 3, 0}},
			{{5, 1, 0}}, {{5, 2, 1}}, {{5, 3, 2}}, {{5, 0, 3}}
		};

		constexpr VolumeCellTriangle hexahedron_triangles[] =
		{
			{{0, 2, 1}}, {{0, 3, 2}},
			{{4, 5, 6}}, {{4, 6, 7}},
			{{0, 4, 5}}, {{0, 5, 1}},
			{{1, 5, 6}}, {{1, 6, 2}},
			{{2, 6, 7}}, {{2, 7, 3}},
			{{3, 7, 4}}, {{3, 4, 0}}
		};

		constexpr uint8 tetrahedron_triangle_faces[] = {0, 1, 2, 3};
		constexpr uint8 triangular_prism_triangle_faces[] = {0, 1, 2, 2, 3, 3, 4, 4};
		constexpr uint8 triangular_bipyramid_triangle_faces[] = {0, 1, 2, 3, 4, 5};
		constexpr uint8 octahedron_triangle_faces[] = {0, 1, 2, 3, 4, 5, 6, 7};
		constexpr uint8 hexahedron_triangle_faces[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5};

		const VolumeCellTopologyDefinition topology_definitions[] =
		{
			{0, 0, 0, nullptr, nullptr},
			{4, 4, 4, tetrahedron_triangles, tetrahedron_triangle_faces},
			{6, 8, 5, triangular_prism_triangles, triangular_prism_triangle_faces},
			{5, 6, 6, triangular_bipyramid_triangles, triangular_bipyramid_triangle_faces},
			{6, 8, 8, octahedron_triangles, octahedron_triangle_faces},
			{8, 12, 6, hexahedron_triangles, hexahedron_triangle_faces}
		};
	}

	const VolumeCellTopologyDefinition& getVolumeCellTopologyDefinition(VolumeCellTopology topology)
	{
		const uint8 topology_index = static_cast<uint8>(topology);
		if(topology_index >= static_cast<uint8>(VolumeCellTopology::count))
		{
			return topology_definitions[static_cast<uint8>(VolumeCellTopology::none)];
		}
		return topology_definitions[topology_index];
	}
}
