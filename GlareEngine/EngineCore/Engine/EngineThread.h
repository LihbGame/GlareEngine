#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <functional>
#include "EngineLog.h"

namespace GlareEngine
{
    class EngineTask
    {
    public:
        typedef std::function<void()> FunctionType;

        EngineTask(FunctionType&& function) { m_function = std::forward<FunctionType>(function); }
        void Execute() { mIsExecuting = true; m_function(); mIsExecuting = false; }
        bool IsExecuting()       const { return mIsExecuting; }

    private:
        bool mIsExecuting = false;
        FunctionType m_function;
    };


    class EngineThread
    {
    private:
        EngineThread();
    public:
        static EngineThread& Get();


        ~EngineThread();

        // Add a task
        template <typename Function>
        void AddTask(Function&& function)
        {
            if (mThreads.empty())
            {
                EngineLog::AddLog(L"No available threads, function will execute in the same thread");
                function();
                return;
            }

            // Lock tasks mutex
            std::unique_lock<std::mutex> lock(mMutexTasksforAdd);

            // Save the task
            mTasks.push_back(std::make_shared<EngineTask>(std::bind(std::forward<Function>(function))));

            // Unlock the mutex
            lock.unlock();

            // Wake up a thread
            mConditionVar.notify_one();
        }

        // Adds a task which is a loop and executes chunks of it in parallel
        template <typename Function>
        void AddTaskLoop(Function&& function, uint32_t range)
        {
            uint32_t available_threads = GetThreadsAvailable();
            std::vector<bool> tasks_done = std::vector<bool>(available_threads, false);
            const uint32_t task_count = available_threads + 1; // plus one for the current thread

            uint32_t start = 0;
            uint32_t end = 0;
            for (uint32_t i = 0; i < available_threads; i++)
            {
                start = (range / task_count) * i;
                end = start + (range / task_count);

                // Kick off task
                AddTask([&function, &tasks_done, i, start, end] { function(start, end); tasks_done[i] = true; });
            }

            // Do last task in the current thread
            function(end, range);

            // Wait till the threads are done
            uint32_t tasks = 0;
            while (tasks != tasks_done.size())
            {
                tasks = 0;
                for (const bool job_done : tasks_done)
                {
                    tasks += job_done ? 1 : 0;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }

        // Get the number of threads used
        uint32_t GetThreadCount()        const { return mThreadCount; }

        // Get the maximum number of threads the hardware supports
        uint32_t GetThreadCountSupport() const { return mThreadCountSupport; }

        // Get the number of threads which are not doing any work
        uint32_t GetThreadsAvailable()   const;

        // Returns true if at least one task is running
        bool AreTasksRunning()           const { return GetThreadsAvailable() != GetThreadCount(); }

        // Waits for all executing (and queued if requested) tasks to finish
        void Flush(bool remove_queued = false);

    private:
        // This function is invoked by the threads
        void ThreadLoop();

        uint32_t mThreadCount = 0;
        uint32_t mThreadCountSupport = 0;
        std::vector<std::thread> mThreads;
        std::deque<std::shared_ptr<EngineTask>> mTasks;
        std::mutex mMutexTasks;
        std::mutex mMutexTasksforAdd;
        std::condition_variable mConditionVar;
        std::unordered_map<std::thread::id, std::string> mThreadNames;
        bool mStopping;
    };
}
