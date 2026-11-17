#include <danikk_space_engine/profiler.h>
#include <danikk_space_engine/component/point/local_player_controller.h>
#include <danikk_space_engine/component/point/machinegun.h>
#include <danikk_space_engine/world/world.h>

#include <cmath>

#include <danikk_engine/input.h>

namespace danikk_space_engine
{
	namespace
	{

	/**
	 * @brief Searches active body/chunk ownership lists for the chunk registering this controller.
	 * @param slot Controller slot whose address is compared with chunk membership.
	 * @param world World whose bodies and chunks are scanned.
	 * @return Owning chunk ID or the invalid sentinel when the slot is unattached.
	 */
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

		/**
		 * @brief Duplicates a point and attaches a machine gun aligned to the camera basis.
		 * @param source_point Controller point copied as the new weapon position.
		 * @param camera Camera orientation used to initialize weapon forward/up/right.
		 * @param chunk_id Chunk that receives the new point component.
		 * @param world WorldData allocator and chunk registry.
		 */
		void duplicateControllerPointWithMachinegun(
			const Point& source_point,
			const CameraComponent& camera,
			ChunkId chunk_id,
			World& world)
		{
			const PointId point_id = world.data.createPoint();
			world.data.getPoint(point_id) = source_point;
			world.data.getPoint(point_id).referencing_chunk_count = 0;
			MachinegunComponent machinegun;
			machinegun.forward = camera.getFront();
			machinegun.up = camera.getUp();
			machinegun.right = camera.getRight();
			const PointComponentSlotId slot_id = world.data.createPointComponentSlot(
				point_id, PointComponentTypeId::machine_gun, &machinegun);
			world.data.addPointComponentSlotToChunk(chunk_id, slot_id);
		}
	}

	/**
	 * @brief Creates default controller state or copies supplied tool, selection, and drag state.
	 * @param destination Destination storage owned by the point-component slot.
	 * @param init_data Optional initial controller state; null selects member defaults.
	 */
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

	/**
	 * @brief Transfers controller state and its selected-face array to relocated slot storage.
	 * @param destination Replacement payload storage.
	 * @param source Existing controller payload, destroyed after the move.
	 */
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

	/**
	 * @brief Destroys controller state, including the dynamic selected-face array.
	 * @param data Existing live controller payload.
	 */
	void destroyLocalPlayerControllerComponent(void* data)
	{
		if(data == nullptr)
		{
			danikk_engine::formatFatalError("Local player controller destruction received null memory");
		}
		static_cast<LocalPlayerControllerComponent*>(data)->~LocalPlayerControllerComponent();
	}

	/**
	 * @brief Validates the controller, processes its input, then handles a pressed M key by spawning a machine gun.
	 * @param slot Controller slot whose point/chunk ownership is used for duplication.
	 * @param world World providing camera orientation and allocating the weapon point/component.
	 */
	void tickLocalPlayerControllerComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		Watcher watcher("tickLocalPlayerControllerComponent");

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

	/**
	 * @brief Updates tool/camera controls, runs drag behavior, applies pressed actions, and refreshes the hover face.
	 *
	 * Ctrl-wheel multiplies camera speed by `1.25^scroll_delta`, clamped to
	 * 0.01–1,000,000 m/s. Wheel cycling is not used; commands are selected from
	 * the tool catalog and assigned to hotbar slots. During a drag, wheel and
	 * mouse input go to that drag handler.
	 * During Place -> Cube/Slope, X/Y/Z selects the size axis and Shift-wheel resizes it in 0.25 m steps.
	 * Each tick raycasts from the camera. A left-button press applies the current
	 * tool when no drag is active. Persistent selected faces occupy the array prefix;
	 * a distinct hovered face is appended last for rendering and tool dispatch.
	 * @param world World providing input context, camera raycast, and tool operations.
	 */
	void LocalPlayerControllerComponent::tick(World& world)
	{
		/**
		 * @brief Measures the duration of the enclosing profiler scope.
		 */
		Watcher watcher("LocalPlayerControllerComponent::tick");
		if(tool_catalog_open) return;

		if(selected_faces.size() > selected_face_count)
		{
			selected_faces.resize(selected_face_count);
		}

		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const int left_control_state = getKeyboardState(keyboard_buttons::left_control);
		const int right_control_state = getKeyboardState(keyboard_buttons::right_control);
		const int left_shift_state = getKeyboardState(keyboard_buttons::left_shift);
		const int right_shift_state = getKeyboardState(keyboard_buttons::right_shift);
		const int left_mouse_state = getMouseState(mouse_buttons::left);
		const bool left_mouse_down = left_mouse_state == button_states::press
			|| left_mouse_state == button_states::hold;
		const float mouse_scroll_delta = getMouseScrollDelta();
		const bool alt_pressed = left_alt_state == button_states::press || left_alt_state == button_states::hold
			|| right_alt_state == button_states::press || right_alt_state == button_states::hold;
		const bool control_pressed = left_control_state == button_states::press
			|| left_control_state == button_states::hold
			|| right_control_state == button_states::press
			|| right_control_state == button_states::hold;
		const bool shift_pressed = left_shift_state == button_states::press
			|| left_shift_state == button_states::hold
			|| right_shift_state == button_states::press
			|| right_shift_state == button_states::hold;
		const bool placing_build_part = active_tool == ToolId::place
			&& (selected_tool_action == ToolActionId::create_body
				|| selected_tool_action == ToolActionId::create_slope);
		if(placing_build_part && !alt_pressed && !control_pressed && !shift_pressed)
		{
			if(getKeyboardState(keyboard_buttons::x) == button_states::press) build_part_size_axis = 0;
			if(getKeyboardState(keyboard_buttons::y) == button_states::press) build_part_size_axis = 1;
			if(getKeyboardState(keyboard_buttons::z) == button_states::press) build_part_size_axis = 2;
			build_part_size_scroll_accumulator = 0.0f;
		}
		const bool adjusting_build_part_size = placing_build_part
			&& shift_pressed && !control_pressed && !alt_pressed;
		if(adjusting_build_part_size)
		{
			build_part_size_scroll_accumulator += mouse_scroll_delta;
			while(build_part_size_scroll_accumulator >= 1.0f)
			{
				build_part_size[build_part_size_axis] = glm::min(
					build_part_size[build_part_size_axis] + 0.25f, 100.0f);
				build_part_size_scroll_accumulator -= 1.0f;
			}
			while(build_part_size_scroll_accumulator <= -1.0f)
			{
				build_part_size[build_part_size_axis] = glm::max(
					build_part_size[build_part_size_axis] - 0.25f, 0.25f);
				build_part_size_scroll_accumulator += 1.0f;
			}
		}
		if(!alt_pressed && !(control_pressed && shift_pressed))
		{
			constexpr int hotbar_key_codes[12] =
			{
				keyboard_buttons::number1,
				keyboard_buttons::number2,
				keyboard_buttons::number3,
				keyboard_buttons::number4,
				keyboard_buttons::number5,
				keyboard_buttons::number6,
				keyboard_buttons::number7,
				keyboard_buttons::number8,
				keyboard_buttons::number9,
				keyboard_buttons::number0,
				keyboard_buttons::minus,
				keyboard_buttons::equal
			};
			const uint32 row_offset = control_pressed ? 24 : (shift_pressed ? 12 : 0);
			for(uint32 index = 0; index < 12; ++index)
			{
				if(getKeyboardState(hotbar_key_codes[index]) == button_states::press)
				{
					selectHotbarSlot(row_offset + index);
					break;
				}
			}
		}
		const float tool_mouse_scroll_delta = control_pressed || adjusting_build_part_size
			? 0.0f : mouse_scroll_delta;
		const bool drag_tool_selected = active_tool == ToolId::drag
			&& selected_tool_action == ToolActionId::drag;
		const bool drag_tool_down = drag_tool_selected && left_mouse_down;
		if(control_pressed)
		{
			if(mouse_scroll_delta != 0.0f)
			{
				camera_movement_speed = glm::clamp(
					camera_movement_speed * std::pow(1.25f, mouse_scroll_delta),
					0.01f,
					1000000.0f);
			}
		}
		else if(adjusting_build_part_size)
		{
			// Shift-wheel was consumed above to adjust the active BuildPart dimension.
		}

		RaycastHit hit = world.raycastFromCamera();
		if(drag_tool_down)
		{
			tickHumanDragTool(world, *this, hit, true, left_mouse_state,
				alt_pressed ? tool_mouse_scroll_delta : 0.0f, alt_pressed);
			const bool dragging_human = dragged_human_slot_id != static_cast<PointComponentSlotId>(-1);
			tickDragTool(
				world, *this, hit, !dragging_human, left_mouse_state,
				tool_mouse_scroll_delta, alt_pressed,
				getCursorDelta() * getMouseSensivity());
		}
		else
		{
			tickDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false, vec2(0.0f));
			tickHumanDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false);
			if(active_tool != ToolId::none && active_tool != ToolId::drag
				&& left_mouse_state == button_states::press)
			{
				ToolApplyResult result = applyTool(
					active_tool,
					selected_tool_action,
					world,
					hit,
					build_part_size);
				if(result.replace_selected_faces)
				{
					selected_faces = static_cast<DynamicArray<FacePtr>&&>(result.selected_faces);
					selected_face_count = selected_faces.size();
				}
				hit = world.raycastFromCamera();
			}
		}

		if(hit.hit && !hit.hit_build_part)
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

	/** @copydoc LocalPlayerControllerComponent::selectHotbarSlot */
	void LocalPlayerControllerComponent::selectHotbarSlot(uint32 slot_index)
	{
		if(slot_index >= 36)
		{
			danikk_engine::formatFatalError("Hotbar slot index % is out of range", slot_index);
		}
		selected_hotbar_slot = slot_index;
		const ToolCatalogEntry& entry = getToolCatalogEntry(hotbar_entry_ids[slot_index]);
		active_tool = entry.tool;
		selected_tool_action = entry.action;
	}

	/** @copydoc LocalPlayerControllerComponent::selectToolCatalogEntry */
	void LocalPlayerControllerComponent::selectToolCatalogEntry(ToolCatalogEntryId entry_id)
	{
		const ToolCatalogEntry& entry = getToolCatalogEntry(entry_id);
		hotbar_entry_ids[selected_hotbar_slot] = entry_id;
		active_tool = entry.tool;
		selected_tool_action = entry.action;
	}
}
