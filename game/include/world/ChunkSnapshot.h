#pragma once

#include <world/WorldDefines.h>
#include <wv/core.h>

namespace ScuffedMinecraft
{
	constexpr int PAD = CHUNK_SIZE + 2;

	struct ChunkSnapshot
	{
		glm::ivec3 id;
		uint32_t dataVersion;
		BlockId blocks[PAD * PAD * PAD];
		uint8_t light[PAD * PAD * PAD];
	};

	constexpr int PadIndex(int x, int y, int z) { return (y + 1) + PAD * ((x + 1) + PAD * (z + 1)); }
}