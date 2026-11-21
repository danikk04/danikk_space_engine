#pragma once

#include <danikk_framework/dynamic_array.h>

#include <danikk_space_engine/component/point/point.h>
#include <danikk_space_engine/id.h>
#include <danikk_space_engine/material.h>
#include <danikk_space_engine/fwd.h>

namespace danikk_space_engine { class ChunkSystem; }

namespace danikk_space_engine
{

	/**
	 * @brief Stores spatial point references and BuildPart geometry for one chunk.
	 *
	 * A Chunk is positioned relative to its WorldBody. Point and BuildPart
	 * membership indices and render data are owned by ChunkSystem; the chunk keeps
	 * only body-relative spatial metadata.
	 * Planet streaming metadata uses grid_coordinate.
	 */
	class Chunk
	{
	public:
		/** @brief Stable pooled chunk ID. */
		ChunkId chunk_id = 0;
		/** @brief World body that supplies this chunk's parent transform. */
		WorldBodyId world_body_id = 0;
		/** @brief Integer grid coordinate for streamed planet chunks. */
		glm::ivec3 grid_coordinate = glm::ivec3(0);
		/** @brief Chunk origin relative to its WorldBody origin, in double precision. */
		glm::dvec3 position = glm::dvec3(0.0);

		/** @brief Creates an empty chunk resource. */
		Chunk() = default;
		/** @brief Copying is disabled because the chunk represents a unique pooled resource. */
		Chunk(const Chunk&) = delete;
		/** @brief Copy assignment is disabled for the same ownership reason as copy construction. */
		Chunk& operator=(const Chunk&) = delete;
		/** @brief Transfers spatial metadata from another chunk. */
		Chunk(Chunk&& other) noexcept = default;
		/** @brief Replaces spatial metadata with another chunk's values. */
		Chunk& operator=(Chunk&& other) noexcept = default;
		/** @brief Destroys CPU-side chunk data. */
		~Chunk() = default;

		/**
		 * @brief Builds the chunk-to-camera-relative render transform from body and chunk transforms.
		 * @param world World providing the owning body transform and camera position.
		 * @return Matrix mapping chunk-local coordinates into camera-relative render coordinates.
		 */
		mat4 createWorldMatrix(const World& world) const;

	};
}
