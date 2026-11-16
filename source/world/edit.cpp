#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Checks the chunk’s point ID list before allowing the point to be reclaimed.
		 */
		bool pointIsReferencedByChunk(const WorldData& data, const Chunk& chunk, PointId point_id)
		{
			for(GroundTriangleId triangle_id : chunk.ground_triangle_ids)
			{
				if(!data.isGroundTriangleActive(triangle_id)) continue;
				const GroundTriangle& triangle = data.getGroundTriangle(triangle_id);
				for(PointId triangle_point_id : triangle.points)
				{
					if(triangle_point_id == point_id) return true;
				}
			}
			for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
			{
				if(!data.isPointComponentSlotActive(slot_id)) continue;
				if(data.getPointComponentSlot(slot_id).point_id == point_id) return true;
			}
			for(GraphComponentSlotId slot_id : chunk.graph_component_slot_ids)
			{
				if(!data.isGraphComponentSlotActive(slot_id)) continue;
				const GraphComponentSlot& graph = data.getGraphComponentSlot(slot_id);
				if(graph.point_a == point_id || graph.point_b == point_id) return true;
			}
			return false;
		}
	}

	/**
	 * @brief Deletes selected mesh faces or applies subtractive edits to planet terrain.
	 * @param faces Face handles to process; inactive chunks/triangles are ignored and repeated
	 *        triangle IDs are processed once.
	 * Planet faces add a radius-6 m subtractive terrain stamp because their mesh is generated
	 * from a scalar field. For other bodies the triangle is destroyed, then each of its points
	 * is reclaimed only when this chunk is its sole referencing chunk and no remaining triangle
	 * or component in the chunk uses it. The controller's face list and selected count are cleared
	 * after processing, including when every supplied handle was invalid.
	 */
	void World::deleteFaces(const DynamicArray<FacePtr>& faces)
	{
		DynamicArray<GroundTriangleId> handled_triangle_ids;
		for(const FacePtr& face : faces)
		{
			bool already_handled = false;
			for(GroundTriangleId handled_id : handled_triangle_ids)
			{
				if(handled_id == face.ground_triangle_id)
				{
					already_handled = true;
					break;
				}
			}
			if(already_handled || !data.isChunkActive(face.chunk_id)
				|| !data.isGroundTriangleActive(face.ground_triangle_id)) continue;

			if(editPlanetGround(*this, face, 6.0, false))
			{
				handled_triangle_ids.push(face.ground_triangle_id);
				continue;
			}

			const GroundTriangle triangle = data.getGroundTriangle(face.ground_triangle_id);
			data.destroyGroundTriangle(face.ground_triangle_id);
			handled_triangle_ids.push(face.ground_triangle_id);
			const Chunk& chunk = data.getChunk(face.chunk_id);
			for(PointId point_id : triangle.points)
			{
				if(!data.isPointActive(point_id)
					|| data.getPoint(point_id).referencing_chunk_count != 1
					|| pointIsReferencedByChunk(data, chunk, point_id)) continue;
				data.destroyPoint(point_id);
			}
		}

		LocalPlayerControllerComponent& controller = getLocalPlayerControllerComponent();
		controller.selected_faces = DynamicArray<FacePtr>();
		controller.selected_face_count = 0;
	}
}
