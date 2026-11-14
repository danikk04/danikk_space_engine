#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/world/world.h>

#include <danikk_engine/input.h>

namespace danikk_space_engine
{
	namespace
	{
		ChunkId findOwningChunkId(PointComponentSlot& slot, World& world)
		{
			for(WorldBodyId body_id : world.data.getUsedWorldBodyIds())
			{
				const WorldBody& body = world.data.getWorldBody(body_id);
				for(ChunkId chunk_id : body.chunk_ids)
				{
					Chunk& chunk = world.data.getChunk(chunk_id);
					for(PointComponentSlotId slot_id : chunk.point_component_slot_ids)
					{
						if(world.data.isPointComponentSlotActive(slot_id)
							&& &world.data.getPointComponentSlot(slot_id) == &slot) return chunk_id;
					}
				}
			}
			return static_cast<ChunkId>(-1);
		}

		void duplicateControllerPointWithMachinegun(
			const Point& source_point,
			const CameraComponent& camera,
			ChunkId chunk_id,
			World& world)
		{
			const PointId point_id = world.data.createPoint();
			world.data.getPoint(point_id) = source_point;
			world.data.getPoint(point_id).chunk_reference_count = 0;
			MachinegunComponent machinegun;
			machinegun.forward = camera.getFront();
			machinegun.up = camera.getUp();
			machinegun.right = camera.getRight();
			const PointComponentSlotId slot_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::machine_gun, &machinegun);
			world.data.addPointComponentSlotToChunk(chunk_id, slot_id);
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

		const bool spawn_machinegun = getKeyboardState(keyboard_buttons::m) == button_states::press;
		Point source_point;
		CameraComponent source_camera;
		ChunkId owner_chunk_id = static_cast<ChunkId>(-1);
		if(spawn_machinegun)
		{
			if(!world.data.isPointActive(slot.point_id))
			{
				danikk_engine::formatFatalError("Local player controller references invalid point %", slot.point_id);
			}
			source_point = world.data.getPoint(slot.point_id);
			source_camera = world.getCameraComponent();
			owner_chunk_id = findOwningChunkId(slot, world);
			if(owner_chunk_id == static_cast<ChunkId>(-1))
			{
				danikk_engine::formatFatalError("Local player controller is not registered in a chunk");
			}
		}

		static_cast<LocalPlayerControllerComponent*>(slot.getData())->tick(world);
		if(spawn_machinegun)
		{
			duplicateControllerPointWithMachinegun(source_point, source_camera, owner_chunk_id, world);
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
		const bool alt_pressed = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		if(alt_pressed)
		{
			create_object_scroll_accumulator += getMouseScrollDelta();
			while(create_object_scroll_accumulator >= 1.0f)
			{
				selected_create_object = stepCreateObject(selected_create_object, 1);
				create_object_scroll_accumulator -= 1.0f;
			}
			while(create_object_scroll_accumulator <= -1.0f)
			{
				selected_create_object = stepCreateObject(selected_create_object, -1);
				create_object_scroll_accumulator += 1.0f;
			}
		}
		else
		{
			tool_scroll_accumulator += getMouseScrollDelta();
			while(tool_scroll_accumulator >= 1.0f)
			{
				active_tool = stepTool(active_tool, 1);
				tool_scroll_accumulator -= 1.0f;
			}
			while(tool_scroll_accumulator <= -1.0f)
			{
				active_tool = stepTool(active_tool, -1);
				tool_scroll_accumulator += 1.0f;
			}
		}

		RaycastHit hit = world.raycastFromCamera();
		if(active_tool != ToolId::none && getMouseState(mouse_buttons::left) == button_states::press)
		{
			ToolApplyResult result = applyTool(
				active_tool,
				world,
				selected_faces,
				selected_face_count,
				hit.hit,
				hit.face,
				hit,
				selected_create_object);
			if(result.replace_selected_faces)
			{
				selected_faces = static_cast<DynamicArray<FacePtr>&&>(result.selected_faces);
				selected_face_count = selected_faces.size();
			}
			hit = world.raycastFromCamera();
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
