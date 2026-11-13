#include <danikk_space_engine/component/local_player_controller.h>
#include <danikk_space_engine/component/machinegun.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/input.h>

namespace danikk_space_engine
{
	namespace
	{
		bool isAltDown()
		{
			const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
			const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
			return left_alt_state == button_states::press || left_alt_state == button_states::hold
				|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		}

		Chunk* findOwningChunk(PointComponentSlot& slot, World& world)
		{
			for(ChunkId chunk_id = 0; chunk_id < world.data.getChunkCount(); chunk_id++)
			{
				Chunk& chunk = world.data.getChunk(chunk_id);
				for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
				{
					if(slot_id < world.data.getPointComponentSlotCount()
						&& &world.data.getPointComponentSlot(slot_id) == &slot)
					{
						return &chunk;
					}
				}
			}
			return nullptr;
		}

		void duplicateControllerPointWithMachinegun(
			const Point& source_point,
			const vec3& rotation,
			Chunk& chunk,
			World& world)
		{
			const PointId point_id = world.data.createPoint();
			world.data.getPoint(point_id) = source_point;
			MachinegunComponent machinegun;
			machinegun.rotation = rotation;
			const PointComponentSlotId machinegun_slot_id = world.data.createPointComponentSlot(
				point_id,
				PointComponentTypeId::machine_gun,
				&machinegun);
			chunk.addPointComponentSlot(machinegun_slot_id);
		}
	}

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

		const int machinegun_state = getKeyboardState(keyboard_buttons::m);
		const bool spawn_machinegun = isAltDown() && machinegun_state == button_states::press;
		Point source_point;
		vec3 source_rotation(0.0f);
		Chunk* owner_chunk = nullptr;
		if(spawn_machinegun)
		{
			if(slot.point_id >= world.data.getPointCount())
			{
				danikk_engine::formatFatalError("Local player controller references invalid point %", slot.point_id);
			}
			source_point = world.data.getPoint(slot.point_id);
			source_rotation = world.getCameraComponent().rotation;
			owner_chunk = findOwningChunk(slot, world);
			if(owner_chunk == nullptr)
			{
				danikk_engine::formatFatalError("Local player controller is not registered in a chunk");
			}
		}

		static_cast<LocalPlayerControllerComponent*>(slot.getData())->tick(world);
		if(spawn_machinegun)
		{
			duplicateControllerPointWithMachinegun(source_point, source_rotation, *owner_chunk, world);
		}
	}

	// Keeps selected faces at the start of selected_faces and appends the hovered face
	// when it is not already selected. This lets editing operate on the selected prefix,
	// while rendering gets both selection and hover state from the same array.
	void LocalPlayerControllerComponent::tick(World& world)
	{
		if(selected_faces.size() > selected_face_count)
		{
			selected_faces.resize(selected_face_count);
		}

		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const bool alt_down = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		const RaycastHit edit_hit = world.raycastFromCamera();
		const bool edit_uses_selection = selected_face_count > 0;
		DynamicArray<FacePtr> hovered_face;
		if(!edit_uses_selection && edit_hit.hit)
		{
			hovered_face.push(edit_hit.face);
		}
		const DynamicArray<FacePtr>& edit_faces = selected_face_count > 0
			? selected_faces
			: hovered_face;

		if(alt_down
			&& getKeyboardState(keyboard_buttons::e) == button_states::press
			&& edit_faces.size() > 0)
		{
			DynamicArray<FacePtr> extruded_faces = world.extrudeFaces(edit_faces, 1.0f);
			if(edit_uses_selection && extruded_faces.size() > 0)
			{
				selected_faces = static_cast<DynamicArray<FacePtr>&&>(extruded_faces);
				selected_face_count = selected_faces.size();
			}
		}
		if(alt_down
			&& getKeyboardState(keyboard_buttons::d) == button_states::press
			&& edit_faces.size() > 0)
		{
			world.deleteVolumeCells(edit_faces);
		}

		const RaycastHit hit = world.raycastFromCamera();
		if(getMouseState(mouse_buttons::left) == button_states::press)
		{
			if(hit.hit)
			{
				size_t selected_index = selected_face_count;
				for(size_t i = 0; i < selected_face_count; i++)
				{
					const FacePtr& selected = selected_faces[i];
					if(selected == hit.face)
					{
						selected_index = i;
						break;
					}
				}

				if(selected_index < selected_face_count)
				{
					for(size_t i = selected_index; i + 1 < selected_face_count; i++)
					{
						selected_faces[i] = selected_faces[i + 1];
					}
					selected_faces.resize(selected_faces.size() - 1);
					selected_face_count--;
				}
				else
				{
					selected_faces.push(hit.face);
					selected_face_count++;
				}
			}
			else
			{
				selected_faces = DynamicArray<FacePtr>();
				selected_face_count = 0;
			}
		}

		if(hit.hit)
		{
			bool already_selected = false;
			for(size_t i = 0; i < selected_face_count; i++)
			{
				if(selected_faces[i] == hit.face)
				{
					already_selected = true;
					break;
				}
			}
			if(!already_selected)
			{
				selected_faces.push(hit.face);
			}
		}
		else if(selected_face_count == 0 && selected_faces.size() == 0)
		{
			selected_faces = DynamicArray<FacePtr>();
		}
	}
}
