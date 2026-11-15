#include <cmath>
#include <limits>

#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine
{
	namespace
	{
		struct ConvexChunkCollider
		{
			// Rotated offsets from the body origin avoid subtracting large world coordinates.
			DynamicArray<glm::dvec3> vertices;
			DynamicArray<glm::dvec3> normals;
			DynamicArray<glm::dvec3> edges;
			glm::dvec3 center = glm::dvec3(0.0);
			double radius = 0.0;
		};

		struct BodyCollider
		{
			WorldBodyId id;
			DynamicArray<ConvexChunkCollider> chunks;
			double radius = 0.0;
			double inverse_mass = 0.0;
			double inverse_inertia = 0.0;
		};

		struct BodyContact
		{
			glm::dvec3 normal;
			glm::dvec3 offset_a;
			glm::dvec3 offset_b;
			double penetration = std::numeric_limits<double>::max();
		};

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

		bool isPlanetBody(const World& world, const WorldBody& body)
		{
			for(WorldBodyComponentSlotId id : body.component_slot_ids)
			{
				if(world.data.isWorldBodyComponentSlotActive(id)
					&& world.data.getWorldBodyComponentSlot(id).type_id == WorldBodyComponentTypeId::planet)
					return true;
			}
			return false;
		}

		// Reads only mesh triangle vertices, skipping isolated component points. Each chunk is
		// treated as a convex collider; unique face normals and edge directions provide SAT axes.
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

		// SAT checks both sets of face normals and every edge cross product. The smallest
		// overlap supplies the separation normal; averaged support vertices supply the contact.
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

		void applyPairImpulse(WorldBody& a, WorldBody& b, const BodyCollider& ca,
			const BodyCollider& cb, const BodyContact& contact, const glm::dvec3& impulse)
		{
			a.linear_velocity -= impulse * ca.inverse_mass;
			b.linear_velocity += impulse * cb.inverse_mass;
			a.angular_velocity -= glm::cross(contact.offset_a, impulse) * ca.inverse_inertia;
			b.angular_velocity += glm::cross(contact.offset_b, impulse) * cb.inverse_inertia;
		}

		double effectiveInverseMass(const BodyCollider& a, const BodyCollider& b,
			const BodyContact& contact, const glm::dvec3& direction)
		{
			const glm::dvec3 arm_a = glm::cross(contact.offset_a, direction);
			const glm::dvec3 arm_b = glm::cross(contact.offset_b, direction);
			return a.inverse_mass + b.inverse_mass
				+ glm::dot(arm_a, arm_a) * a.inverse_inertia + glm::dot(arm_b, arm_b) * b.inverse_inertia;
		}

		// Splits positional correction by inverse mass, then cancels closing contact velocity.
		// Equal opposite impulses include angular motion; Coulomb friction limits the tangent impulse.
		void resolvePair(WorldBody& a, WorldBody& b, const BodyCollider& ca,
			const BodyCollider& cb, const BodyContact& contact)
		{
			const double inverse_mass = ca.inverse_mass + cb.inverse_mass;
			if(inverse_mass <= 0.0) return;
			const double correction = glm::max(contact.penetration - 0.0001, 0.0) * 0.8 / inverse_mass;
			a.position -= contact.normal * correction * ca.inverse_mass;
			b.position += contact.normal * correction * cb.inverse_mass;
			const auto relative_velocity = [&]()
			{
				return b.linear_velocity + glm::cross(b.angular_velocity, contact.offset_b)
					- a.linear_velocity - glm::cross(a.angular_velocity, contact.offset_a);
			};
			const double speed = glm::dot(relative_velocity(), contact.normal);
			if(speed >= 0.0) return;
			const double normal_impulse = -speed / effectiveInverseMass(ca, cb, contact, contact.normal);
			applyPairImpulse(a, b, ca, cb, contact, contact.normal * normal_impulse);
			glm::dvec3 tangent = relative_velocity();
			tangent -= contact.normal * glm::dot(tangent, contact.normal);
			const double tangent_speed = glm::length(tangent);
			if(tangent_speed < 1e-8) return;
			tangent /= tangent_speed;
			const double friction_impulse = glm::min(
				tangent_speed / effectiveInverseMass(ca, cb, contact, tangent), normal_impulse * 0.9);
			applyPairImpulse(a, b, ca, cb, contact, -tangent * friction_impulse);
		}
	}

	// Builds transient colliders for mesh bodies, excluding planets and component-only bodies.
	// Four solver sweeps process each unordered body/chunk pair once per sweep. Body spheres
	// and chunk spheres reject distant pairs before convex SAT; dragged bodies act as kinematic obstacles.
	void World::resolveBodyCollisions()
	{
		Watcher watcher("World::body collisions");
		WorldBodyId dragged_id = static_cast<WorldBodyId>(-1);
		if(data.isPointComponentSlotActive(local_player_controller_component_slot_id))
			dragged_id = getLocalPlayerControllerComponent().dragged_world_body_id;
		DynamicArray<BodyCollider> colliders;
		for(WorldBodyId id : data.getUsedWorldBodyIds())
		{
			if(isPlanetBody(*this, data.getWorldBody(id))) continue;
			BodyCollider collider = createCollider(*this, id, dragged_id);
			if(collider.chunks.size() > 0) colliders.push(static_cast<BodyCollider&&>(collider));
		}
		for(uint8 iteration = 0; iteration < 4; iteration++)
		{
			for(size_t i = 0; i < colliders.size(); i++)
			{
				const BodyCollider& a = colliders[i];
				WorldBody& body_a = data.getWorldBody(a.id);
				for(size_t j = i + 1; j < colliders.size(); j++)
				{
					const BodyCollider& b = colliders[j];
					if(a.inverse_mass + b.inverse_mass <= 0.0) continue;
					WorldBody& body_b = data.getWorldBody(b.id);
					const glm::dvec3 delta = body_b.position - body_a.position;
					const double radius = a.radius + b.radius;
					if(glm::dot(delta, delta) > radius * radius) continue;
					for(const ConvexChunkCollider& chunk_a : a.chunks)
					{
						for(const ConvexChunkCollider& chunk_b : b.chunks)
						{
							BodyContact contact;
							if(findContact(chunk_a, chunk_b, body_b.position - body_a.position, contact))
								resolvePair(body_a, body_b, a, b, contact);
						}
					}
				}
			}
		}
	}
}
