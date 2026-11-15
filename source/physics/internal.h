#pragma once

#include <limits>

#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine::collision_internal
{
	struct BodyMassProperties
	{
		double mass = 0.0;
		glm::dvec3 local_center_of_mass = glm::dvec3(0.0);
		double scalar_inertia = 0.0;
	};

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

	BodyMassProperties calculateMassProperties(const WorldBody& body);
	void resolveContact(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, double penetration, const glm::dvec3& support_point_local);
	void applyContactImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, const glm::dvec3& support_point_local);
	BodyCollider createCollider(const World& world, WorldBodyId id, WorldBodyId dragged_id);
	bool findContact(const ConvexChunkCollider& a, const ConvexChunkCollider& b,
		const glm::dvec3& translation, BodyContact& contact);
	void resolvePair(WorldBody& a, WorldBody& b, const BodyCollider& ca,
		const BodyCollider& cb, const BodyContact& contact);
}
