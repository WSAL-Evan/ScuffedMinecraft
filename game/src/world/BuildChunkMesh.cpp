#include <world/BuildChunkMesh.h>

#include <world/BlockRegistry.h>
#include <world/ChunkVertex.h>
#include <world/Direction.h>

namespace ScuffedMinecraft
{
	constexpr glm::vec3 VertexPositions[6][4] = {
		{ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 1.0f) },
		{ glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f) },
		{ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f) },
		{ glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f) },
		{ glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f) },
		{ glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f) }
	};

	static constexpr glm::vec2 GetTexturePos(BlockDisplay* blockDisplay, Direction dir, int vertex)
	{
		float minX = (dir == NEG_Y ? blockDisplay->bottomTextureMinX : (dir == POS_Y ? blockDisplay->topTextureMinX : blockDisplay->sideTextureMinX));
		float maxX = (dir == NEG_Y ? blockDisplay->bottomTextureMaxX : (dir == POS_Y ? blockDisplay->topTextureMaxX : blockDisplay->sideTextureMaxX));
		float minY = (dir == NEG_Y ? blockDisplay->bottomTextureMinY : (dir == POS_Y ? blockDisplay->topTextureMinY : blockDisplay->sideTextureMinY));
		float maxY = (dir == NEG_Y ? blockDisplay->bottomTextureMaxY : (dir == POS_Y ? blockDisplay->topTextureMaxY : blockDisplay->sideTextureMaxY));

		switch (vertex)
		{
		case 0:
			return { minX, minY };
		case 1:
			return { maxX, minY };
		case 2:
			return { minX, maxY };
		default:
			return { maxX, maxY };
		}
	}

	static constexpr int SmoothLightingIndexOffsets[6][9] = {
		{ PadIndex(-2, -2, -2), PadIndex(-2, -2, -1), PadIndex(-2, -2, 0), PadIndex(-2, -1, -2), PadIndex(-2, -1, -1), PadIndex(-2, -1, 0), PadIndex(-2, 0, -2), PadIndex(-2, 0, -1), PadIndex(-2, 0, 0) },
		{ PadIndex(0, -2, 0), PadIndex(0, -2, -1), PadIndex(0, -2, -2), PadIndex(0, -1, 0), PadIndex(0, -1, -1), PadIndex(0, -1, -2), PadIndex(0, 0, 0), PadIndex(0, 0, -1), PadIndex(0, 0, -2) },
		{ PadIndex(-2, -2, -2), PadIndex(-1, -2, -2), PadIndex(0, -2, -2), PadIndex(-2, -2, -1), PadIndex(-1, -2, -1), PadIndex(0, -2, -1), PadIndex(-2, -2, 0), PadIndex(-1, -2, 0), PadIndex(0, -2, 0) },
		{ PadIndex(-2, 0, 0), PadIndex(-1, 0, 0), PadIndex(0, 0, 0), PadIndex(-2, 0, -1), PadIndex(-1, 0, -1), PadIndex(0, 0, -1), PadIndex(-2, 0, -2), PadIndex(-1, 0, -2), PadIndex(0, 0, -2) },
		{ PadIndex(0, -2, -2), PadIndex(-1, -2, -2), PadIndex(-2, -2, -2), PadIndex(0, -1, -2), PadIndex(-1, -1, -2), PadIndex(-2, -1, -2), PadIndex(0, 0, -2), PadIndex(-1, 0, -2), PadIndex(-2, 0, -2) },
		{ PadIndex(-2, -2, 0), PadIndex(-1, -2, 0), PadIndex(0, -2, 0), PadIndex(-2, -1, 0), PadIndex(-1, -1, 0), PadIndex(0, -1, 0), PadIndex(-2, 0, 0), PadIndex(-1, 0, 0), PadIndex(0, 0, 0) }
	};

	static inline void AddIndices(std::vector<uint32_t>& indices, uint32_t& vertexCount)
	{
		indices.push_back(vertexCount + 0);
		indices.push_back(vertexCount + 2);
		indices.push_back(vertexCount + 1);
		indices.push_back(vertexCount + 1);
		indices.push_back(vertexCount + 2);
		indices.push_back(vertexCount + 3);
		vertexCount += 4;
	}

	static inline bool CullsFace(BlockId block)
	{
		if (block == AIR) return false;

		const auto registry = BlockRegistry::GetInstance();
		return !registry->GetBlock(block)->transparent;
	}

	std::unique_ptr<WillowVox::Mesh> BuildChunkMesh(const ChunkSnapshot& snapshot, bool smoothLighting)
	{
		static auto* blockRegistry = BlockRegistry::GetInstance();
		static auto* renderer = WillowVox::Renderer::GetInstance();

		std::vector<ChunkVertex> vertices;
		std::vector<uint32_t> indices;

		uint32_t vertexCount = 0;
		for (int z = 0; z < CHUNK_SIZE; z++)
		{
			for (int x = 0; x < CHUNK_SIZE; x++)
			{
				for (int y = 0; y < CHUNK_SIZE; y++)
				{
					int index = PadIndex(x, y, z);
					BlockId block = snapshot.blocks[index];
					if (block <= AIR) // Skip if air (or invalid)
						continue;

					auto* blockDisplay = blockRegistry->GetBlockDisplay(block);

					if (blockDisplay->foliageRendering)
					{
						int rawLight = snapshot.light[PadIndex(x, y, z)];
						int blockLight = rawLight & 0xF;
						int skyLight = (rawLight >> 4) & 0xF;
						int lightData = (skyLight * 17) << 8 | (blockLight * 17);

						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y,        z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y,        z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y + 1.0f, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMaxY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y + 1.0f, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMaxY), lightData);
						AddIndices(indices, vertexCount);

						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y + 1.0f, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMaxY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y + 1.0f, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMaxY), lightData);
						AddIndices(indices, vertexCount);

						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y + 1.0f, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMaxY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y + 1.0f, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMaxY), lightData);
						AddIndices(indices, vertexCount);
						
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMinY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.8535533906f, y + 1.0f, z + 0.1464466094f), glm::vec2(blockDisplay->sideTextureMinX, blockDisplay->sideTextureMaxY), lightData);
						vertices.emplace_back(glm::vec3(x + 0.1464466094f, y + 1.0f, z + 0.8535533906f), glm::vec2(blockDisplay->sideTextureMaxX, blockDisplay->sideTextureMaxY), lightData);
						AddIndices(indices, vertexCount);
					}
					else
					{
						// Create cube
						for (int d = 0; d < 6; d++)
						{
							glm::ivec3 n = glm::ivec3(x, y, z) + dirOffsets[d];
							if (!CullsFace(snapshot.blocks[PadIndex(n.x, n.y, n.z)]))
							{
								int blockLight;
								int skyLight;
								uint32_t lightData;
								if (!smoothLighting)
								{
									int rawLight = snapshot.light[PadIndex(n.x, n.y, n.z)];
									blockLight = rawLight & 0xF;
									skyLight = (rawLight >> 4) & 0xF;
									lightData = (skyLight * 17) << 8 | (blockLight * 17);
								}

								for (int v = 0; v < 4; v++)
								{
									if (smoothLighting)
									{
										blockLight = 0;
										int blockLightCorner = 0;
										skyLight = 0;
										int skyLightCorner = 0;
										int airBlocks = 0;
										int aoEdges = 0;
										bool aoCorner = false;
										for (int vx = 0; vx <= 1; vx++)
										{
											for (int vy = 0; vy <= 1; vy++)
											{
												int vxi = v % 2 + vx;
												int vyi = v / 2 + vy;
												int vi = vxi + (vyi * 3);
												if (!CullsFace(snapshot.blocks[index + SmoothLightingIndexOffsets[d][vi]]))
												{
													airBlocks++;
													if (vxi == 1 || vyi == 1)
													{
														blockLight += snapshot.light[index + SmoothLightingIndexOffsets[d][vi]] & 0xF;
														skyLight += (snapshot.light[index + SmoothLightingIndexOffsets[d][vi]] >> 4) & 0xF;
													}
													else
													{
														blockLightCorner += snapshot.light[index + SmoothLightingIndexOffsets[d][vi]] & 0xF;
														skyLightCorner += (snapshot.light[index + SmoothLightingIndexOffsets[d][vi]] >> 4) & 0xF;
													}
												}
												else if (vxi == 1 || vyi == 1) aoEdges++;
												else aoCorner = true;
											}
										}

										if (aoEdges < 2)
										{
											blockLight += blockLightCorner;
											skyLight += skyLightCorner;
										}
										blockLight = (int)(blockLight / (float)airBlocks * 17.0f);
										skyLight = (int)(skyLight / (float)airBlocks * 17.0f);
										int aoValue = aoEdges > 0 ? aoEdges : (aoCorner ? 1 : 0);
										lightData = aoValue << 16 | skyLight << 8 | blockLight;
									}

									vertices.emplace_back(glm::vec3(x, y, z) + VertexPositions[d][v], GetTexturePos(blockDisplay, Direction(d), v), lightData);
								}

								AddIndices(indices, vertexCount);
							}
						}
					}
				}
			}
		}

		return std::unique_ptr<WillowVox::Mesh>(renderer->CreateMesh(vertices.data(), sizeof(ChunkVertex) * vertices.size(), indices.data(), indices.size()));
	}
}