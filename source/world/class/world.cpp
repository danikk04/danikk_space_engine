#include <danikk_framework/glm.h>

#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/world_body/world_body.h>
#include <danikk_space_engine/asset.h>
#include <danikk_space_engine/effects/polygon_highlight.h>
#include <danikk_space_engine/scenario/base.h>

namespace danikk_space_engine
{
	World world;

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
		return data.getWorldBody(chunk.world_body_id).position + chunk.position;
	}

	glm::dvec3 World::getPointWorldPosition(ChunkId chunk_id, PointId point_id) const
	{
		return getChunkWorldPosition(chunk_id) + glm::dvec3(data.getPoint(point_id).position);
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
		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.tick != nullptr) type.tick(slot, *this);
			}
			for(ChunkId chunk_id : body.chunk_ids) data.getChunk(chunk_id).tick(*this);
		}
		for(PointId point_id : pending_point_destruction_ids)
		{
			if(data.isPointActive(point_id)) data.destroyPoint(point_id);
		}
		pending_point_destruction_ids.clear();
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

	void World::queueChunkGenerationRequest(ChunkGenerationRequest request)
	{
		request.generation_id = chunk_loader_generation_id;
		chunk_generation_requests.pushMove(request);
	}

	void World::frame()
	{
		chunk_generation_requests.clear();
		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			for(WorldBodyComponentSlotId slot_id : body.component_slot_ids)
			{
				if(!data.isWorldBodyComponentSlotActive(slot_id)) continue;
				WorldBodyComponentSlot& slot = data.getWorldBodyComponentSlot(slot_id);
				const WorldBodyComponentType& type = getWorldBodyComponentType(slot.type_id);
				if(type.frame != nullptr) type.frame(slot, *this);
			}
		}
		chunk_loader.updatePendingRequests(chunk_loader_generation_id, chunk_generation_requests);

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
					type.accept_generated_chunk(slot, *this, generated_chunk);
				}
			}
		}
		static_asset_collection.base3d_shader.use();
		const CameraComponent& camera = getCameraComponent();
		const vec3 camera_front = camera.getFront();
		const mat4 view = glm::lookAt(vec3(0.0f), camera_front, camera.getUp());
		setViewMatrix(view);
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
		for(WorldBodyId body_id : data.getUsedWorldBodyIds())
		{
			const WorldBody& body = data.getWorldBody(body_id);
			for(ChunkId chunk_id : body.chunk_ids)
			{
				Chunk& chunk = data.getChunk(chunk_id);
				chunk.draw(*this);
			}
		}
		drawPolygonHighlights(*this);
	}

	void World::init()
	{
		chunk_loader.start();
		chunk_loader_generation_id++;
		if(chunk_loader_generation_id == 0) chunk_loader_generation_id++;
		chunk_generation_requests.clear();
		chunk_loader.updatePendingRequests(chunk_loader_generation_id, chunk_generation_requests);
		pending_point_destruction_ids.clear();
		startBaseScenario(*this);
	}

	void World::shutdown()
	{
		chunk_loader.stop();
		chunk_loader_generation_id++;
		chunk_generation_requests.clear();
		data.release();
		pending_point_destruction_ids.clear();
	}
}
