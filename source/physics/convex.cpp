#include <cmath>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/geometry/build_part.h>
#include "internal.h"

namespace danikk_space_engine::collision_internal
{
	namespace
	{

		/**
		 * @brief Adds a normalized direction unless a parallel or antiparallel one is already present.
		 * @param directions Destination list of unique unoriented axes.
		 * @param direction Candidate edge or face-normal direction; near-zero vectors are ignored.
		 */
		void addDirection(DynamicArray<glm::dvec3>& directions, glm::dvec3 direction)
		{
			const double length = glm::length(direction);
			if(length < 1e-9) return;
			direction /= length;
			for(const glm::dvec3& existing : directions)
			{
				if(std::abs(glm::dot(existing, direction)) > 1.0 - 1e-8) return;
			}
			directions.push(direction);
		}

		/**
		 * @brief Projects both convex colliders onto an axis and updates the minimum separating contact.
		 *
		 * The second interval is shifted by the candidate body translation. Disjoint intervals
		 * reject the SAT candidate; otherwise the axis replaces the current contact only when it
		 * yields a smaller penetration depth.
		 * @param a First collider, projected in its current body-relative position.
		 * @param b Second collider, shifted by `translation` before projection.
		 * @param translation Relative offset from collider a to collider b.
		 * @param axis Candidate separating axis; near-zero axes are treated as non-separating.
		 * @param contact Contact record whose penetration and normal may be updated.
		 * @return False if the projected intervals are disjoint; true if they overlap or the
		 *         candidate axis is degenerate.
		 */
		bool testAxis(const ConvexChunkCollider& a, const ConvexChunkCollider& b,
			const glm::dvec3& translation, glm::dvec3 axis, BodyContact& contact)
		{
			const double length = glm::length(axis);
			if(length < 1e-9) return true;
			axis /= length;
			double min_a = std::numeric_limits<double>::max(), max_a = -min_a;
			double min_b = min_a, max_b = max_a;
			for(const glm::dvec3& vertex : a.vertices)
			{
				const double value = glm::dot(vertex, axis);
				min_a = glm::min(min_a, value); max_a = glm::max(max_a, value);
			}
			const double shift = glm::dot(translation, axis);
			for(const glm::dvec3& vertex : b.vertices)
			{
				const double value = glm::dot(vertex, axis) + shift;
				min_b = glm::min(min_b, value); max_b = glm::max(max_b, value);
			}
			if(max_a < min_b || max_b < min_a) return false;
			const double positive_depth = max_a - min_b;
			const double negative_depth = max_b - min_a;
			const double depth = glm::min(positive_depth, negative_depth);
			if(depth < contact.penetration)
			{
				contact.penetration = depth;
				contact.normal = positive_depth <= negative_depth ? axis : -axis;
			}
			return true;
		}

		/**
		 * @brief Averages vertices lying on the collider's extreme support plane.
		 * @param shape Convex chunk proxy whose relative vertices are inspected.
		 * @param normal Direction defining the support plane.
		 * @return Average of vertices within 1e-5 projection units of the maximum.
		 */
		glm::dvec3 supportCenter(const ConvexChunkCollider& shape, const glm::dvec3& normal)
		{
			double maximum = -std::numeric_limits<double>::max();
			for(const glm::dvec3& vertex : shape.vertices) maximum = glm::max(maximum, glm::dot(vertex, normal));
			glm::dvec3 center(0.0);
			size_t count = 0;
			for(const glm::dvec3& vertex : shape.vertices)
			{
				if(maximum - glm::dot(vertex, normal) > 1e-5) continue;
				center += vertex; count++;
			}
			return center / static_cast<double>(count);
		}
	}

	/**
	 * @brief Builds body-relative convex proxies from active BuildPart components.
	 * @param world World supplying active chunks and component data.
	 * @param id ID of the body to prepare.
	 * @param dragged_id Body whose proxy must be kinematic during pair resolution.
	 * @return One proxy for each chunk with at least four unique valid vertices.
	 * BuildPart faces contribute candidate SAT normals and edge directions. The
	 * returned body radius encloses all collected vertices around the body's origin.
	 */
	BodyCollider createCollider(const World& world, WorldBodyId id, WorldBodyId dragged_id)
	{
		BodyCollider collider;
		collider.id = id;
		const WorldBody& body = world.world_body_data.getWorldBody(id);
		for(ChunkId chunk_id : body.chunk_ids)
		{
			if(!world.chunk_system.isChunkActive(chunk_id)) continue;
			const Chunk& chunk = world.chunk_system.getChunk(chunk_id);
			for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
			{
				if(!world.point_data.isPointComponentSlotActive(slot_id)) continue;
				const PointComponentSlot& slot = world.point_data.getPointComponentSlot(slot_id);
				if(slot.type_id != PointComponentTypeId::build_part
					|| !world.point_data.isPointActive(slot.point_id)) continue;
				const BuildPartComponent& part = *static_cast<const BuildPartComponent*>(slot.getData());
				const BuildPartGeometry& geometry = getBuildPartGeometry(part.shape);
				ConvexChunkCollider part_shape;
				const glm::dvec3 anchor = chunk.position
					+ glm::dvec3(world.point_data.getPoint(slot.point_id).position);
				part_shape.vertices.reserve(geometry.positions.size());
				for(const vec3& local_position : geometry.positions)
				{
					const glm::dvec3 part_position = anchor
						+ glm::dvec3(part.rotation * vec4(local_position * part.size, 0.0f));
					const glm::dvec3 body_position = body.rotation * part_position;
					part_shape.vertices.push(body_position);
					part_shape.center += body_position;
					collider.radius = glm::max(collider.radius, glm::length(body_position));
				}
				if(part_shape.vertices.size() < 4) continue;
				part_shape.center /= static_cast<double>(part_shape.vertices.size());
				for(const CubeTriangle& triangle : geometry.triangles)
				{
					const glm::dvec3& a = part_shape.vertices[triangle.points[0]];
					const glm::dvec3& b = part_shape.vertices[triangle.points[1]];
					const glm::dvec3& c = part_shape.vertices[triangle.points[2]];
					addDirection(part_shape.normals, glm::cross(b - a, c - a));
					addDirection(part_shape.edges, b - a);
					addDirection(part_shape.edges, c - b);
					addDirection(part_shape.edges, a - c);
				}
				for(const glm::dvec3& vertex : part_shape.vertices)
				{
					part_shape.radius = glm::max(part_shape.radius,
						glm::length(vertex - part_shape.center));
				}
				collider.chunks.push(static_cast<ConvexChunkCollider&&>(part_shape));
			}
		}
		if(id != dragged_id && body.mass > 0.0)
		{
			collider.inverse_mass = 1.0 / body.mass;
			collider.inverse_inertia = 1.0 / glm::max(0.4 * body.mass * collider.radius * collider.radius, 1e-6);
		}
		return collider;
	}

	bool hasOverlappingBuildPart(
		const World& world, WorldBodyId body_id, BuildPartShape shape,
		const glm::dvec3& world_position, const mat4& body_local_rotation,
		const vec3& size, double penetration_tolerance)
	{
		const WorldBody& body = world.world_body_data.getWorldBody(body_id);
		const BodyCollider existing = createCollider(world, body_id, static_cast<WorldBodyId>(-1));
		const BuildPartGeometry& geometry = getBuildPartGeometry(shape);
		ConvexChunkCollider candidate;
		candidate.vertices.reserve(geometry.positions.size());
		const glm::dvec3 anchor_body_position = glm::conjugate(body.rotation)
			* (world_position - body.position);
		for(const vec3& local_position : geometry.positions)
		{
			const vec3 part_position = body_local_rotation * vec4(local_position * size, 0.0f);
			const glm::dvec3 body_position = body.rotation
				* (anchor_body_position + glm::dvec3(part_position));
			candidate.vertices.push(body_position);
			candidate.center += body_position;
		}
		if(candidate.vertices.size() < 4) return true;
		candidate.center /= static_cast<double>(candidate.vertices.size());
		for(const CubeTriangle& triangle : geometry.triangles)
		{
			const glm::dvec3& a = candidate.vertices[triangle.points[0]];
			const glm::dvec3& b = candidate.vertices[triangle.points[1]];
			const glm::dvec3& c = candidate.vertices[triangle.points[2]];
			addDirection(candidate.normals, glm::cross(b - a, c - a));
			addDirection(candidate.edges, b - a);
			addDirection(candidate.edges, c - b);
			addDirection(candidate.edges, a - c);
		}
		for(const glm::dvec3& vertex : candidate.vertices)
			candidate.radius = glm::max(candidate.radius, glm::length(vertex - candidate.center));

		for(const ConvexChunkCollider& other : existing.chunks)
		{
			BodyContact contact;
			if(findContact(other, candidate, glm::dvec3(0.0), contact)
				&& contact.penetration > penetration_tolerance)
				return true;
		}
		return false;
	}

	/**
	 * @brief Rejects separated convex chunks or returns their minimum-penetration SAT contact.
	 * @param a First chunk proxy.
	 * @param b Second chunk proxy.
	 * @param translation Current body-origin displacement from a to b.
	 * @param contact Output for the overlap normal, penetration, and approximate offsets.
	 * @return False when bounding spheres or an SAT axis separate the proxies;
	 *         true when all tested axes overlap and a finite minimum penetration exists.
	 * Face normals from both chunks and cross products of edge directions form the
	 * candidate axes. The contact point is approximated from opposing support centers.
	 */
	bool findContact(const ConvexChunkCollider& a, const ConvexChunkCollider& b,
		const glm::dvec3& translation, BodyContact& contact)
	{
		const double radius = a.radius + b.radius;
		const glm::dvec3 delta = translation + b.center - a.center;
		if(glm::dot(delta, delta) > radius * radius) return false;
		for(const glm::dvec3& normal : a.normals) if(!testAxis(a, b, translation, normal, contact)) return false;
		for(const glm::dvec3& normal : b.normals) if(!testAxis(a, b, translation, normal, contact)) return false;
		for(const glm::dvec3& edge_a : a.edges)
			for(const glm::dvec3& edge_b : b.edges)
				if(!testAxis(a, b, translation, glm::cross(edge_a, edge_b), contact)) return false;
		if(contact.penetration == std::numeric_limits<double>::max()) return false;
		const glm::dvec3 point = (supportCenter(a, contact.normal)
			+ translation + supportCenter(b, -contact.normal)) * 0.5;
		contact.offset_a = point;
		contact.offset_b = point - translation;
		return true;
	}
}

namespace danikk_space_engine
{
	/** @copydoc WorldPhysics::isBuildPartPlacementClear */
	bool WorldPhysics::isBuildPartPlacementClear(
		WorldBodyId body_id, BuildPartShape shape,
		const glm::dvec3& world_position, const mat4& body_local_rotation,
		const vec3& size, double penetration_tolerance) const
	{
		DSE_WATCH();
		if(!world.world_body_data.isWorldBodyActive(body_id)) return true;
		return !collision_internal::hasOverlappingBuildPart(
			world, body_id, shape, world_position, body_local_rotation, size, penetration_tolerance);
	}
}
