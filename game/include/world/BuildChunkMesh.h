#pragma once

#include <world/ChunkSnapshot.h>
#include <wv/core.h>

namespace ScuffedMinecraft
{
	std::unique_ptr<WillowVox::Mesh> BuildChunkMesh(const ChunkSnapshot& snapshot, bool smoothLighting);
}