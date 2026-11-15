#include <cmath>

#include <danikk_space_engine/profiler.h>
#include "internal.h"

namespace danikk_space_engine::collision_internal
{
	namespace
	{
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

		// Projects both colliders onto an axis, rejects disjoint intervals and retains
		// the smallest translation needed to separate overlapping intervals.
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

	// Builds convex chunk proxies from unique mesh vertices, face normals and edge directions.
	BodyCollider createCollider(const World& world, WorldBodyId id, WorldBodyId dragged_id)
	{
		BodyCollider collider;
		collider.id = id;
		const WorldBody& body = world.data.getWorldBody(id);
		for(ChunkId chunk_id : body.chunk_ids)
		{
			if(!world.data.isChunkActive(chunk_id)) continue;
			const Chunk& chunk = world.data.getChunk(chunk_id);
			ConvexChunkCollider shape;
			DynamicArray<PointId> point_ids;
			for(GroundTriangleId triangle_id : chunk.ground_triangle_ids)
			{
				if(!world.data.isGroundTriangleActive(triangle_id)) continue;
				const GroundTriangle& triangle = world.data.getGroundTriangle(triangle_id);
				glm::dvec3 positions[3];
				bool valid = true;
				for(uint8 corner = 0; corner < 3; corner++)
				{
					if(!world.data.isPointActive(triangle.points[corner])) { valid = false; break; }
					positions[corner] = body.rotation * (chunk.position
						+ glm::dvec3(world.data.getPoint(triangle.points[corner]).position));
				}
				if(!valid || glm::length(glm::cross(positions[1] - positions[0], positions[2] - positions[0])) < 1e-9)
					continue;
				addDirection(shape.normals, glm::cross(positions[1] - positions[0], positions[2] - positions[0]));
				for(uint8 corner = 0; corner < 3; corner++)
				{
					addDirection(shape.edges, positions[(corner + 1) % 3] - positions[corner]);
					bool found = false;
					for(PointId point_id : point_ids) if(point_id == triangle.points[corner]) { found = true; break; }
					if(found) continue;
					point_ids.push(triangle.points[corner]);
					shape.vertices.push(positions[corner]);
					shape.center += positions[corner];
					collider.radius = glm::max(collider.radius, glm::length(positions[corner]));
				}
			}
			if(shape.vertices.size() < 4) continue;
			shape.center /= static_cast<double>(shape.vertices.size());
			for(const glm::dvec3& vertex : shape.vertices)
				shape.radius = glm::max(shape.radius, glm::length(vertex - shape.center));
			collider.chunks.push(static_cast<ConvexChunkCollider&&>(shape));
		}
		if(id != dragged_id && body.mass > 0.0)
		{
			collider.inverse_mass = 1.0 / body.mass;
			collider.inverse_inertia = 1.0 / glm::max(0.4 * body.mass * collider.radius * collider.radius, 1e-6);
		}
		return collider;
	}

	// Rejects separated spheres, runs SAT on face and edge axes, then computes the support contact.
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
