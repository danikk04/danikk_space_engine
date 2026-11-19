#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/component/point/bullet.h>
#include <danikk_space_engine/component/point/inventory.h>
#include <danikk_space_engine/component/point/build_part.h>
#include <danikk_space_engine/component/point/damageable.h>
#include <danikk_space_engine/component/point/human/human.h>

namespace danikk_space_engine
{
	namespace
	{
		const PointComponentType point_component_types[] =
		{
			{}, // none
			{
				sizeof(MachinegunComponent),
				alignof(MachinegunComponent),
				true,
				constructMachinegunComponent,
				moveMachinegunComponent,
				destroyMachinegunComponent,
				tickMachinegunComponent,
				frameMachinegunComponent
			},
			{
				sizeof(BulletComponent),
				alignof(BulletComponent),
				true,
				constructBulletComponent,
				moveBulletComponent,
				destroyBulletComponent,
				tickBulletComponent,
				frameBulletComponent
			},
			{}, // reserved engine type; no component implementation is registered yet
			{
				sizeof(HumanComponent), alignof(HumanComponent), true,
				constructHumanComponent, moveHumanComponent, destroyHumanComponent,
				tickHumanComponent, frameHumanComponent
			},
			{
				sizeof(InventoryComponent), alignof(InventoryComponent), false,
				constructInventoryComponent, moveInventoryComponent, destroyInventoryComponent,
				tickInventoryComponent, nullptr
			},
			{
				sizeof(BuildPartComponent), alignof(BuildPartComponent), true,
				constructBuildPartComponent, moveBuildPartComponent, destroyBuildPartComponent,
				nullptr, frameBuildPartComponent
			},
			{
				sizeof(DamageableComponent), alignof(DamageableComponent), true,
				constructDamageableComponent, moveDamageableComponent, destroyDamageableComponent,
				nullptr, nullptr
			}
		};
		static_assert(sizeof(point_component_types) / sizeof(point_component_types[0])
			== static_cast<size_t>(PointComponentTypeId::count));
	}

	/**
	 * @brief Resolves a point-component ID to its static storage and callback descriptor.
	 * @param type_id Component type requested by a slot or allocator.
	 * @return Registered descriptor, or the inert `none` descriptor for an out-of-range ID.
	 */
	const PointComponentType& getPointComponentType(PointComponentTypeId type_id)
	{
		const uint32 index = static_cast<uint32>(type_id);
		if(index >= static_cast<uint32>(PointComponentTypeId::count))
		{
			return point_component_types[static_cast<uint32>(PointComponentTypeId::none)];
		}
		return point_component_types[index];
	}
}
