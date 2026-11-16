#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{

		/**
		 * @brief Checks whether an active chunk is owned by a body with a live Planet component.
		 * @param world World used to validate the chunk/body/component chain.
		 * @param chunk_id Chunk being classified.
		 * @return True only for an active planet chunk.
		 */
		bool isPlanetChunk(const World& world, ChunkId chunk_id)
		{
			if(!world.data.isChunkActive(chunk_id)) return false;
			const Chunk& chunk = world.data.getChunk(chunk_id);
			if(!world.data.isWorldBodyActive(chunk.world_body_id)) return false;
			const WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(world.data.isWorldBodyComponentSlotActive(slot_id)
					&& world.data.getWorldBodyComponentSlot(slot_id).type_id == WorldBodyComponentTypeId::planet)
				{
					return true;
				}
			}
			return false;
		}

		/**
		 * @brief Allocates a triangle, assigns its three point IDs, and registers it with a chunk.
		 * @param world WorldData owner of the new triangle.
		 * @param chunk_id Chunk receiving the triangle reference.
		 * @param a First vertex ID in winding order.
		 * @param b Second vertex ID in winding order.
		 * @param c Third vertex ID in winding order.
		 * @return ID of the created GroundTriangle.
		 */
		GroundTriangleId addTriangle(World& world, ChunkId chunk_id, PointId a, PointId b, PointId c)
		{
			const GroundTriangleId triangle_id = world.data.createGroundTriangle();
			GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
			triangle.points[0] = a;
			triangle.points[1] = b;
			triangle.points[2] = c;
			world.data.addGroundTriangleToChunk(chunk_id, triangle_id);
			return triangle_id;
		}
	}

	/**
	 * @brief Replaces one editable triangle with a translated cap and three triangulated side faces.
	 * @param world World whose chunk-local point and triangle pools are modified.
	 * @param face Source triangle to extrude; planet chunks are rejected.
	 * @param distance Translation distance along the stored triangle normal.
	 * @return Selection replacement containing the new cap when successful; an
	 *         empty result when the source is invalid, degenerate, or belongs to a planet.
	 * The source triangle is destroyed after its three offset points, cap, and six
	 * side triangles have been added. Winding is adjusted so the new prism faces
	 * point outward, and the owning body's broadphase collision radius is expanded.
	 */
	ToolApplyResult applyBodyExtrudeTool(World& world, const FacePtr& face, double distance)
	{
		if(!world.data.isChunkActive(face.chunk_id)
			|| !world.data.isGroundTriangleActive(face.ground_triangle_id)
			|| isPlanetChunk(world, face.chunk_id)) return ToolApplyResult();

		const Chunk& chunk = world.data.getChunk(face.chunk_id);
		const GroundTriangle original = world.data.getGroundTriangle(face.ground_triangle_id);
		if(original.chunk_id != face.chunk_id) return ToolApplyResult();
		const vec3 a = world.data.getPoint(original.points[0]).position;
		const vec3 b = world.data.getPoint(original.points[1]).position;
		const vec3 c = world.data.getPoint(original.points[2]).position;
		vec3 normal = glm::cross(b - a, c - a);
		const float normal_length = glm::length(normal);
		if(normal_length <= 0.000001f) return ToolApplyResult();
		normal /= normal_length;
		const vec3 offset = normal * static_cast<float>(distance);

		PointId upper[3]{};
		for(uint8 corner = 0; corner < 3; corner++)
		{
			upper[corner] = world.data.createPoint();
			world.data.getPoint(upper[corner]).position = world.data.getPoint(original.points[corner]).position + offset;
		}

		const GroundTriangleId cap_id = addTriangle(
			world, face.chunk_id, upper[0], upper[1], upper[2]);
		for(uint8 edge = 0; edge < 3; edge++)
		{
			const uint8 next = static_cast<uint8>((edge + 1) % 3);
			const PointId lower_a = original.points[edge];
			const PointId lower_b = original.points[next];
			const PointId upper_a = upper[edge];
			const PointId upper_b = upper[next];
			addTriangle(world, face.chunk_id, lower_a, lower_b, upper_b);
			addTriangle(world, face.chunk_id, lower_a, upper_b, upper_a);
		}
		world.data.destroyGroundTriangle(face.ground_triangle_id);

		WorldBody& body = world.data.getWorldBody(chunk.world_body_id);
		for(ChunkId body_chunk_id : body.chunk_ids)
		{
			if(!world.data.isChunkActive(body_chunk_id)) continue;
			const Chunk& body_chunk = world.data.getChunk(body_chunk_id);
			for(PointId point_id : body_chunk.point_ids)
			{
				if(!world.data.isPointActive(point_id)) continue;
				const glm::dvec3 body_local_position = body_chunk.position
					+ glm::dvec3(world.data.getPoint(point_id).position);
				body.collision_radius = glm::max(
					body.collision_radius, glm::length(body_local_position));
			}
		}

		ToolApplyResult result;
		result.replace_selected_faces = true;
		result.selected_faces.push(FacePtr{face.chunk_id, cap_id});
		return result;
	}

	/**
	 * @brief Dispatches body extrusion or erasure to a hovered face or selected face set.
	 * @param world World whose mesh resources may be edited.
	 * @param action Requested Body action; Drag is handled by its separate tick function.
	 * @param selected_faces Face list whose first selected_face_count entries are selected.
	 * @param selected_face_count Number of selected prefix entries.
	 * @param has_hovered_face Whether hovered_face is the current target.
	 * @param hovered_face Face currently under the cursor.
	 * @return Extrusion's replacement selection or an empty result. Current code
	 *         gives hover priority for extrusion; erase removes only hover when
	 *         present, otherwise all selected non-planet faces.
	 */
	ToolApplyResult applyBodyTool(
		World& world,
		ToolActionId action,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face)
	{
		if(action == ToolActionId::drag) return ToolApplyResult();

		const FacePtr* target = nullptr;
		if(has_hovered_face) target = &hovered_face;
		else if(selected_face_count > 0) target = &selected_faces[0];
		if(target == nullptr || isPlanetChunk(world, target->chunk_id)) return ToolApplyResult();

		if(action == ToolActionId::extrude)
		{
			return applyBodyExtrudeTool(world, *target, 1.0);
		}
		if(action == ToolActionId::erase)
		{
			DynamicArray<FacePtr> faces;
			if(has_hovered_face)
			{
				faces.push(*target);
			}
			else
			{
				for(size_t index = 0; index < selected_face_count; index++)
				{
					if(!isPlanetChunk(world, selected_faces[index].chunk_id))
					{
						faces.push(selected_faces[index]);
					}
				}
			}
			if(faces.size() > 0) world.deleteFaces(faces);
		}
		return ToolApplyResult();
	}
}
