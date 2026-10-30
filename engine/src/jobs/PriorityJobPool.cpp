#include <wv/jobs/PriorityJobPool.h>

namespace WillowVox
{
	PriorityJobPool::PriorityJobPool(int threads)
	{
		for (int i = 0; i < threads; i++)
			_threads.emplace_back(&PriorityJobPool::JobThread, this);
	}

	PriorityJobPool::~PriorityJobPool()
	{
		{
			std::unique_lock lock(_mutex);
			_shouldEnd = true;
		}
		_conditionVariable.notify_all();
		for (auto& t : _threads)
			t.join();
	}

	void PriorityJobPool::Submit(int priority, std::function<void()> job)
	{
		{
			std::unique_lock lock(_mutex);
			_queue.emplace(priority, _seq++, std::move(job));
		}
		_conditionVariable.notify_one();
	}

	void PriorityJobPool::JobThread()
	{
		while (!_shouldEnd)
		{
			std::function<void()> fn;
			{
				std::unique_lock lock(_mutex);
				_conditionVariable.wait(lock, [&] {
					return _shouldEnd || !_queue.empty();
					});

				if (_shouldEnd && _queue.empty()) return;
				fn = std::move(const_cast<Job&>(_queue.top()).fn);
				_queue.pop();
			}

			fn();
		}
	}
}