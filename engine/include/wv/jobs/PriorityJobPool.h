#pragma once

#include <wv/pch.h>

namespace WillowVox
{
	class PriorityJobPool
	{
	public:
		PriorityJobPool(int threads);
		~PriorityJobPool();

		// Lower priority runs sooner
		void Submit(int priority, std::function<void()> job);

	private:
		void JobThread();

		struct Job
		{
			int priority;
			uint64_t seq;
			std::function<void()> fn;
		};
		struct Cmp
		{
			bool operator()(const Job& a, const Job& b) const
			{
				return a.priority != b.priority ? a.priority > b.priority : a.seq > b.seq;
			};
		};
		std::priority_queue<Job, std::vector<Job>, Cmp> _queue;
		std::mutex _mutex;
		std::condition_variable _conditionVariable;
		bool _shouldEnd = false;
		uint64_t _seq = 0;
		std::vector<std::thread> _threads;
	};
}