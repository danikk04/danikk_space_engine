#include <danikk_space_engine/tool.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/planet/planet.h>

#include <cmath>

namespace danikk_space_engine
{
	namespace
	{
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

	// Replaces a triangular face with a translated cap and three side quads. Each quad is
	// triangulated with a consistent outward winding. New points stay in the owning chunk's
	// local space; the returned cap FacePtr becomes the controller's current selection.
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
