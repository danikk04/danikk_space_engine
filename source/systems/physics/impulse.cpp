#include <cmath>

#include <danikk_space_engine/systems/telemetry/telemetry.h>
#include <danikk_space_engine/systems/physics/internal.h>

namespace danikk_space_engine::collision_internal
{
	namespace
	{

		/**
		 * @brief Applies a single-body impulse to linear and angular velocity.
		 * @param body Body receiving the impulse.
		 * @param mass_properties Mass and scalar inertia used to scale the response.
		 * @param contact_offset Lever arm from the center of mass in world coordinates.
		 * @param impulse Impulse vector in world coordinates.
		 */
		void applyImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
			const glm::dvec3& contact_offset, const glm::dvec3& impulse)
		{
			body.linear_velocity += impulse / mass_properties.mass;
			body.angular_velocity += glm::cross(contact_offset, impulse) / mass_properties.scalar_inertia;
		}

		/**
		 * @brief Applies equal and opposite impulses using each body's inverse mass and inertia.
		 * @param a First body, receiving the negative impulse.
		 * @param b Second body, receiving the positive impulse.
		 * @param ca Collider response properties for a.
		 * @param cb Collider response properties for b.
		 * @param contact Contact offsets relative to each body origin.
		 * @param impulse World-space impulse applied to b and negated for a.
		 */
		void applyPairImpulse(WorldBody& a, WorldBody& b, const BodyCollider& ca,
			const BodyCollider& cb, const BodyContact& contact, const glm::dvec3& impulse)
		{
			a.linear_velocity -= impulse * ca.inverse_mass;
			b.linear_velocity += impulse * cb.inverse_mass;
			a.angular_velocity -= glm::cross(contact.offset_a, impulse) * ca.inverse_inertia;
			b.angular_velocity += glm::cross(contact.offset_b, impulse) * cb.inverse_inertia;
		}

		/**
		 * @brief Computes the inverse mass contribution of a contact point, including rotational inertia.
		 * @param a First collider's inverse mass and inertia.
		 * @param b Second collider's inverse mass and inertia.
		 * @param contact Local contact arms for both bodies.
		 * @param direction Unit impulse direction being evaluated.
		 * @return Sum of linear and angular inverse-mass terms along the direction.
		 */
		double effectiveInverseMass(const BodyCollider& a, const BodyCollider& b,
			const BodyContact& contact, const glm::dvec3& direction)
		{
			const glm::dvec3 arm_a = glm::cross(contact.offset_a, direction);
			const glm::dvec3 arm_b = glm::cross(contact.offset_b, direction);
			return a.inverse_mass + b.inverse_mass
				+ glm::dot(arm_a, arm_a) * a.inverse_inertia + glm::dot(arm_b, arm_b) * b.inverse_inertia;
		}
	}

	/**
	 * @brief Approximates a body's mass and inertia as a solid sphere using collision_radius.
	 * @param body Body providing mass and collision radius.
	 * @return Zero-initialized properties when the collision radius is not positive;
	 *         otherwise non-positive body mass falls back to 1 and inertia is clamped
	 *         to at least 1e-6. The center of mass is local origin.
	 */
	BodyMassProperties calculateMassProperties(const WorldBody& body)
	{
		BodyMassProperties properties;
		if(body.collision_radius <= 0.0) return properties;
		properties.mass = body.mass > 0.0 ? body.mass : 1.0;
		properties.scalar_inertia = glm::max(
			0.4 * properties.mass * body.collision_radius * body.collision_radius,
			0.000001);
		properties.scalar_inertia = glm::max(properties.scalar_inertia, 0.000001);
		return properties;
	}

	/**
	 * @brief Cancels inward point velocity and applies friction against a static surface.
	 * @param body Body receiving the response.
	 * @param mass_properties Approximate body mass and scalar inertia.
	 * @param outward_direction Unit contact normal directed away from the surface.
	 * @param support_point_local Contact point relative to body origin, in body-local coordinates.
	 * The normal impulse includes angular response about the center of mass. A
	 * subsequent friction impulse opposes tangential point velocity and is capped
	 * by a Coulomb coefficient of 0.9. Separating contacts receive no impulse.
	 */
	void applyContactImpulse(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, const glm::dvec3& support_point_local)
	{
		const glm::dvec3 world_center_of_mass = body.position
			+ body.rotation * mass_properties.local_center_of_mass;
		const glm::dvec3 contact_point = body.position + body.rotation * support_point_local;
		const glm::dvec3 contact_offset = contact_point - world_center_of_mass;
		const double inverse_mass = 1.0 / mass_properties.mass;
		const double inverse_inertia = 1.0 / mass_properties.scalar_inertia;
		const glm::dvec3 contact_velocity = body.linear_velocity
			+ glm::cross(body.angular_velocity, contact_offset);
		const double inward_speed = glm::dot(contact_velocity, outward_direction);
		if(inward_speed >= 0.0) return;

		const glm::dvec3 normal_arm = glm::cross(contact_offset, outward_direction);
		const double normal_effective_mass = inverse_mass
			+ glm::dot(normal_arm, normal_arm) * inverse_inertia;
		const double normal_impulse_magnitude = -inward_speed / normal_effective_mass;
		applyImpulse(body, mass_properties, contact_offset,
			outward_direction * normal_impulse_magnitude);

		const glm::dvec3 post_normal_velocity = body.linear_velocity
			+ glm::cross(body.angular_velocity, contact_offset);
		const glm::dvec3 tangent_velocity = post_normal_velocity
			- outward_direction * glm::dot(post_normal_velocity, outward_direction);
		const double tangent_speed = glm::length(tangent_velocity);
		if(tangent_speed <= 0.000001) return;
		const glm::dvec3 tangent_direction = tangent_velocity / tangent_speed;
		const glm::dvec3 tangent_arm = glm::cross(contact_offset, tangent_direction);
		const double tangent_effective_mass = inverse_mass
			+ glm::dot(tangent_arm, tangent_arm) * inverse_inertia;
		constexpr double friction_coefficient = 0.9;
		const double friction_impulse_magnitude = glm::clamp(
			-tangent_speed / tangent_effective_mass,
			-friction_coefficient * normal_impulse_magnitude,
			friction_coefficient * normal_impulse_magnitude);
		applyImpulse(body, mass_properties, contact_offset,
			tangent_direction * friction_impulse_magnitude);
	}

	/**
	 * @brief Applies positional depenetration, then resolves velocity at a static contact.
	 * @param body Body moved out of the surface.
	 * @param mass_properties Approximate mass and scalar inertia.
	 * @param outward_direction Unit direction from the surface into free space.
	 * @param penetration Estimated overlap depth; a 0.001 m margin is added.
	 * @param support_point_local Contact point relative to the body's local origin.
	 */
	void resolveContact(WorldBody& body, const BodyMassProperties& mass_properties,
		const glm::dvec3& outward_direction, double penetration,
		const glm::dvec3& support_point_local)
	{
		body.position += outward_direction * (penetration + 0.001);
		applyContactImpulse(body, mass_properties, outward_direction, support_point_local);
	}

	/**
	 * @brief Corrects a pair's overlap and applies normal and friction impulses when they approach.
	 * @param a First body, corrected opposite the contact normal.
	 * @param b Second body, corrected along the contact normal.
	 * @param ca Collider response properties for a.
	 * @param cb Collider response properties for b.
	 * @param contact SAT contact data, including penetration and local contact offsets.
	 * Removes 80% of penetration beyond 0.1 mm, distributed by inverse mass.
	 * There is no restitution term; friction magnitude is capped at 0.9 times the
	 * normal impulse. A pair with no movable mass is unchanged.
	 */
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
