#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/component/point/bullet.h>

namespace danikk_space_engine
{
	namespace
	{
		const PointComponentType point_component_types[] =
		{
			{}, // none
			{}, // Legacy component ID slot kept reserved to preserve existing type values.
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
			{}, // Reserved legacy type; people live in HumanSystem.
			{}, // Reserved legacy type; inventories live with their owning entities.
			{}, // Legacy BuildPart component ID retained as an unused slot.
			{} // Reserved legacy type; health is stored with its owning system record.
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
