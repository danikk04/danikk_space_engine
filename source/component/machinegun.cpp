#include <utility>

#include <danikk_space_engine/component/machinegun.h>
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/asset.h>

#include <danikk_engine/texture.h>
#include <danikk_engine/texture_methods.h>
#include <danikk_framework/glm.h>

namespace danikk_space_engine
{
	void constructMachinegunComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component construction received null destination");
		}
		if(init_data == nullptr)
		{
			new (destination) MachinegunComponent();
		}
		else
		{
			new (destination) MachinegunComponent(*static_cast<const MachinegunComponent*>(init_data));
		}
	}

	void moveMachinegunComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component move received null memory");
		}
		MachinegunComponent* source_component = static_cast<MachinegunComponent*>(source);
		new (destination) MachinegunComponent(std::move(*source_component));
		std::destroy_at(source_component);
	}

	void destroyMachinegunComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component destruction received null memory");
		}
		std::destroy_at(static_cast<MachinegunComponent*>(data));
	}

	// Draws the shared MG-42 mesh at the position of the point that owns this component.
	// The component scale is applied after translation; OBJ groups and materials have already
	// been combined into the shared mesh during static asset loading.
	void frameMachinegunComponent(PointComponentSlot& slot, World& world)
	{
		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Machinegun component frame received null data");
		}
		if(!world.data.isPointActive(slot.point_id))
		{
			danikk_engine::formatFatalError("Machinegun component references invalid point %", slot.point_id);
		}
		if(static_asset_collection.mg42_mesh.isNull())
		{
			danikk_engine::formatFatalError("Machinegun component has no loaded MG-42 mesh");
		}

		const MachinegunComponent& machinegun = *static_cast<const MachinegunComponent*>(slot.getData());
		white_texture.bind();
		setDrawColor(vec4(0.4f, 0.4f, 0.4f, 1.0f));
		const mat4 world_matrix = glm::translate(
			mat4(1.0f),
			world.data.getPoint(slot.point_id).position)
			* glm::rotate(mat4(1.0f), -machinegun.rotation.x, vec3(0.0f, 1.0f, 0.0f))
			* glm::rotate(mat4(1.0f), machinegun.rotation.y, vec3(0.0f, 0.0f, 1.0f))
			* glm::rotate(mat4(1.0f), machinegun.rotation.z, vec3(1.0f, 0.0f, 0.0f))
			* glm::scale(mat4(1.0f), vec3(machinegun.scale));
		setWorldMatrix(world_matrix);
		static_asset_collection.mg42_mesh.draw();
	}
}
