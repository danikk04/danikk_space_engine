#pragma once

#include <default.h>
#include <object/world.h>
#include <danikk_engine/mesh.h>
#include <danikk_framework/tensor.h>
#include <block/type.h>

namespace game
{
	struct Voxel
	{
		uint32 type = 0;
	};

	class BlockMapChuck
	{
		static constexpr size_t axis_size = 64;
		static constexpr uvec3 size = uvec3(axis_size);
		FixedTensor<Voxel, size> data;
		Mesh mesh;

		void regenerateMesh();
	};

	class BlockMapObject : public WorldObject
	{
		static constexpr size_t axis_size = 64;
		static constexpr uvec3 size = uvec3(axis_size);
	public:
		FixedTensor<BlockMapChuck, size> data;

		void tick() override;

		void frame() override;

	};
}
