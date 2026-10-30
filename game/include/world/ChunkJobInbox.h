#pragma once

#include <variant>

namespace ScuffedMinecraft
{
	struct ChunkDataResult
	{
		ChunkDataResult(const glm::ivec3& id, uint64_t token, std::unique_ptr<ChunkData> data)
			: id(id), token(token), data(std::move(data)) {}

		glm::ivec3 id;
		uint64_t token;
		std::unique_ptr<ChunkData> data;
	};
	struct MeshResult
	{
		MeshResult(const glm::ivec3& id, uint64_t token, uint32_t dataVersion, std::unique_ptr<WillowVox::Mesh> mesh)
			: id(id), token(token), dataVersion(dataVersion), mesh(std::move(mesh)) {}

		glm::ivec3 id;
		uint64_t token;
		uint32_t dataVersion;
		std::unique_ptr<WillowVox::Mesh> mesh;
	};
	using Result = std::variant<ChunkDataResult, MeshResult>;

	class ChunkJobInbox
	{
	public:
		void Push(Result r)
		{
			std::lock_guard lock(_mutex);
			_items.push_back(std::move(r));
		}

		std::vector<Result> TakeAll()
		{
			std::lock_guard lock(_mutex);
			std::vector<Result> out;
			out.swap(_items);
			return out;
		}

	private:
		std::mutex _mutex;
		std::vector<Result> _items;
	};
}
