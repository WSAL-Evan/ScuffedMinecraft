#pragma once

#include <world/ChunkManager.h>
#include <wv/pch.h>

namespace ScuffedMinecraft
{
	enum Direction : uint8_t { NEG_X, POS_X, NEG_Y, POS_Y, NEG_Z, POS_Z };

	constexpr glm::ivec3 dirOffsets[6] = {
		{ -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
	};

	constexpr Direction Opposite(Direction d) { return Direction(d ^ 1); }
}