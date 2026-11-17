#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/chunk.h>
#include <danikk_space_engine/world/world.h>

#include <cstdlib>

#include <danikk_engine/mesh.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	namespace
	{
		/** @brief Removes an ID from a callback list while preserving traversal order. */
		bool removePointComponentId(DynamicArray<PointComponentSlotId>& ids, PointComponentSlotId slot_id)
		{
			for(size_t end = ids.size(); end > 0; --end)
			{
				if(ids[end - 1] != slot_id) continue;
				for(size_t index = end; index < ids.size(); ++index) ids[index - 1] = ids[index];
				ids.pop();
				return true;
			}
			return false;
		}
	}

	/**
	 * @brief Transfers mesh and resource-array ownership from another chunk.
	 */
	Chunk::Chunk(Chunk&& other) noexcept
		: mesh(other.mesh), material_ranges(static_cast<DynamicArray<ChunkMaterialRange>&&>(other.material_ranges)),
		bounds_valid(other.bounds_valid),
		human_component_slot_ids(static_cast<DynamicArray<PointComponentSlotId>&&>(other.human_component_slot_ids)),
		mesh_dirty(other.mesh_dirty), has_active_human_collisions(other.has_active_human_collisions),
		bounds_center(other.bounds_center), bounds_radius(other.bounds_radius),
		chunk_id(other.chunk_id), world_body_id(other.world_body_id),
		grid_coordinate(other.grid_coordinate), position(other.position),
		ground_triangle_ids(static_cast<DynamicArray<GroundTriangleId>&&>(other.ground_triangle_ids)),
		point_ids(static_cast<DynamicArray<PointId>&&>(other.point_ids)),
		point_component_slot_ids(static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids)),
		graph_component_slot_ids(static_cast<DynamicArray<GraphComponentSlotId>&&>(other.graph_component_slot_ids))
	{
		other.mesh = nullptr;
	}

	Chunk& Chunk::operator=(Chunk&& other) noexcept
	{
		if(this == &other)
		{
			return *this;
		}

		destroyMesh();
		mesh_dirty = other.mesh_dirty;
		has_active_human_collisions = other.has_active_human_collisions;
		bounds_valid = other.bounds_valid;
		bounds_center = other.bounds_center;
		bounds_radius = other.bounds_radius;
		chunk_id = other.chunk_id;
		world_body_id = other.world_body_id;
		grid_coordinate = other.grid_coordinate;
		position = other.position;
		ground_triangle_ids = static_cast<DynamicArray<GroundTriangleId>&&>(other.ground_triangle_ids);
		point_ids = static_cast<DynamicArray<PointId>&&>(other.point_ids);
		point_component_slot_ids = static_cast<DynamicArray<PointComponentSlotId>&&>(other.point_component_slot_ids);
		human_component_slot_ids = static_cast<DynamicArray<PointComponentSlotId>&&>(other.human_component_slot_ids);
		graph_component_slot_ids = static_cast<DynamicArray<GraphComponentSlotId>&&>(other.graph_component_slot_ids);
		material_ranges = static_cast<DynamicArray<ChunkMaterialRange>&&>(other.material_ranges);
		mesh = other.mesh;
		other.mesh = nullptr;
		return *this;
	}

	/**
	 * @brief Releases the mesh owned by the chunk.
	 */
	Chunk::~Chunk()
	{
		destroyMesh();
	}

	/**
	 * @brief Frees the GPU mesh allocation owned by the chunk, if one exists.
	 */
	void Chunk::destroyMesh()
	{
		/** @brief Measures the mesh destruction duration for the profiler. */
		DSE_WATCH();
		if(mesh != nullptr)
		{
			mesh->free();
			mesh->~Mesh();
			free(mesh);
			mesh = nullptr;
		}
	}

	/**
	 * @brief Marks geometry stale and invalidates both material draw ranges and raycast bounds.
	 */
	void Chunk::invalidateMesh()
	{
		mesh_dirty = true;
		bounds_valid = false;
		material_ranges.clear();
	}

	/** @copydoc Chunk::addPointComponentSlot */
	void Chunk::addPointComponentSlot(PointComponentSlotId slot_id, PointComponentTypeId type_id)
	{
		point_component_slot_ids.push(slot_id);
		if(type_id == PointComponentTypeId::human) human_component_slot_ids.push(slot_id);
	}

	/** @copydoc Chunk::removePointComponentSlot */
	void Chunk::removePointComponentSlot(PointComponentSlotId slot_id, PointComponentTypeId type_id)
	{
		if(!removePointComponentId(point_component_slot_ids, slot_id))
			danikk_engine::formatFatalError("Point component % is missing from owning chunk %", slot_id, chunk_id);
		if(type_id == PointComponentTypeId::human && !removePointComponentId(human_component_slot_ids, slot_id))
			danikk_engine::formatFatalError("Human component % is missing from chunk % cache", slot_id, chunk_id);
	}

	/**
	 * @brief Appends a graph-component slot ID to the chunk's callback traversal list.
	 * @param slot_id Slot ID to append.
	 */
	void Chunk::addGraphComponentSlot(GraphComponentSlotId slot_id)
	{
		graph_component_slot_ids.push(slot_id);
	}

	/** @brief Runs the tick callbacks configured for the chunk's point components. */
	void Chunk::tickPointComponents(World& world)
	{
		DSE_WATCH();
		const size_t component_count = point_component_slot_ids.size();
		for(size_t component_index = 0; component_index < component_count; component_index++)
		{
			const PointComponentSlotId slot_id = point_component_slot_ids[component_index];
			if(slot_id >= world.data.getPointComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
			}
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;

			PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			if(slot.chunk_id != chunk_id) continue;
			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}
			if(type.tick != nullptr)
			{
				type.tick(slot, world);
			}
		}
	}

	/** @brief Runs the frame callbacks configured for the chunk's point components. */
	void Chunk::framePointComponents(World& world)
	{
		DSE_WATCH();
		const size_t component_count = point_component_slot_ids.size();
		for(size_t component_index = 0; component_index < component_count; component_index++)
		{
			const PointComponentSlotId slot_id = point_component_slot_ids[component_index];
			if(slot_id >= world.data.getPointComponentSlotCount())
			{
				danikk_engine::formatFatalError("Chunk references invalid point component slot %", slot_id);
			}
			if(!world.data.isPointComponentSlotActive(slot_id)) continue;

			PointComponentSlot& slot = world.data.getPointComponentSlot(slot_id);
			if(slot.uses_external_data && slot.external_data == nullptr)
			{
				danikk_engine::formatFatalError("Point component slot % has no external data", slot_id);
			}

			const PointComponentType& type = getPointComponentType(slot.type_id);
			if(type.frame != nullptr)
			{
				type.frame(slot, world);
			}
		}
	}

	/**
	 * @brief Runs tick callbacks for the active point and graph components attached to the chunk.
	 */
	void Chunk::tick(World& world)
	{
		/** @brief Measures the full chunk tick traversal. */
		DSE_WATCH();
		tickPointComponents(world);
		{
			/** @brief Measures the graph-component tick traversal. */
			DSE_WATCH_SCOPE("Graph components");
			const size_t graph_component_count = graph_component_slot_ids.size();
			for(size_t component_index = 0; component_index < graph_component_count; component_index++)
			{
				const GraphComponentSlotId slot_id = graph_component_slot_ids[component_index];
				if(slot_id >= world.data.getGraphComponentSlotCount())
				{
					danikk_engine::formatFatalError("Chunk references invalid graph component slot %", slot_id);
				}
				if(!world.data.isGraphComponentSlotActive(slot_id)) continue;
				GraphComponentSlot& slot = world.data.getGraphComponentSlot(slot_id);
				const GraphComponentType& type = getGraphComponentType(slot.type_id);
				if(slot.uses_external_data && slot.external_data == nullptr)
				{
					danikk_engine::formatFatalError("Graph component slot % has no external data", slot_id);
				}
				if(type.tick != nullptr)
				{
					type.tick(slot, world);
				}
			}
		}
	}

	/**
	 * @brief Runs frame callbacks for the active point and graph components attached to the chunk.
	 */
	void Chunk::frame(World& world)
	{
		/** @brief Measures the full chunk frame traversal. */
		DSE_WATCH();
		framePointComponents(world);
		{
			/** @brief Measures the graph-component frame traversal. */
			DSE_WATCH_SCOPE("Graph components");
			const size_t graph_component_count = graph_component_slot_ids.size();
			for(size_t component_index = 0; component_index < graph_component_count; component_index++)
			{
				const GraphComponentSlotId slot_id = graph_component_slot_ids[component_index];
				if(slot_id >= world.data.getGraphComponentSlotCount())
				{
					danikk_engine::formatFatalError("Chunk references invalid graph component slot %", slot_id);
				}
				if(!world.data.isGraphComponentSlotActive(slot_id)) continue;
				GraphComponentSlot& slot = world.data.getGraphComponentSlot(slot_id);
				const GraphComponentType& type = getGraphComponentType(slot.type_id);
				if(slot.uses_external_data && slot.external_data == nullptr)
				{
					danikk_engine::formatFatalError("Graph component slot % has no external data", slot_id);
				}
				if(type.frame != nullptr)
				{
					type.frame(slot, world);
				}
			}
		}
	}

	/**
	 * @brief Combines body rotation and camera-relative chunk translation into the render transform.
	 * @param world World supplying the body's current transform and camera origin.
	 * @return Matrix mapping chunk-local vertices to camera-relative render coordinates.
	 */
	mat4 Chunk::createWorldMatrix(const World& world) const
	{
		const WorldBody& body = world.data.getWorldBody(world_body_id);
		const glm::dvec3 relative_chunk_position = body.position
			+ body.rotation * position
			- world.getCameraWorldPosition();
		mat4 body_rotation(1.0f);
		body_rotation[0] = vec4(vec3(body.rotation * glm::dvec3(1.0, 0.0, 0.0)), 0.0f);
		body_rotation[1] = vec4(vec3(body.rotation * glm::dvec3(0.0, 1.0, 0.0)), 0.0f);
		body_rotation[2] = vec4(vec3(body.rotation * glm::dvec3(0.0, 0.0, 1.0)), 0.0f);
		return glm::translate(mat4(1.0f), vec3(relative_chunk_position)) * body_rotation;
	}

	/**
	 * @brief Binds each material's texture/tint and draws that material's contiguous mesh index range.
	 * @param world World used to calculate the camera-relative chunk matrix.
	 */
	void Chunk::draw(World& world)
	{
		if(mesh == nullptr || material_ranges.size() == 0) return;
		setWorldMatrix(createWorldMatrix(world));
		for(const ChunkMaterialRange& range : material_ranges)
		{
			if(range.index_count == 0) continue;
			MaterialDefinition& material = getMaterial(range.material_id);
			if(material.texture.isNull())
			{
				white_texture.bind();
			}
			else
			{
				material.texture.bind();
			}
			setDrawColor(vec4(material.color, 1.0f));
			mesh->drawRange(range.first_index, range.index_count);
		}
	}
}
