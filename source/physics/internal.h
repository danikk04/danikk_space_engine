#pragma once

#include <limits>

#include <danikk_space_engine/world/world.h>

namespace danikk_space_engine::collision_internal
{

	/**
	 * @brief Reports whether loaded chunk.
	 */
	inline bool hasLoadedChunk(const World& world, const WorldBody& body)
	{
		for(ChunkId chunk_id : body.chunk_ids)
		{

			if(world.data.isChunkActive(chunk_id)) return true;
		}
		return false;
	}

	/**
	 * @brief Represents body mass properties data and behavior.
	 */
	struct BodyMassProperties
	{
		double mass = 0.0;
		glm::dvec3 local_center_of_mass = glm::dvec3(0.0);
		double scalar_inertia = 0.0;
	};

	/**
	 * @brief Represents convex chunk collider data and behavior.
	 */
	struct ConvexChunkCollider
	{
		// Rotated offsets from the body origin avoid subtracting large world coordinates.
		DynamicArray<glm::dvec3> vertices;
		DynamicArray<glm::dvec3> normals;
		DynamicArray<glm::dvec3> edges;
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
	};

	/**
	 * @brief Represents body collider data and behavior.
	 */
	struct BodyCollider
	{
		WorldBodyId id;
		DynamicArray<ConvexChunkCollider> chunks;
		double radius = 0.0;
		double inverse_mass = 0.0;
		double inverse_inertia = 0.0;
	};

	/**
	 * @brief Represents body contact data and behavior.
	 */
	struct BodyContact
	{
		glm::dvec3 normal;
		glm::dvec3 offset_a;
		glm::dvec3 offset_b;
		double penetration = std::numeric_limits<double>::max();
	};

	/**
	 * @brief Calculates mass properties.
	 */
	BodyMassProperties calculateMassProperties(const WorldBody& body);

	/**
	 * @brief Resolves contact.
	 */
	void resolveContact(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, double penetration, const glm::dvec3& support_point_local);

	/**
	 * @brief Applies contact impulse.
	 */
	void applyContactImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, const glm::dvec3& support_point_local);

	/**
	 * @brief Creates collider.
	 */
	BodyCollider createCollider(const World& world, WorldBodyId id, WorldBodyId dragged_id);

	/**
	 * @brief Finds contact.
	 */
	bool findContact(const ConvexChunkCollider& a, const ConvexChunkCollider& b,
		const glm::dvec3& translation, BodyContact& contact);

	/**
	 * @brief Resolves pair.
	 */
	void resolvePair(WorldBody& a, WorldBody& b, const BodyCollider& ca,
		const BodyCollider& cb, const BodyContact& contact);
}
