#pragma once

// If chunk size changes, everything below needs to be changed. Everything else is done automatically.
// - Light nodes in VoxelLighting
constexpr int CHUNK_SIZE = 16;
constexpr int CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE;
constexpr int CHUNK_VOLUME = CHUNK_AREA * CHUNK_SIZE;

constexpr int CHUNK_SHIFT = 4;
constexpr int CHUNK_MASK = CHUNK_SIZE - 1;

using BlockId = uint8_t;
constexpr BlockId INVALID_BLOCK_POS = 0;
constexpr BlockId AIR = 1;