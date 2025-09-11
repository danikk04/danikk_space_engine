#include<object/entity/particle.h>

#include<object/entity/particle/destroyer.h>
#include<object/entity/particle/filler.h>
#include<object/entity/particle/plasma.h>

namespace danikk_space_engine
{
	class NoneBullet : Particle
	{
		int a;
	public:
		static constexpr const char* name = "none";
		static size_t id;

		NoneBullet() = default;
	};

	size_t current_particle_id = 0;

	size_t NoneBullet::id;
	size_t DestroyerBullet::id;
	size_t FillerBullet::id;
	size_t PlasmaBullet::id;

	template<class type> static particle_type createParticleType()
	{
		particle_type result;
		result.particle_name = type::name;
		result.constructor = voidCreateObject<type>;
		type::id = current_particle_id++;
		return result;
	}

	DynamicArray<particle_type> particle_type_list
	{
		createParticleType<NoneBullet>(),
		createParticleType<DestroyerBullet>(),
		createParticleType<FillerBullet>(),
		createParticleType<PlasmaBullet>(),
	};
}
