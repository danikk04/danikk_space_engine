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
			{{0, 1, 5}}, {{0, 5, 4}},
			{{1, 2, 6}}, {{1, 6, 5}},
			{{2, 3, 7}}, {{2, 7, 6}},
			{{3, 0, 4}}, {{3, 4, 7}}
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

	// Extracts a face's boundary loop from its triangle topology.
	// Cancels reversed internal triangle edges, then walks the remaining directed edges
	// to return perimeter point IDs in winding order for extrusion and face matching.
	uint8 getVolumeCellFacePointIds(const VolumeCell& cell, uint8 face_index, PointId point_ids[8])
	{
		const VolumeCellTopologyDefinition& topology = getVolumeCellTopologyDefinition(cell.topology);
		if(topology.triangle_face_indices == nullptr)
		{
			return 0;
		}

		uint8 edge_start[24]{};
		uint8 edge_end[24]{};
		uint8 edge_count = 0;
		for(uint8 triangle_index = 0; triangle_index < topology.triangle_count; triangle_index++)
		{
			if(topology.triangle_face_indices[triangle_index] != face_index)
			{
				continue;
			}

			const VolumeCellTriangle& triangle = topology.triangles[triangle_index];
			for(uint8 edge_index = 0; edge_index < 3; edge_index++)
			{
				const uint8 from = triangle.points[edge_index];
				const uint8 to = triangle.points[(edge_index + 1) % 3];
				bool cancelled = false;
				for(uint8 i = 0; i < edge_count; i++)
				{
					if(edge_start[i] == to && edge_end[i] == from)
					{
						edge_count--;
						edge_start[i] = edge_start[edge_count];
						edge_end[i] = edge_end[edge_count];
						cancelled = true;
						break;
					}
				}
				if(!cancelled)
				{
					edge_start[edge_count] = from;
					edge_end[edge_count] = to;
					edge_count++;
				}
			}
		}

		if(edge_count < 3 || edge_count > 8)
		{
			return 0;
		}

		bool used_edges[24]{};
		uint8 current_point = edge_start[0];
		for(uint8 point_index = 0; point_index < edge_count; point_index++)
		{
			point_ids[point_index] = cell.points[current_point];
			bool next_edge_found = false;
			for(uint8 edge_index = 0; edge_index < edge_count; edge_index++)
			{
				if(!used_edges[edge_index] && edge_start[edge_index] == current_point)
				{
					used_edges[edge_index] = true;
					current_point = edge_end[edge_index];
					next_edge_found = true;
					break;
				}
			}
			if(!next_edge_found)
			{
				return 0;
			}
		}
		return edge_count;
	}

	bool volumeCellFacesSharePoints(const VolumeCell& cell_a, uint8 face_a, const VolumeCell& cell_b, uint8 face_b)
	{
		PointId point_ids_a[8]{};
		PointId point_ids_b[8]{};
		const uint8 point_count_a = getVolumeCellFacePointIds(cell_a, face_a, point_ids_a);
		const uint8 point_count_b = getVolumeCellFacePointIds(cell_b, face_b, point_ids_b);
		if(point_count_a < 3 || point_count_a != point_count_b)
		{
			return false;
		}

		for(uint8 i = 0; i < point_count_a; i++)
		{
			bool found = false;
			for(uint8 j = 0; j < point_count_b; j++)
			{
				if(point_ids_a[i] == point_ids_b[j])
				{
					found = true;
					break;
				}
			}
			if(!found)
			{
				return false;
			}
		}
		return true;
	}
}
