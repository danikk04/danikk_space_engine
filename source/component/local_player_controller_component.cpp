#include <danikk_space_engine/component/local_player_controller_component.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/input.h>

namespace danikk_space_engine
{
	void constructLocalPlayerControllerComponent(void* destination, const void* init_data)
	{
		if(destination == nullptr)
		{
			danikk_engine::formatFatalError("Local player controller construction received null destination");
		}
		if(init_data == nullptr)
		{
			new (destination) LocalPlayerControllerComponent();
		}
		else
		{
			new (destination) LocalPlayerControllerComponent(*static_cast<const LocalPlayerControllerComponent*>(init_data));
		}
	}

	void moveLocalPlayerControllerComponent(void* destination, void* source)
	{
		if(destination == nullptr || source == nullptr)
		{
			danikk_engine::formatFatalError("Local player controller move received null memory");
		}
		LocalPlayerControllerComponent* source_controller = static_cast<LocalPlayerControllerComponent*>(source);
		new (destination) LocalPlayerControllerComponent(static_cast<LocalPlayerControllerComponent&&>(*source_controller));
		source_controller->~LocalPlayerControllerComponent();
	}

	void destroyLocalPlayerControllerComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Local player controller destruction received null memory");
		}
		static_cast<LocalPlayerControllerComponent*>(data)->~LocalPlayerControllerComponent();
	}

	void tickLocalPlayerControllerComponent(PointComponentSlot& slot, World& world)
	{
		if(slot.getData() == nullptr)
		{
			danikk_engine::formatFatalError("Local player controller tick received null data");
		}
		static_cast<LocalPlayerControllerComponent*>(slot.getData())->tick(world);
	}

	// Handles Alt+E extrusion, Alt+D deletion, and left-click face selection.
	// Raycasts after the camera and chunk ticks, toggles the hit FacePtr, and clears
	// all selected faces when clicking empty space.
	void LocalPlayerControllerComponent::tick(World& world)
	{
		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const bool alt_down = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		if(alt_down
			&& getKeyboardState(keyboard_buttons::e) == button_states::press
			&& selected_polygons.size() > 0)
		{
			DynamicArray<FacePtr> extruded_faces = world.extrudeFaces(selected_polygons, 1.0f);
			if(extruded_faces.size() > 0)
			{
				selected_polygons = static_cast<DynamicArray<FacePtr>&&>(extruded_faces);
			}
		}
		if(alt_down
			&& getKeyboardState(keyboard_buttons::d) == button_states::press
			&& selected_polygons.size() > 0)
		{
			world.deleteVolumeCells(selected_polygons);
		}

		const RaycastHit hit = world.raycastFromCamera();
		if(getMouseState(mouse_buttons::left) == button_states::press)
		{
			if(hit.hit)
			{
				size_t selected_index = selected_polygons.size();
				for(size_t i = 0; i < selected_polygons.size(); i++)
				{
					const FacePtr& selected = selected_polygons[i];
					if(selected == hit.face)
					{
						selected_index = i;
						break;
					}
				}

				if(selected_index < selected_polygons.size())
				{
					for(size_t i = selected_index; i + 1 < selected_polygons.size(); i++)
					{
						selected_polygons[i] = selected_polygons[i + 1];
					}
					selected_polygons.resize(selected_polygons.size() - 1);
					if(selected_polygons.size() == 0)
					{
						selected_polygons = DynamicArray<FacePtr>();
					}
				}
				else
				{
					selected_polygons.push(hit.face);
				}
			}
			else
			{
				selected_polygons = DynamicArray<FacePtr>();
			}
		}

	}
}
