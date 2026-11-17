#pragma once
#include <danikk_space_engine/world/world.h>
#include <danikk_space_engine/component/point/human/human.h>
namespace danikk_space_engine::human_internal
{
	void moveWithGround(World&, const Chunk&, const WorldBody&, HumanComponent&, Point&, const vec3&, float, float);
	void drawEquippedWeapons(const PointComponentSlot&, const HumanComponent&, const mat4&, World&);
}
