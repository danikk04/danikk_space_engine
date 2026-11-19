#pragma once

#include <limits>

#include <danikk_space_engine/world.h>

namespace danikk_space_engine::collision_internal
{

	/**
	 * @brief Reports whether a body has at least one currently active chunk.
	 * @param world World providing the chunk resource pool.
	 * @param body Body whose chunk IDs are inspected.
	 * @return True as soon as an active chunk ID is found; false if every ID is inactive.
	 */
	inline bool hasLoadedChunk(const World& world, const WorldBody& body)
	{
		for(ChunkId chunk_id : body.chunk_ids)
		{

			if(world.chunks.isChunkActive(chunk_id)) return true;
		}
		return false;
	}

	/**
	 * @brief Stores the mass, local center of mass, and scalar inertia used by rigid-body impulses.
	 */
	struct BodyMassProperties
	{
		double mass = 0.0;
		glm::dvec3 local_center_of_mass = glm::dvec3(0.0);
		double scalar_inertia = 0.0;
	};

	/**
	 * @brief Stores a convex chunk’s transformed vertices, face normals, edges, and broadphase radius.
	 */
	struct ConvexChunkCollider
	{
		/** @brief Vertices are body-relative rotated offsets to preserve precision at large world positions. */
		DynamicArray<glm::dvec3> vertices;
		DynamicArray<glm::dvec3> normals;
		DynamicArray<glm::dvec3> edges;
		glm::dvec3 center = glm::dvec3(0.0);
		double radius = 0.0;
	};

	/**
	 * @brief Groups a world body’s convex chunk colliders and derived mass properties for collision tests.
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
	 * @brief Stores a contact normal, contact offsets, and penetration depth for an intersecting body pair.
	 */
	struct BodyContact
	{
		glm::dvec3 normal;
		glm::dvec3 offset_a;
		glm::dvec3 offset_b;
		double penetration = std::numeric_limits<double>::max();
	};

	/**
	 * @brief Derives the fallback mass and spherical scalar inertia used for planet contacts.
	 * @param body Body whose collision radius and mass are read.
	 * @return Zero properties when collision_radius is non-positive; otherwise uses
	 *         body.mass or 1 kg when its mass is non-positive, and clamps inertia
	 *         to a small positive minimum. The local center of mass remains zero.
	 */
	BodyMassProperties calculateMassProperties(const WorldBody& body);

	/**
	 * @brief Moves one body out of a static contact and removes inward contact velocity.
	 * @param body Movable body to correct.
	 * @param mass_properties Approximate mass and scalar inertia for the body.
	 * @param outward_direction Unit direction away from the surface.
	 * @param penetration Estimated overlap depth; a 1 mm separation margin is added.
	 * @param support_point_local Contact support point in body-local coordinates.
	 */
	void resolveContact(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, double penetration, const glm::dvec3& support_point_local);

	/**
	 * @brief Applies a normal impulse and Coulomb-limited friction at a static contact.
	 * @param body Body receiving the impulse.
	 * @param mass_properties Approximate mass and scalar inertia.
	 * @param outward_direction Contact normal pointing out of the surface.
	 * @param support_point_local Contact point relative to the body's local origin.
	 * Outward or tangential contact motion receives no normal impulse; friction is
	 * limited to 0.9 times the normal impulse magnitude.
	 */
	void applyContactImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, const glm::dvec3& support_point_local);

	/**
	 * @brief Builds convex per-chunk proxies and aggregate inverse mass/inertia for a body.
	 * @param world World supplying active chunks, triangles, and points.
	 * @param id Body ID to convert into collision proxies.
	 * @param dragged_id Body treated as kinematic by assigning zero inverse mass and inertia.
	 * @return Proxies for chunks with at least four unique valid vertices, plus a
	 *         bounding radius about the body origin for broadphase rejection.
	 * Mesh vertices are rotated offsets from the body origin, avoiding addition of
	 * large world-space coordinates during SAT tests.
	 */
	BodyCollider createCollider(const World& world, WorldBodyId id, WorldBodyId dragged_id);

	/**
	 * @brief Tests two convex chunk proxies with bounding spheres and the separating-axis theorem.
	 * @param a First proxy, expressed relative to its body's origin.
	 * @param b Second proxy, expressed relative to its body's origin.
	 * @param translation Current offset from a's body origin to b's body origin.
	 * @param contact Receives the minimum-penetration axis and approximate support contact.
	 * @return False if the bounding spheres or any SAT axis separates the chunks;
	 *         otherwise true when a finite overlap axis was found.
	 */
	bool findContact(const ConvexChunkCollider& a, const ConvexChunkCollider& b,
		const glm::dvec3& translation, BodyContact& contact);

	/**
	 * @brief Applies positional correction, normal impulse, and friction to a body pair.
	 * @param a First body, moved opposite the contact normal.
	 * @param b Second body, moved along the contact normal.
	 * @param ca Collider data and inverse mass/inertia for a.
	 * @param cb Collider data and inverse mass/inertia for b.
	 * @param contact Contact normal, penetration, and body-relative contact offsets.
	 * Positional correction removes 80% of penetration beyond 0.1 mm, weighted by
	 * inverse mass. Impulses are applied only when the bodies are approaching.
	 */
	void resolvePair(WorldBody& a, WorldBody& b, const BodyCollider& ca,
		const BodyCollider& cb, const BodyContact& contact);
}
