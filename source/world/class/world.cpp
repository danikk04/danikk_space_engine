#include <danikk_framework/glm.h>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/effects/world_body_preview.h>
#include <danikk_space_engine/effects/chunk_boundaries.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/config.h>
#include <danikk_space_engine/effects/polygon_highlight.h>
#include <danikk_space_engine/scenario/base.h>

namespace danikk_space_engine
{
	World world;

	namespace
	{
		const char* worldBodyComponentName(WorldBodyComponentTypeId type_id)
		{
			switch(type_id)
			{
				case WorldBodyComponentTypeId::planet: return "Planet";
				default: return "Unknown world body component";
			}
		}
	}

	PointComponentSlot& World::getCameraSlot()
	{
		return data.getPointComponentSlot(camera_component_slot_id);
	}

	const PointComponentSlot& World::getCameraSlot() const
	{
		return data.getPointComponentSlot(camera_component_slot_id);
	}

	CameraComponent& World::getCameraComponent()
	{
		return *static_cast<CameraComponent*>(getCameraSlot().getData());
	}

	const CameraComponent& World::getCameraComponent() const
	{
		return *static_cast<const CameraComponent*>(getCameraSlot().getData());
	}

	Point& World::getCameraPoint()
	{
		return data.getPoint(getCameraSlot().point_id);
	}

	const Point& World::getCameraPoint() const
	{
		return data.getPoint(getCameraSlot().point_id);
	}

	glm::dvec3 World::getCameraWorldPosition() const
	{
		const PointComponentSlot& camera_slot = getCameraSlot();
		return getPointWorldPosition(camera_slot.chunk_id, camera_slot.point_id);
	}

	glm::dvec3 World::getChunkWorldPosition(ChunkId chunk_id) const
	{
		const Chunk& chunk = data.getChunk(chunk_id);
		const WorldBody& body = data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation * chunk.position;
	}

	glm::dvec3 World::getPointWorldPosition(ChunkId chunk_id, PointId point_id) const
	{
		const Chunk& chunk = data.getChunk(chunk_id);
		const WorldBody& body = data.getWorldBody(chunk.world_body_id);
		return body.position + body.rotation
			* (chunk.position + glm::dvec3(data.getPoint(point_id).position));
	}

	LocalPlayerControllerComponent& World::getLocalPlayerControllerComponent()
	{
		PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_component_slot_id);
		return *static_cast<LocalPlayerControllerComponent*>(slot.getData());
	}

	const LocalPlayerControllerComponent& World::getLocalPlayerControllerComponent() const
	{
		const PointComponentSlot& slot = data.getPointComponentSlot(local_player_controller_component_slot_id);
		return *static_cast<const LocalPlayerControllerComponent*>(slot.getData());
	}

	void World::tick()
	{
		Watcher tick_scope("tick");
		{
			Watcher world_body_components_scope("WorldBody components");
			const DynamicArray<WorldBodyId> world_body_ids(data.getUsedWorldBodyIds());
			DynamicArray<WorldBodyComponentSlotId> component_slot_ids;
			DynamicArray<ChunkId> chunk_ids;
			for(WorldBodyId body_id : world_body_ids)
			{
				if(!data.isWorldBodyActive(body_id)) continue;
				component_slot_ids.clear();
				chunk_ids.clear();
				{
					const WorldBody& body = data.getWorldBody(body_id);
					for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
					{
						component_slot_ids.push(slot_id);
					}
					for(ChunkId chunk_id : body.chunk_ids)
					{
						chunk_ids.push(chunk_id);
					}
				}

				for(WorldBodyComponentSlotId slot_id : component_slot_ids)
				{
					if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
					WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
					const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
					if(type.tick != nullptr)
					{
						Watcher component_scope(worldBodyComponentName(slot.type_id));
						type.tick(slot, *this);
					}
				}
				for(ChunkId chunk_id : chunk_ids)
				{
					if(!data.isChunkActive(chunk_id)) continue;
					data.getChunk(chunk_id).tick(*this);
				}
			}
		}
		for(PointId point_id : pending_point_destruction_ids)
		{
			if(data.isPointActive(point_id)) data.destroyPoint(point_id);
		}
		pending_point_destruction_ids.clear();
		physics.tick(*this);
	}

	void World::deferPointDestruction(PointId point_id)
	{
		if(!data.isPointActive(point_id)) return;
		for(PointId queued_point_id : pending_point_destruction_ids)
		{
			if(queued_point_id == point_id) return;
		}
		pending_point_destruction_ids.push(point_id);
	}

	void World::replaceChunkGenerationRequests(
		WorldBodyComponentSlotId component_slot_id,
		const DynamicArray<ChunkGenerationRequest>& requests)
	{
		for(size_t index = 0; index < chunk_generation_requests.size();)
		{
			if(chunk_generation_requests[index].component_slot_id != component_slot_id)
			{
				index++;
				continue;
			}
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
			{
				chunk_generation_requests[shift - 1] = static_cast<ChunkGenerationRequest&&>(chunk_generation_requests[shift]);
			}
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
		}
		for(const ChunkGenerationRequest& request : requests)
		{
			ChunkGenerationRequest queued_request = request;
			queued_request.generation_id = chunk_loader_generation_id;
			chunk_generation_requests.pushMove(queued_request);
		}
		chunk_generation_requests_dirty = true;
	}

	void World::removeChunkGenerationRequest(
		WorldBodyComponentSlotId component_slot_id,
		const glm::ivec3& grid_coordinate)
	{
		for(size_t index = 0; index < chunk_generation_requests.size(); index++)
		{
			const ChunkGenerationRequest& request = chunk_generation_requests[index];
			if(request.component_slot_id != component_slot_id
				|| request.grid_coordinate.x != grid_coordinate.x
				|| request.grid_coordinate.y != grid_coordinate.y
				|| request.grid_coordinate.z != grid_coordinate.z) continue;
			for(size_t shift = index + 1; shift < chunk_generation_requests.size(); shift++)
			{
				chunk_generation_requests[shift - 1] = static_cast<ChunkGenerationRequest&&>(chunk_generation_requests[shift]);
			}
			chunk_generation_requests.resize(chunk_generation_requests.size() - 1);
			chunk_generation_requests_dirty = true;
			return;
		}
	}

	void World::frame()
	{
		Watcher frame_scope("World::frame");
		{
			Watcher world_body_components_scope("WorldBody components");
			for(WorldBodyId body_id : data.getUsedWorldBodyIds())
			{
				const WorldBody& body = data.getWorldBody(body_id);
				for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
				{
					if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
					WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
					const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
					if(type.frame != nullptr)
					{
						Watcher component_scope(worldBodyComponentName(slot.type_id));
						type.frame(slot, *this);
					}
				}
			}
		}
		if(chunk_generation_requests_dirty)
		{
			Watcher request_update_scope("ChunkLoader::updatePendingRequests");
			chunk_loader.updatePendingRequests(chunk_loader_generation_id, chunk_generation_requests);
			chunk_generation_requests_dirty = false;
		}

		GeneratedChunkData generated_chunk;
		if(chunk_loader.tryTakeCompleted(generated_chunk)
			&& generated_chunk.generation_id == chunk_loader_generation_id
			&& data.isWorldBodyActive(generated_chunk.world_body_id)
			&& data.isWorldBodyComponentSlotActive(generated_chunk.component_slot_id))
		{
			WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(generated_chunk.component_slot_id);
			if(slot.world_body_id == generated_chunk.world_body_id
				&& slot.generation_id == generated_chunk.component_generation_id)
			{
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.accept_generated_chunk != nullptr)
				{
					Watcher accept_scope("Accept generated chunk");
					type.accept_generated_chunk(slot, *this, generated_chunk);
				}
			}
		}
		static_asset_collection.base3d_shader.use();
		const CameraComponent& camera = getCameraComponent();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
		{
			Watcher chunk_frame_scope("Chunk mesh and component frame");
			for(WorldBodyId body_id : data.getUsedWorldBodyIds())
			{
				const WorldBody& body = data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					Chunk& chunk = data.getChunk(chunk_id);
					if(chunk.mesh_dirty) chunk.createMesh();
					chunk.frame(*this);
				}
			}
		}
		{
			Watcher chunk_draw_scope("Chunk draw");
			for(WorldBodyId body_id : data.getUsedWorldBodyIds())
			{
				const WorldBody& body = data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					Chunk& chunk = data.getChunk(chunk_id);
					chunk.draw(*this);
				}
			}
		}
		if(settings.show_chunk_bounds) drawChunkBoundaries(*this);
		if(settings.show_face_highlight) drawPolygonHighlights(*this);
		const LocalPlayerControllerComponent& controller = getLocalPlayerControllerComponent();
		if(settings.show_world_body_preview
			&& controller.active_tool == ToolId::place
			&& controller.selected_tool_action == ToolActionId::create_body)
		{
			const RaycastHit hit = raycastFromCamera();
			drawWorldBodyPlacementPreview(*this, hit);
		}
	}

	void World::init()
	{
		chunk_loader.start();
		chunk_loader_generation_id++;
		if(chunk_loader_generation_id == 0) chunk_loader_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		chunk_loader.updatePendingRequests(chunk_loader_generation_id, chunk_generation_requests);
		pending_point_destruction_ids.clear();
		startBaseScenario(*this);
	}

	void World::shutdown()
	{
		chunk_loader.stop();
		chunk_loader_generation_id++;
		chunk_generation_requests.clear();
		chunk_generation_requests_dirty = false;
		data.release();
		pending_point_destruction_ids.clear();
	}
}
