#include "EngineThread.h"

using namespace GlareEngine;

EngineThread::EngineThread()
{
	mStopping = false;
	mThreadCountSupport = std::thread::hardware_concurrency();
	mThreadCount = mThreadCountSupport - 1; // exclude the main (this) thread
	mThreadNames[std::this_thread::get_id()] = "Main Thread";

	for (uint32_t i = 0; i < mThreadCount; i++)
	{
		mThreads.emplace_back(std::thread(&EngineThread::ThreadLoop, this));
		mThreadNames[mThreads.back().get_id()] = "WorkerThread_" + std::to_string(i);
	}

	EngineLog::AddLog(L"%d threads have been created", mThreadCount);
}

EngineThread::~EngineThread()
{
	Flush(true);

	// Put unique lock on task mutex.
	std::unique_lock<std::mutex> lock(mMutexTasks);

	// Set termination flag to true.
	mStopping = true;

	// Unlock the mutex
	lock.unlock();

	// Wake up all threads.
	mConditionVar.notify_all();

	// Join all threads.
	for (auto& thread : mThreads)
	{
		thread.join();
	}

	// Empty worker threads.
	mThreads.clear();
}

uint32_t EngineThread::GetThreadsAvailable() const
{
	uint32_t available_threads = mThreadCount;

	for (const auto& task : mTasks)
	{
		available_threads -= task->IsExecuting() ? 1 : 0;
	}

	return available_threads;
}

void EngineThread::Flush(bool remove_queued /*= false*/)
{
	// Clear any queued tasks
	if (remove_queued)
	{
		mTasks.clear();
	}

	// If so, wait for them
	while (AreTasksRunning())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

void EngineThread::ThreadLoop()
{
	std::shared_ptr<EngineTask> task;
	while (true)
	{
		// Lock tasks mutex
		std::unique_lock<std::mutex> lock(mMutexTasks);

		// Check condition on notification
		mConditionVar.wait(lock, [this] { return !mTasks.empty() || mStopping; });

		// If m_stopping is true, it's time to shut everything down
		if (mStopping && mTasks.empty())
			return;

		// Get next task in the queue.
		task = mTasks.front();

		// Execute the task.
		task->Execute();

		// Remove it from the queue.
		if (mTasks.size() > 0)
			mTasks.pop_front();

		// Unlock the mutex
		lock.unlock();
	}
}