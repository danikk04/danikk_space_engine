#pragma once

#include <danikk_space_engine/world/face_ptr.h>
#include <danikk_framework/dynamic_array.h>

namespace danikk_space_engine
{
	class World;
	struct RaycastHit;

	enum class ToolId : uint8
	{
		none = 0,
		select,
		expand,
		erase_faces,
		create,
		count
	};

	enum class CreateObjectId : uint8
	{
		none = 0,
		machinegun,
		volume_cell,
		count
	};

	struct ToolApplyResult
	{
		bool replace_selected_faces = false;
		DynamicArray<FacePtr> selected_faces;
	};

	ToolId stepTool(ToolId current, int direction);
	CreateObjectId stepCreateObject(CreateObjectId current, int direction);
	const char* getToolName(ToolId tool);
	const char* getCreateObjectName(CreateObjectId object);
	ToolApplyResult applyTool(
		ToolId tool,
		World& world,
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face,
		const RaycastHit& hit,
		CreateObjectId create_object);
	ToolApplyResult applySelectTool(
		const DynamicArray<FacePtr>& selected_faces,
		size_t selected_face_count,
		bool has_hovered_face,
		const FacePtr& hovered_face);

	ToolApplyResult applyNoneTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection);
	ToolApplyResult applyExpandTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection);
	ToolApplyResult applyEraseTool(
		World& world,
		const DynamicArray<FacePtr>& faces,
		bool using_selection);
	ToolApplyResult applyCreateTool(World& world, const RaycastHit& hit, CreateObjectId object);
}
