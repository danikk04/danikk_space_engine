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
		 * @brief Finds owning chunk id.
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
		 * @brief Performs the duplicate controller point with machinegun operation.
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
	 * @brief Constructs local player controller component.
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
	 * @brief Moves local player controller component.
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
	 * @brief Removes and releases local player controller component.
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
	 * @brief Runs one simulation tick for local player controller component.
	 */
	void tickLocalPlayerControllerComponent(PointComponentSlot& slot, World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
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

	// Keeps selected faces at the start of selected_faces and appends the hovered face
	// when it is not already selected. This lets editing operate on the selected prefix,
	// while rendering gets both selection and hover state from the same array.

	/**
	 * @brief Performs the tick operation.
	 */
	void LocalPlayerControllerComponent::tick(World& world)
	{
		/**
		 * @brief Disables copying to ensure a measured scope is recorded once.
		 */
		Watcher watcher("LocalPlayerControllerComponent::tick");

		if(selected_faces.size() > selected_face_count)
		{
			selected_faces.resize(selected_face_count);
		}

		const int left_alt_state = getKeyboardState(keyboard_buttons::left_alt);
		const int right_alt_state = getKeyboardState(keyboard_buttons::right_alt);
		const int left_control_state = getKeyboardState(keyboard_buttons::left_control);
		const int right_control_state = getKeyboardState(keyboard_buttons::right_control);
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
		const float tool_mouse_scroll_delta = control_pressed ? 0.0f : mouse_scroll_delta;
		const bool body_dragging = active_tool == ToolId::body
			&& selected_tool_action == ToolActionId::drag && left_mouse_down;
		const bool human_dragging = active_tool == ToolId::human
			&& selected_tool_action == ToolActionId::drag && left_mouse_down;
		const bool dragging = body_dragging || human_dragging;
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
		else if(!dragging && alt_pressed)
		{
			tool_action_scroll_accumulator += tool_mouse_scroll_delta;
			while(tool_action_scroll_accumulator >= 1.0f)
			{
				selected_tool_action = stepToolAction(active_tool, selected_tool_action, 1);
				tool_action_scroll_accumulator -= 1.0f;
			}
			while(tool_action_scroll_accumulator <= -1.0f)
			{
				selected_tool_action = stepToolAction(active_tool, selected_tool_action, -1);
				tool_action_scroll_accumulator += 1.0f;
			}
		}
		else if(!dragging)
		{
			tool_scroll_accumulator += tool_mouse_scroll_delta;
			while(tool_scroll_accumulator >= 1.0f)
			{
				active_tool = stepTool(active_tool, 1);
				selected_tool_action = getFirstToolAction(active_tool);
				tool_scroll_accumulator -= 1.0f;
			}
			while(tool_scroll_accumulator <= -1.0f)
			{
				active_tool = stepTool(active_tool, -1);
				selected_tool_action = getFirstToolAction(active_tool);
				tool_scroll_accumulator += 1.0f;
			}
		}

		RaycastHit hit = world.raycastFromCamera();
		if(body_dragging)
		{
			tickDragTool(
				world, *this, hit, true, left_mouse_state,
				left_mouse_down ? tool_mouse_scroll_delta : 0.0f,
				alt_pressed,
				getCursorDelta() * getMouseSensivity());
			tickHumanDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false);
		}
		else if(human_dragging)
		{
			tickDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false, vec2(0.0f));
			tickHumanDragTool(world, *this, hit, true, left_mouse_state,
				alt_pressed ? tool_mouse_scroll_delta : 0.0f, alt_pressed);
		}
		else
		{
			tickDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false, vec2(0.0f));
			tickHumanDragTool(world, *this, hit, false, left_mouse_state, 0.0f, false);
			if(active_tool != ToolId::none && left_mouse_state == button_states::press)
			{
				ToolApplyResult result = applyTool(
					active_tool,
					selected_tool_action,
					world,
					selected_faces,
					selected_face_count,
					hit.hit,
					hit.face,
					hit);
				if(result.replace_selected_faces)
				{
					selected_faces = static_cast<DynamicArray<FacePtr>&&>(result.selected_faces);
					selected_face_count = selected_faces.size();
				}
				hit = world.raycastFromCamera();
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
