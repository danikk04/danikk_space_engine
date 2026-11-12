#pragma once

#include <danikk_space_engine/volume_cell.h>

namespace danikk_space_engine
{
	struct FacePtr
	{
		uint32 region_index = 0;
		VolumeCellId cell_id = 0;
		uint8 face_index = 0;

		bool operator==(const FacePtr& other) const
		{
			return region_index == other.region_index
				&& cell_id == other.cell_id
				&& face_index == other.face_index;
		}
	};
}
