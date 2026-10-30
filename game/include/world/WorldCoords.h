#pragma once

#include <world/WorldDefines.h>
#include <wv/pch.h>

namespace ScuffedMinecraft
{
	constexpr int BlockToChunkId(int b) { return b >> CHUNK_SHIFT; }
	constexpr int BlockToLocal(int b) { return b & CHUNK_MASK; }
	constexpr int ChunkToBlock(int c) { return c << CHUNK_SHIFT; }

	constexpr glm::ivec3 BlockToChunkId(const glm::ivec3& b) { return { b.x >> CHUNK_SHIFT, b.y >> CHUNK_SHIFT, b.z >> CHUNK_SHIFT }; }
	constexpr glm::ivec3 BlockToChunkId(int x, int y, int z) { return { x >> CHUNK_SHIFT, y >> CHUNK_SHIFT, z >> CHUNK_SHIFT }; }

	constexpr glm::ivec3 BlockToLocal(const glm::ivec3& b) { return { b.x & CHUNK_MASK, b.y & CHUNK_MASK, b.z & CHUNK_MASK }; }
	constexpr glm::ivec3 BlockToLocal(int x, int y, int z) { return { x & CHUNK_MASK, y & CHUNK_MASK, z & CHUNK_MASK }; }

	constexpr glm::ivec3 ChunkToBlock(const glm::ivec3& c) { return { c.x << CHUNK_SHIFT, c.y << CHUNK_SHIFT, c.z << CHUNK_SHIFT }; }

	inline glm::ivec3 WorldToBlock(const glm::vec3 pos)
	{
		return { std::floor(pos.x), std::floor(pos.y), std::floor(pos.z) };
	}

	inline glm::ivec3 WorldToBlock(float x, float y, float z)
	{
		return { std::floor(x), std::floor(y), std::floor(z) };
	}
}